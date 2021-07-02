#include "http_conn.h"
#include "../log/log.h"
#include "../lock/locker.h"
#include <map>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <utility>
#include <errno.h>
namespace TinyWebServer
{
	const char* OK_200_TITLE = "OK";
	const char* ERROR_400_TITLE = "Bad Request";
	const char* ERROR_400_FORM = "Your request has bad syntax or is inherently impossible to staisfy.\n";
	const char* ERROR_403_TITLE = "Forbidden";
	const char* ERROR_403_FORM = "You do not have permission to get file from this server.\n";
	const char* ERROR_404_TITLE = "Not Found";
	const char* ERROR_404_FORM = "The requested file was not found on this server.\n";
	const char* ERROR_500_TITLE = "Internal Error";
	const char* ERROR_500_FORM ="There was an unusual problem serving the request file.\n";

	
	Locker g_lock;
	std::map<std::string,std::string> g_users;
	int HttpConn::user_count_ = 0;
	int HttpConn::epollfd_ = -1;
	// 4个全局函数
	// 将文件描述符设置为非阻塞
	int SetNonBlocking(int fd)
	{
		int old_option = fcntl(fd,F_GETFL);
		int new_option = old_option | O_NONBLOCK;
		fcntl(fd,F_SETFL,new_option);
		return old_option;
	}
	// 向内核事件表注册读事件，ET模式，选择开启one_shot
	void AddFd(int epollfd,int fd,bool one_shot,int trig_mode)
	{
		epoll_event event;
		event.data.fd = fd;
		if(trig_mode == 1)
			event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
		else
			event.events = EPOLLIN | EPOLLRDHUP;
		if(one_shot)
			event.events |= EPOLLONESHOT;
		epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
		SetNonBlocking(fd);
	}
	// 向内核事件表删除描述符
	void RemoveFd(int epollfd,int fd)
	{
		epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
		close(fd);		
	}
	// 重置事件为one_shot
	void ModFd(int epollfd,int fd,int ev,int trig_mode)
	{
		epoll_event event;
		event.data.fd = fd;
		if(trig_mode == 1)
			event.events = ev | EPOLLONESHOT | EPOLLET | EPOLLRDHUP;
		else
			event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
		epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
	}


	void HttpConn::Init()
	{
		time_flag_ = 0;
		improv_ = 0;
		mysql_ = NULL;
		bytes_to_send_ = 0;
		bytes_have_send_ = 0;
		state_ = 0;

		read_index_ = 0;
		checked_index_ = 0;
		start_line_ = 0;
		write_index_ = 0;
		check_state_ = CHECK_STATE_REQUESTLINE;
		method_ = GET;
		url_ = NULL;
		version_ = NULL;
		host_ = NULL;
		content_length_ = 0;
		linger_ = false;
		file_address_ = NULL;
		cgi_ = 0;

		memset(read_buf_,0,READ_BUFFER_SIZE);
		memset(write_buf_,0,WRITE_BUFFER_SIZE);
		memset(real_file_,0,FILENAME_LEN);
	}

