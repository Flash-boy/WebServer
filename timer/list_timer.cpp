#include "list_timer.h"
#include "../http/http_conn.h"
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>

namespace TinyWebServer
{
	TimerList::TimerList()
	{
		head_=nullptr;
		tail_=nullptr;
	}
	TimerList::~TimerList()
	{
		TimerNode* tmp=head_;
		while(tmp)
		{
			head_=tmp->next;
			delete tmp;
			tmp=head_;
		}
	}
	void TimerList::AddTimer(TimerNode* timer)
	{
		if(timer==nullptr)
			return;
		if(head_==nullptr)
		{
			head_=tail_=timer;
			return;
		}
		if(timer->expire<head_->expire)
		{
			timer->next=head_;
			head_->prev=timer;
			head_=timer;
			return;
		}
		AddTimerAux(timer,head_);
	}
	void TimerList::AddTimerAux(TimerNode* timer,TimerNode* begin)
	{
		TimerNode* prev = begin;
		TimerNode* tmp= prev->next;
		while(timer->expire<tmp->expire)
		{
			if(timer->expire<tmp->expire)
			{
				prev->next = timer;
				timer->next = tmp;
				tmp->prev=timer;
				timer->prev=prev;
				break;
			}
			prev=tmp;
			tmp=tmp->next;
		}
		if(!tmp)
		{
			prev->next=timer;
			timer->prev=prev;
			timer->next=nullptr;
			tail_=timer;
		}
	}

	// 将某一个定时器的时钟延后
	void TimerList::AdjustTimer(TimerNode* timer)
	{
		if(timer==nullptr)
			return;
		TimerNode* tmp = timer->next;
		if(!tmp || timer->expire<tmp->expire)
			return;
		if(timer == head_)
		{
			head_=head_->next;
			head_->prev=nullptr;
			timer->next = nullptr;
			AddTimerAux(timer,head_);
		}
		else
		{
			timer->prev->next= timer->next;
			timer->next->prev = timer->prev;
			AddTimerAux(timer,timer->next);
		}
	}
	void TimerList::DeleteTimer(TimerNode* timer)
	{
		if(timer==nullptr)
			return ;
		if(timer==head_ && timer==tail_)
		{
			delete timer;
			head_=tail_=nullptr;
			return;
		}
		if(timer==head_)
		{
			head_=head_->next;
			head_->prev=nullptr;
			delete timer;
			return;
		}
		if(timer==tail_)
		{
			tail_=tail_->prev;
			tail_->next=nullptr;
			delete timer;
			return;
		}

		timer->prev->next= timer->next;
		timer->next->prev = timer->prev;
		delete timer;
	}
	// 执行定时器任务
	void TimerList::Tick()
	{
		if(!head_)
			return ;
		time_t cur = time(NULL);
		TimerNode* tmp=head_;
		while(tmp)
		{
			if(cur<tmp->expire)
				break;
			tmp->cb_func(tmp->user_data);
			head_=tmp->next;
			if(head_)
			{
				head_->prev=nullptr;
			}
			delete tmp;
			tmp=head_;
		}
	}

	// Utiles类实现
	int Utils::SetNonBlocking(int fd)
	{
		int old_option = fcntl(fd,F_GETFL);
		int new_option = old_option|O_NONBLOCK;
		fcntl(fd,F_SETFL,new_option);
		return old_option;
	}
	void Utils::Init(int timeslot)
	{
		timeslot_ = timeslot;
	}


	void Utils::AddFd(int epollfd,int fd,bool one_shot,int trige_mode)
	{
		epoll_event event;
		event.data.fd=fd;
		if(trige_mode==1)
		{
			event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
		}
		else
		{
			event.events = EPOLLIN | EPOLLRDHUP;
		}
		if(one_shot)
			event.events |= EPOLLONESHOT;
		epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
		SetNonBlocking(fd);
	}
	// bug to fix,字节序的问题
	void Utils::SigHandler(int sig)
	{
		int save_errno= errno;
		int msg = sig;
		send(user_pipefd[1],(char*)&msg,1,0);
		errno = save_errno;
	}
	void Utils::AddSig(int sig,void(handler)(int),bool restart)
	{
		struct sigaction sa;
		memset(&sa,0,sizeof(sa));
		sa.sa_handler = handler;
		if(restart)
			sa.sa_flags |= SA_RESTART;
		sigfillset(&sa.sa_mask);
		assert(sigaction(sig, &sa, NULL) != -1);
	}

	void Utils::TImerHandler()
	{
		timer_list_.Tick();
		alarm(timeslot_);
	}
	void Utils::ShowError(int connfd,const char* info)
	{
		send(connfd,info,strlen(info),0);
		close(connfd);
	}
	int * Utils::user_pipefd = nullptr;
	int Utils::user_epollfd = 0;
	void  CbFun(ClientData* user_data)
	{
		epoll_ctl(Utils::user_epollfd,EPOLL_CTL_DEL,user_data->sockfd,0);
		assert(user_data);
		close(user_data->sockfd);
		HttpConn::user_count_--;
	}
}