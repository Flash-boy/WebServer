#ifndef WEBSERVER_CONFIG_H_
#define WEBSERVER_CONFIG_H_

namespace TinyWebServer
{
class Config
{
public:
	Config();
	~Config(){};
	void ParseArgs(int argc,char** argv);
public:
	int port_;
	int log_write_;
	int trige_mode_;
	int listenfd_trige_mode_;
	int connfd_trige_mode_;
	int opt_linger_;
	int sql_num_;
	int thread_num_;
	int close_log_;
	int actor_model_;
};
}

#endif