	/*********************************/
	// 解析请求行
	HttpConn::HTTP_CODE HttpConn::ParseRequestLine(char* text)
	{
		url_ = strpbrk(text," \t");
		if(!url_)
			return BAD_REQUEST;
		*url_++ = '\0';
		char* method = text;
		if(strcasecmp(method,"GET")==0)
			method_ = GET;
		else if(strcasecmp(method,"POST")==0)
		{
			method_ = POST;
			cgi_ = 1;
		}
		else
			return BAD_REQUEST;

		url_ += strspn(url_, " \t");
		version_ = strpbrk(url_," \t");
		if(!version_)
			return BAD_REQUEST;

		*version_++ = '\0';
		version_+=strspn(version_," \t");
		if(strcasecmp(version_,"HTTP/1.1")!=0)
			return BAD_REQUEST;
		if(strncasecmp(url_,"http://",7)==0)
		{
			url_+=7;
			url_ = strchr(url_,'/');
		}
		if(strncasecmp(url_,"https://",8)==0)
		{
			url_+=8;
			url_ = strchr(url_,'/');
		}
		if(!url_ || url_[0]!='/')
			return BAD_REQUEST;
		// bug to fix 覆盖掉version_指针指向的位置
		if(strlen(url_)==1)
			strcat(url_,"judge.html");
		check_state_ = CHECK_STATE_HEADER;
		return NO_REQUEST;
	}
	// 解析请求头部
	HttpConn::HTTP_CODE HttpConn::ParseHeaders(char *text)
	{
		if(text[0]='\0')
		{
			if(content_length_!=0)
			{
				check_state_ = CHECK_STATE_CONTENT;
				return NO_REQUEST;
			}
			// 解析完毕一个完整的GET请求
			return GET_REQUEST;
		}
		else if(strncasecmp(text,"Connection:",11)==0)
		{
			text+=11;
			text+=strspn(text," \t");
			if(strcasecmp(text,"keep-alive")==0)
				linger_ = true;
		}
		else if(strncasecmp(text,"Content-length:",15)==0)
		{
			text+=15;
			text+=strspn(text," \t");
			content_length_ = atol(text);
		}
		else if(strncasecmp(text,"Host:",5)==0)
		{
			text+=5;
			text+=strspn(text," \t");
			host_ = text;
		}
		else
		{
			LOG_INFO("oop! unknow header: %s",text);
		}
		return NO_REQUEST;
	}
	// 解析请求体
	HttpConn::HTTP_CODE HttpConn::ParseContent(char *text)
	{	
		if(read_index_>=(content_length_+checked_index_))
		{
			text[content_length_]='\0';
			request_body_string_ = text;
			return GET_REQUEST;
		}
		return NO_REQUEST;
	}

	// 解析一行
	// LINE_OK解析一行成功，LINE_OPEN继续读取数据解析，LINE_BAD请求数据有错
	HttpConn::LINE_STATUS HttpConn::ParseLine()
	{
		char temp;
		for(;checked_index_<read_index_;++checked_index_)
		{
			temp = read_buf_[checked_index_];
			if(temp == '\r')
			{
				if(checked_index_+1==read_index_)
					return LINE_OPEN;
				else if(read_buf_[checked_index_+1]=='\n')
				{
					read_buf_[checked_index_++]='\0';
					read_buf_[checked_index_++]='\0';
					return LINE_OK;
				}
				return LINE_BAD;
			}
			else if(temp=='\n')
			{
				if(checked_index_>1 && read_buf_[checked_index_-1]=='\r')
				{
					read_buf_[checked_index_-1]='\0';
					read_buf_[checked_index_++]='\0';
					return LINE_OK;
				}
				return LINE_BAD;
			}
		}
		return LINE_OPEN;
		
	}

	// 处理读取数据
	HttpConn::HTTP_CODE HttpConn::ProcessRead()
	{
		LINE_STATUS line_status = LINE_OK;
		HTTP_CODE ret  = NO_REQUEST;
		char* text = NULL;
		while ((check_state_ == CHECK_STATE_CONTENT && line_status == LINE_OK) ||((line_status=ParseLine())==LINE_OK))
		{
			text = GetLine();
			start_line_ = checked_index_;
			LOG_INFO("%s",text);
			switch(check_state_)
			{
				case CHECK_STATE_REQUESTLINE:
				{
					ret = ParseRequestLine(text);
					if(ret == BAD_REQUEST)
						return BAD_REQUEST;
					break;
				}
				case CHECK_STATE_HEADER:
				{
					ret = ParseHeaders(text);
					if(ret == BAD_REQUEST)
						return BAD_REQUEST;
					else if(ret == GET_REQUEST)
					{
						return DoRequest();
					}
					break;
				}
				case CHECK_STATE_CONTENT:
				{
					ret = ParseContent(text);
					if(ret == GET_REQUEST)
					{
						return DoRequest();
					}
					line_status = LINE_OPEN;
					break;
				}
				default:
					return INTERNAL_ERROR;
			}
		}
		return NO_REQUEST;
	}

