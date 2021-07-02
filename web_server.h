#ifndef WEBSERVER_WEB_SERVER_H_
#define WEBSERVER_WEB_SERVER_H_

#include <string>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "./timer/list_timer.h"
#include "./http/http_conn.h"
#include "./CGImysql/sql_connection_pool.h"
#include "./thread_pool/thread_pool.h"
namespace TinyWebServer
{

const int MAX_FD = 65536;
const int MAX_EVENT_NUMBER = 10000;
const int TIMESLOT=5;


class WebServer
{
public:
	WebServer();
	~WebServer();
	void Init(int port,std::string user,std::string password,std::string database_name,
		  int log_write,int opt_linger,int trige_mode,int sql_num,int thread_num,
		  int close_log,int actor_model);

	void Start();
private:
	void StartThreadPool();
	void StartSqlPool();
	void StartLogWrite();
	void ParseTrigeMode();
	void EventListen();
	void EventLoop();

public:
	void Timer(int connfd,struct sockaddr_in client_address);
	void AdjustTimer(TimerNode* timer);
	void DeleteTimer(TimerNode* timer,int sockfd);
	bool AcceptHandler();
	bool SignalHandler(bool &timeout,bool& stop_server);
	void ReadHandler(int sockfd);
	void WriteHanler(int sockfd);
private:
	int port_;
	char* root_;
	int log_write_;
	int close_log_;
	int actor_model_;

	int user_pipefd_[2];
	int epollfd_;
	HttpConn *users_;

	// database
	ConnectionPool* conn_pool_;
	std::string user_;
	std::string password_;
	std::string database_name_;
	int sql_num_;

	// thread 
	ThreadPool<HttpConn>* thread_pool_;
	int thread_num_;

	//epoll 
	epoll_event events[MAX_EVENT_NUMBER];
	int listenfd_;
	int opt_linger_;
	int trige_mode_;
	int listenfd_trige_mode_;
	int connfd_trige_mode_;

	// timer
	ClientData* users_timer_;
	Utils utils_;
};
}

#endif