#include "web_server.h"
#include "config.h"
int main(int argc,char** argv)
{
	std::string user = "root";
	std::string passwd = "123456";
	std::string database_name = "webserver";
	TinyWebServer::Config config;
	config.ParseArgs(argc,argv);

	TinyWebServer::WebServer server;
	server.Init(config.port_,user,passwd,database_name,config.log_write_,config.opt_linger_,config.trige_mode_,config.sql_num_,
		    config.thread_num_,config.close_log_,config.actor_model_);
	
	server.Start();
	return 0;
}