	// 处理请求
	HttpConn::HTTP_CODE HttpConn::DoRequest()
	{
		strcpy(real_file_,doc_root_);
		int len = strlen(doc_root_);
		const char* p =strrchr(url_,'/');

		// 处理cgi
		if(cgi_==1 &&(*(p+1)=='2'||*(p+1)=='3'))
		{
			char flag = url_[1];
			char* url_real = (char*)malloc(sizeof(char)*200);
			strcpy(url_real,"/");
			strcat(url_real,url_+2);
			strncpy(real_file_+len,url_real,FILENAME_LEN - len - 1);
			free(url_real);

			// user=123&password=123
			char name[100],password[100];
			int i;
			for(i=5;request_body_string_[i]!='&';++i)
			{
				name[i-5]=request_body_string_[i];
			}
			name[i-5]='\0';
			int j=0;
			for(i=i+10;request_body_string_[i]!='\0';++i,++j)
			{
				password[j]=request_body_string_[i];
			}
			password[j]='\0';

			// 注册
			if(*(p+1)=='3')
			{
				char* sql_insert = (char*)malloc(sizeof(char)*200);
				strcpy(sql_insert,"INSERT INTO user(username,passwd) VALUES(");
				strcat(sql_insert,"'");
				strcat(sql_insert,name);
				strcat(sql_insert,"', '");
				strcat(sql_insert,password);
				strcat(sql_insert,"')");
				if(g_users.find(name)==g_users.end())
				{
					g_lock.Lock();
					int res = mysql_query(mysql_,sql_insert);
					g_users.insert(std::pair<std::string,std::string>(name,password));
					g_lock.UnLock();

					if(!res)
						strcpy(url_,"/log.html");
					else
						strcpy(url_,"/registerError.html");
				}
				else
				{
					strcpy(url_,"/registerError.html");
				}
			}
			else if(*(p+1)=='2')
			{
				if(g_users.find(name)!=g_users.end() && g_users[name]==password)
					strcpy(url_,"/welcome.html");
				else
					strcpy(url_,"/logError.html");
			}
		}
		if(*(p+1)=='0')
		{
			char* url_real = (char*)malloc(sizeof(char)*200);
			strcpy(url_real,"/register.html");
			strncpy(real_file_+len,url_real,strlen(url_real));
			free(url_real);
		}
		else if(*(p+1)=='1')
		{
			char* url_real = (char*)malloc(sizeof(char)*200);
			strcpy(url_real,"/log.html");
			strncpy(real_file_+len,url_real,strlen(url_real));
			free(url_real);
		}
		else if(*(p+1)=='5')
		{
			char* url_real = (char*)malloc(sizeof(char)*200);
			strcpy(url_real,"/picture.html");
			strncpy(real_file_+len,url_real,strlen(url_real));
			free(url_real);
		}
		else if(*(p+1)=='6')
		{
			char* url_real = (char*)malloc(sizeof(char)*200);
			strcpy(url_real,"/video.html");
			strncpy(real_file_+len,url_real,strlen(url_real));
			free(url_real);
		}
		else if(*(p+1)=='7')
		{
			char* url_real = (char*)malloc(sizeof(char)*200);
			strcpy(url_real,"/fans.html");
			strncpy(real_file_+len,url_real,strlen(url_real));
			free(url_real);
		}
		else
		{
			strncpy(real_file_ + len, url_, FILENAME_LEN - len - 1);
		}

		if(stat(real_file_,&file_stat_)<0)
			return NO_RESOURCE;
		if(!(file_stat_.st_mode & S_IROTH))
			return FORBIDDEN_REQUEST;
		if(S_ISDIR(file_stat_.st_mode))
			return BAD_REQUEST;
		
		int fd = open(real_file_,O_RDONLY);
		file_address_ = (char*)mmap(0,file_stat_.st_size,PROT_READ,MAP_PRIVATE,fd,0);
		close(fd);
		return FILE_REQUEST;

	}

