#include "web_server.h"
#include <assert.h>
#include <sys/socket.h>
#include <signal.h>
#include <cstdlib>
#include <errno.h>
namespace TinyWebServer
{
	WebServer::WebServer()
	{
		users_ = new HttpConn[MAX_FD];
		char server_path[200];
		getcwd(server_path,200);
		char root[6]="/root";
		root_ = (char *)malloc(strlen(server_path) + strlen(root) + 1);
		strcpy(root_,server_path);
		strcat(root_,root_);
	
		users_timer_ = new ClientData[MAX_FD];
	}
	WebServer::~WebServer()
	{
		close(epollfd_);
		close(listenfd_);
		close(user_pipefd_[0]);
		close(user_pipefd_[1]);
		delete [] users_;
		delete [] users_timer_;
		delete thread_pool_;
		free(root_);
	}

	void WebServer::Init(int port,std::string user,std::string password,std::string database_name,
		  int log_write,int opt_linger,int trige_mode,int sql_num,int thread_num,
		  int close_log,int actor_model)
	{
		port_ = port;
		log_write_=log_write;
		close_log_=close_log;
		actor_model_ = actor_model;

		user_ = user;
		password_ = password;
		database_name_ = database_name;
		sql_num_ = sql_num;
		thread_num_ = thread_num;
		opt_linger_ = opt_linger;
		trige_mode_ = trige_mode;
	}
	void WebServer::Start()
	{
		StartLogWrite();
		StartSqlPool();
		StartThreadPool();
		ParseTrigeMode();
		EventListen();
		EventLoop();
	}
	void WebServer::StartLogWrite()
	{
		if(close_log_==0)
		{
			if(log_write_==1)
				Log::GetInstance()->Init("./ServerLog",close_log_,2000,800000,800);
			else
				Log::GetInstance()->Init("./ServerLog",close_log_,2000,800000,0);
		}
	}
	void WebServer::StartSqlPool()
	{
		conn_pool_ = ConnectionPool::GetInstance();
		conn_pool_->Init("localhost",user_,password_,database_name_,3306,sql_num_,close_log_);

		users_->InitMysqlResult(conn_pool_);
	}
	void WebServer::StartThreadPool()
	{
		thread_pool_ = new ThreadPool<HttpConn>(actor_model_,conn_pool_,thread_num_);
	}
	void WebServer::ParseTrigeMode()
	{
		// LT+LT
		// LT+ET
		// ET+LT
		// ET+ET
		if(trige_mode_<0 || trige_mode_>3)
		{
			// LT + LT;
			listenfd_trige_mode_=0;
			connfd_trige_mode_ =0;
			return;
		}
		listenfd_trige_mode_ = trige_mode_ & 0x02;
		connfd_trige_mode_ = trige_mode_ & 0x01;
	}
	
