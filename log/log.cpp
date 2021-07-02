#include "log.h"
#include <string.h>
#include <sys/time.h>
#include <stdarg.h>
namespace TinyWebServer
{


Log* Log::GetInstance()
{
	static Log instance;
	return &instance;
}
void* Log::FlushLogThread(void* args)
{
	Log::GetInstance()->AsyncWriteLog();
	// 正常运行不会执行到这一步
	return NULL;
}
// 异步写日志线程函数
void* Log::AsyncWriteLog()
{
	std::string single_log;
	while(log_queue_->Pop(single_log))
	{
		mutex_.Lock();
		fputs(single_log.c_str(),fp_);
		mutex_.UnLock();
	}
}

Log::Log()
{
	count_=0;
	is_async_=false;
}
Log::~Log()
{
	if(fp_!=NULL)
		fclose(fp_);
}
bool Log::Init(const char* filename,int close_log,int log_buf_size,int split_lines,int max_queue_size)
{
	if(max_queue_size>=1)
	{
		is_async_=true;
		log_queue_=new BlockQueue<std::string>(max_queue_size);
		pthread_t tid;
		pthread_create(&tid,NULL,FlushLogThread,NULL);
	}
	close_log_=close_log;
	log_buf_size_=log_buf_size;
	buf_=new char[log_buf_size_];
	memset(buf_,0,log_buf_size_);
	split_lines_=split_lines;

	time_t t=time(NULL);
	struct tm* sys_tm=localtime(&t);
	struct tm now_time = *sys_tm;

	const char* p =strrchr(filename,'/');
	char log_full_name[256]={0};
	if(p==NULL)
	{
		snprintf(log_full_name,255,"%d_%02d_%02d_%s",now_time.tm_year+1900,now_time.tm_mon+1,now_time.tm_mday,filename);
	}
	else
	{
		strcpy(log_name_,p+1);
		strncpy(dir_name_,filename,p-filename+1);
		snprintf(log_full_name,255,"%s%d_%02d_%02d_%s",dir_name_,now_time.tm_year+1900,now_time.tm_mon+1,now_time.tm_mday,log_name_);
	}
	today_=now_time.tm_mday;
	fp_=fopen(log_full_name,"a");
	if(fp_==NULL)
		return false;
	return true;
}

void Log::WriteLog(int log_level,const char* format,...)
{
	struct timeval now = now={0,0};
	gettimeofday(&now,NULL);
	time_t t=now.tv_sec;
	struct tm* sys_tm = localtime(&t);
	struct tm now_time = *sys_tm;
	char s[16]={0};
	switch(log_level)
	{
		case 0:
			strcpy(s,"[debug]:");
			break;
		case 1:
			strcpy(s,"[info]:");
			break;
		case 2:
			strcpy(s,"[warn]:");
			break;
		case 3:
			strcpy(s,"[error]:");
			break;
		default:
			strcpy(s,"[info]:");
			break;
	}
	// 锁保护count_和一些内部变量
	mutex_.Lock();
	++count_;
	if(today_!=now_time.tm_mday || count_%split_lines_==0)
	{
		char new_log[256]={0};
		fflush(fp_);
		fclose(fp_);

		char tail[16]={0};
		snprintf(tail,16,"%d_%02d_%02d_",now_time.tm_year+1900,now_time.tm_mon+1,now_time.tm_mday);
		if(today_!=now_time.tm_mday)
		{
			snprintf(new_log,255,"%s%s%s",dir_name_,tail,log_name_);
			today_=now_time.tm_mday;
			count_=0;
		}
		else
		{
			snprintf(new_log,255,"%s%s%s.%lld",dir_name_,tail,log_name_,count_/split_lines_);
		}
		fp_=fopen(new_log,"a");
	}
	mutex_.UnLock();
	
	// 真正的日志数据
	va_list valst;
	va_start(valst,format);
	std::string log_str;
	
	// mutex_保护buf_
	mutex_.Lock();
	int n=snprintf(buf_,48,"%d-%02d-%02d %02d:%02d:%02d.%06ld %s",now_time.tm_year+1900,now_time.tm_mon+1,now_time.tm_mday,
									now_time.tm_hour,now_time.tm_min,now_time.tm_sec,now.tv_usec,s);
	int m=vsnprintf(buf_+n,log_buf_size_-n-2,format,valst);
	buf_[m+n]='\n';
	buf_[m+n+1]='\0';									
	log_str=buf_;
	mutex_.UnLock();

	if(is_async_ && !log_queue_->Full())
	{
		log_queue_->Push(log_str);
	}
	else
	{
		mutex_.Lock();
		fputs(log_str.c_str(),fp_);
		mutex_.UnLock();
	}
	va_end(valst);
}
void Log::Flush()
{
	mutex_.Lock();
	fflush(fp_);
	mutex_.UnLock();
}



}