#ifndef WEBSERVER_TIMER_LIST_TIMER_H_
#define WEBSERVER_TIMER_LIST_TIMER_H_

#include "../log/log.h"
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <string.h>

namespace TinyWebServer
{

struct TimerNode;
struct ClientData
{
	ClientData()
	{
		memset(&address,'\0',sizeof(address));
		sockfd = -1;
		timer = nullptr;
	}
	sockaddr_in address;
	int sockfd;
	TimerNode* timer;
};

struct TimerNode
{
	TimerNode() : prev(NULL), next(NULL) {}
	time_t expire;
	void(* cb_func)(ClientData*);
	ClientData* user_data;
	TimerNode* prev;
	TimerNode* next;
};
// 按时间顺序的定时器列表
class TimerList
{
public:
	TimerList();
	~TimerList();
	void AddTimer(TimerNode* timer);
	void AdjustTimer(TimerNode* timer);
	void DeleteTimer(TimerNode* timer);
	void Tick(); // 执行定时器任务
private:
	void AddTimerAux(TimerNode* timer,TimerNode* begin);
	TimerNode* head_;
	TimerNode* tail_;
};

class Utils
{
public:
	Utils(){}
	~Utils(){}
	void Init(int timeslot);
	// 设置文件描述符为非阻塞
	int SetNonBlocking(int fd);
	// 向内核事件表注册读事件，选择开启oneshot
	void AddFd(int epollfd,int fd,bool one_shot,int trige_mode);
	// 信号处理函数
	static void SigHandler(int sig);
	// 设置信号函数
	void AddSig(int sig,void(handler)(int),bool restart = true);
	// 定时处理任务，重新定时触发SIGALRM信号
	void TImerHandler();
	void ShowError(int connfd,const char* info);
public:
	static int* user_pipefd;
	static int user_epollfd;
	TimerList timer_list_;
	int timeslot_;
};

void CbFun(ClientData* user_data);


}

#endif