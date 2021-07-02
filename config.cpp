#include "config.h"
#include <unistd.h>
#include <stdlib.h>
namespace TinyWebServer
{
	Config::Config()
	{
		port_=9006;
		log_write_=0;
		trige_mode_=0;
		listenfd_trige_mode_=0;
		connfd_trige_mode_=0;
		opt_linger_=0;
		sql_num_=8;
		thread_num_=8;
		close_log_=0;
		actor_model_=0;
	}
	void Config::ParseArgs(int argc,char** argv)
	{
		int opt;
		const char* str="p:l:m:o:s:t:c:a:";
		while((opt=getopt(argc,argv,str))!=-1)
		{
			switch(opt)
			{
				case 'p':
				{
					port_=atoi(optarg);
					break;
				}
				case 'l':
				{
					log_write_ = atoi(optarg);
					break;
				}
				case 'm':
				{
					trige_mode_ = atoi(optarg);
					break;
				}
				case 'o':
				{
					opt_linger_ = atoi(optarg);
					break;
				}
				case 's':
				{
					sql_num_ = atoi(optarg);
					break;
				}
				case 't':
				{
					thread_num_ = atoi(optarg);
					break;
				}
				case 'c':
				{
					close_log_ = atoi(optarg);
					break;
				}
				case 'a':
				{
					actor_model_ = atoi(optarg);
					break;
				}
				default:
					break;
			}
		}
	}
}