	void WebServer::EventListen()
	{
		listenfd_ = socket(PF_INET,SOCK_STREAM,0);
		assert(listenfd_>=0);
		if(opt_linger_==0)
		{
			struct linger tmp={0,1};
			setsockopt(listenfd_,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));
		}		
		else if(opt_linger_ ==1)
		{
			struct linger tmp={1,1};
			setsockopt(listenfd_,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));
		}

		int ret=0;
		struct sockaddr_in address;
		memset(&address,0,sizeof(address));
		address.sin_family=AF_INET;
		address.sin_addr.s_addr=htonl(INADDR_ANY);
		address.sin_port=htons(port_);

		int flag=1;
		setsockopt(listenfd_,SOL_SOCKET,SO_REUSEADDR,&flag,sizeof(flag));
		ret = bind(listenfd_,(struct sockaddr*)&address,sizeof(address));
		assert(ret>=0);
		ret = listen(listenfd_,5);
		assert(ret>=0);

		utils_.Init(TIMESLOT);
		epollfd_ = epoll_create(5);
		assert(epollfd_!=-1);

		utils_.AddFd(epollfd_,listenfd_,false,listenfd_trige_mode_);
		HttpConn::epollfd_ = epollfd_;

		ret = socketpair(PF_UNIX,SOCK_STREAM,0,user_pipefd_);
		assert(ret!=-1);
		utils_.SetNonBlocking(user_pipefd_[1]);
		utils_.AddFd(epollfd_,user_pipefd_[0],false,0);

		utils_.AddSig(SIGPIPE,SIG_IGN);
		utils_.AddSig(SIGALRM,utils_.SigHandler,false);
		utils_.AddSig(SIGTERM,utils_.SigHandler,false);

		alarm(TIMESLOT);
		Utils::user_pipefd = user_pipefd_;
		Utils::user_epollfd = epollfd_;
	}
	void WebServer::Timer(int connfd,struct sockaddr_in client_address)
	{
		users_[connfd].Init(connfd,client_address,root_,connfd_trige_mode_,close_log_,user_,password_,database_name_);

		users_timer_[connfd].address=client_address;
		users_timer_[connfd].sockfd=connfd;
		TimerNode* timer = new TimerNode();
		timer->user_data = &users_timer_[connfd];
		timer->cb_func = CbFun;
		time_t cur = time(NULL);
		timer->expire = cur + 3 * TIMESLOT;
		users_timer_[connfd].timer = timer;
		utils_.timer_list_.AddTimer(timer);
	}
	void WebServer::AdjustTimer(TimerNode* timer)
	{
		time_t cur = time(NULL);
		timer->expire = cur+3*TIMESLOT;
		utils_.timer_list_.AdjustTimer(timer);
		LOG_INFO("%s","adjust timer once");
	}

	void WebServer::DeleteTimer(TimerNode* timer,int sockfd)
	{
		timer->cb_func(&users_timer_[sockfd]);
		if(timer)
		{
			utils_.timer_list_.DeleteTimer(timer);
		}
		LOG_INFO("close fd %d",users_timer_[sockfd].sockfd);
	}

	bool WebServer::AcceptHandler()
	{
		struct sockaddr_in client_address;
		socklen_t client_addr_len = sizeof(client_address);
		if(listenfd_trige_mode_==0)
		{
			int connfd = accept(listenfd_,(struct sockaddr*)&client_address,&client_addr_len);
			if(connfd<0)
			{
				LOG_ERROR("%s:errno is:%d","accept error",errno);
				return false;
			}
			if(HttpConn::user_count_>=MAX_FD)
			{
				utils_.ShowError(connfd,"Internal server busy");
				LOG_ERROR("%s","Internal server busy");
				return false;
			}
			Timer(connfd,client_address);
		}
		else
		{
			while(1)
			{
				int connfd = accept(listenfd_,(struct sockaddr*)&client_address,&client_addr_len);
				if(connfd<0)
				{
					LOG_ERROR("%s:errno is:%d","accept error",errno);
					break;
				}
				if(HttpConn::user_count_>=MAX_FD)
				{
					utils_.ShowError(connfd,"Internal server busy");
					LOG_ERROR("%s","Internal server busy");
					break;
				}
				Timer(connfd,client_address);
			}
			return false;
		}
		return true;
	}
	bool WebServer::SignalHandler(bool &timeout,bool& stop_server)
	{
		int ret = 0;
		int sig;
		char signals[1024];
		ret = recv(user_pipefd_[0],signals,sizeof(signals),0);
		if(ret==-1)
			return false;
		else if(ret == 0)
			return false;
		else
		{
			for(int i=0;i<ret;i++)
			{
				switch(signals[i])
				{
					case SIGALRM:
					{
						timeout = true;
						break;
					}
					case SIGTERM:
					{
						stop_server=true;
						break;
					}
				}
			}
		}
		return true;
	}
	void WebServer::ReadHandler(int sockfd)
	{
		TimerNode* timer = users_timer_[sockfd].timer;
		//reactor
		if(actor_model_==1)
		{
			if(timer)
			{
				AdjustTimer(timer);
			}
			thread_pool_->Append(users_+sockfd,0);
			while(true)
			{
				if(users_[sockfd].improv_==1)
				{
					if(users_[sockfd].time_flag_==1)
					{
						DeleteTimer(timer,sockfd);
						users_[sockfd].time_flag_=0;
					}
					users_[sockfd].improv_=0;
					break;
				}
			}
		}
		else
		{
			if(users_[sockfd].ReadOnce())
			{
				LOG_INFO("deal with the client(%s)",inet_ntoa(users_[sockfd].GetAddress()->sin_addr));
				thread_pool_->AppendP(users_+sockfd);
				if(timer)
				{
					AdjustTimer(timer);
				}
			}
			else
			{
				DeleteTimer(timer,sockfd);
			}
		}
	}

	void WebServer::WriteHanler(int sockfd)
	{
		TimerNode* timer = users_timer_[sockfd].timer;
		//reactor
		if(actor_model_==1)
		{
			if(timer)
			{
				AdjustTimer(timer);
			}
			thread_pool_->Append(users_+sockfd,1);
			while(true)
			{
				if(users_[sockfd].improv_==1)
				{
					if(users_[sockfd].time_flag_==1)
					{
						DeleteTimer(timer,sockfd);
						users_[sockfd].time_flag_=0;
					}
					users_[sockfd].improv_=0;
					break;
				}
			}
		}
		else
		{
			if(users_[sockfd].Write())
			{
				LOG_INFO("deal with the client(%s)",inet_ntoa(users_[sockfd].GetAddress()->sin_addr));
				if(timer)
				{
					AdjustTimer(timer);
				}
			}
			else
			{
				DeleteTimer(timer,sockfd);
			}
		}
	}

	void WebServer::EventLoop()
	{
		bool timeout =false;
		bool stop_server=false;
		while(!stop_server)
		{
			int number = epoll_wait(epollfd_,events,MAX_EVENT_NUMBER,-1);
			if(number<0 && errno!=EINTR)
			{
				LOG_ERROR("%s","epoll failure");
				break;
			}
			for(int i=0;i<number;i++)
			{
				int sockfd = events[i].data.fd;
				if(sockfd==listenfd_)
				{
					bool flag = AcceptHandler();
					if(flag == false)
						continue;
				}
				else if(events[i].events & (EPOLLRDHUP|EPOLLHUP|EPOLLERR))
				{
					TimerNode* timer = users_timer_[sockfd].timer;
					DeleteTimer(timer,sockfd);
				}
				else if(sockfd == user_pipefd_[0] && (events[i].events&EPOLLIN))
				{
					bool flag = SignalHandler(timeout,stop_server);
					if(flag==false)
						LOG_ERROR("%s","deal signal failure");
				}
				else if(events[i].events& EPOLLIN)
				{
					ReadHandler(sockfd);
				}
				else if(events[i].events& EPOLLOUT)
				{
					WriteHanler(sockfd);
				}
			}
			if(timeout)
			{
				utils_.TImerHandler();
				LOG_INFO("%s","time tick");
				timeout = false;
			}
		}
	}






}