	/***********************************/
	void HttpConn::UnMap()
	{
		if(file_address_)
		{
			munmap(file_address_,file_stat_.st_size);
			file_address_ = NULL;
		}
	}
	bool HttpConn::AddResopnse(const char* format,...)
	{
		if(write_index_>=WRITE_BUFFER_SIZE)
			return false;
		va_list arg_list;
		va_start(arg_list,format);
		int len = vsnprintf(write_buf_ + write_index_, WRITE_BUFFER_SIZE - 1 - write_index_, format, arg_list);
		if(len>=(WRITE_BUFFER_SIZE-1-write_index_))
		{
			va_end(arg_list);
			return false;
		}

		write_index_+=len;
		va_end(arg_list);
		LOG_INFO("request:%s",write_buf_);
		return true;
	}
	bool HttpConn::AddContent(const char* content)
	{
		AddResopnse("%s",content);
	}
	bool HttpConn::AddStatusLine(int status,const char* title)
	{
		return AddResopnse("%s %d %s\r\n","HTTP/1.1",status,title);
	}
	bool HttpConn::AddHeaders(int content_length)
	{
		return AddContentLength(content_length) && AddLinger()&&AddBlankLine();
	}
	bool HttpConn::AddContentType()
	{
		return AddResopnse("Content-Type:%s\r\n","text/html");
	}
	bool HttpConn::AddContentLength(int content_length)
	{
		return AddResopnse("Content-Length:%d\r\n",content_length);
	}
	bool HttpConn::AddLinger()
	{
		return AddResopnse("Connection:%s\r\n", (linger_ == true) ? "keep-live" : "close");
	}
	bool HttpConn::AddBlankLine()
	{
		return AddResopnse("%s","\r\n");
	}
	bool HttpConn::ProcessWrite(HTTP_CODE ret)
	{
		switch(ret)
		{
			case INTERNAL_ERROR:
			{
				AddStatusLine(500,ERROR_500_TITLE);
				AddHeaders(strlen(ERROR_500_FORM));
				if(!AddContent(ERROR_500_FORM))
					return false;
				break;
			}
			case BAD_REQUEST:
			{
				AddStatusLine(404,ERROR_404_TITLE);
				AddHeaders(strlen(ERROR_404_FORM));
				if(!AddContent(ERROR_404_FORM))
					return false;
				break;
			}
			case FORBIDDEN_REQUEST:
			{
				AddStatusLine(403,ERROR_403_TITLE);
				AddHeaders(strlen(ERROR_403_FORM));
				if(!AddContent(ERROR_403_FORM))
					return false;
				break;
			}
			case FILE_REQUEST:
			{
				AddStatusLine(200,OK_200_TITLE);
				if(file_stat_.st_size!=0)
				{
					AddHeaders(file_stat_.st_size);
					iv_[0].iov_base=write_buf_;
					iv_[0].iov_len = write_index_;
					iv_[1].iov_base = file_address_;
					iv_[1].iov_len = file_stat_.st_size;
					iv_count_ = 2;
					bytes_to_send_ = write_index_+file_stat_.st_size;
					return true;
				}
				else
				{
					const char* ok_string = "<html><body></body></html>";
					AddHeaders(strlen(ok_string));
					if(!AddContent(ok_string))
						return false;
				}
			}
			default:
				return false;
		}

		iv_[0].iov_base = write_buf_;
		iv_[0].iov_len = write_index_;
		iv_count_ = 1;
		bytes_to_send_ = write_index_;
		return true;
	}
	/************************/


	void HttpConn::InitMysqlResult(ConnectionPool* conn_pool)
	{
		MYSQL* mysql = NULL;
		ConnectionRAII mysql_conn(&mysql,conn_pool);
		if(mysql_query(mysql,"SELECT username,passwd FROM user"))
		{
			LOG_ERROR("SELECT error:%s\n",mysql_error(mysql));
		}
		MYSQL_RES* result = mysql_store_result(mysql);
		int num_fields = mysql_num_fields(result);
		MYSQL_FIELD* fields = mysql_fetch_fields(result);
		while(MYSQL_ROW row = mysql_fetch_row(result))
		{
			std::string name(row[0]);
			std::string passwd(row[1]);
			g_users[name] = passwd;
		}
	}
	void HttpConn::CloseConn(bool real_close)
	{
		if(real_close && (sockfd_!=-1))
		{
			printf("close %d\n",sockfd_);
			RemoveFd(epollfd_,sockfd_);
			sockfd_ = -1;
			user_count_--;
		}
	}

	bool HttpConn::ReadOnce()
	{
		if(read_index_>=READ_BUFFER_SIZE)
			return false;
		int bytes_read = 0;
		if(trig_mode_==0)
		{
			bytes_read = recv(sockfd_,read_buf_+read_index_,READ_BUFFER_SIZE-read_index_,0);
			if(bytes_read<=0)
			{
				return false;
			}
			read_index_+=bytes_read;
			return true;
		}
		else
		{
			while(true)
			{
				bytes_read = recv(sockfd_,read_buf_+read_index_,READ_BUFFER_SIZE-read_index_,0);
				if(bytes_read==-1)
				{
					if(errno == EAGAIN || errno == EWOULDBLOCK)
						break;
					return false;
				}
				else if(bytes_read == 0)
					return false;
				read_index_+=bytes_read;
			}
			return true;
		}
	}
	bool HttpConn::Write()
	{
		int temp = 0;
		if(bytes_to_send_ == 0)
		{
			ModFd(epollfd_,sockfd_,EPOLLIN,trig_mode_);
			Init();
			return true;
		}
		while(1)
		{
			temp = writev(sockfd_,iv_,iv_count_);
			if(temp<0)
			{
				if(errno == EAGAIN)
				{
					ModFd(epollfd_,sockfd_,EPOLLOUT,trig_mode_);
					return true;
				}
				UnMap();
				return false;
			}
			bytes_have_send_+=temp;
			bytes_to_send_-=temp;
			if(bytes_have_send_>=iv_[0].iov_len)
			{
				iv_[0].iov_len=0;
				iv_[1].iov_base=file_address_+(bytes_have_send_-write_index_);
				iv_[1].iov_len=bytes_to_send_;
			}
			else
			{
				iv_[0].iov_base=write_buf_+bytes_have_send_;
				iv_[0].iov_len = iv_[0].iov_len - bytes_have_send_;
			}
			if(bytes_to_send_<=0)
			{
				UnMap();
				ModFd(epollfd_,sockfd_,EPOLLIN,trig_mode_);
				if(linger_)
				{
					Init();
					return true;
				}
				else
					return false;
			}
		}
	}

	void HttpConn::Init(int sockfd,const sockaddr_in& addr,char*root,int trig_mode,int close_log,std::string user,std::string passwd,std::string sql_name)
	{
		sockfd_ = sockfd;
		address_ = addr;
		AddFd(epollfd_,sockfd,true,trig_mode);
		user_count_++;
		doc_root_ = root;
		trig_mode_ = trig_mode;
		close_log_ = close_log;

		strcpy(sql_user_,user.c_str());
		strcpy(sql_passwd_,passwd.c_str());
		strcpy(sql_name_,sql_name.c_str());
		Init();
	}

	void HttpConn::Process()
	{
		HTTP_CODE read_ret = ProcessRead();
		if(read_ret == NO_REQUEST)
		{
			ModFd(epollfd_,sockfd_,EPOLLIN,trig_mode_);
			return;
		}
		bool write_ret = ProcessWrite(read_ret);
		if(!write_ret)
		{
			CloseConn();
			return;
		}
		ModFd(epollfd_, sockfd_, EPOLLOUT, trig_mode_);
	}

}