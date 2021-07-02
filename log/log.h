// 异步日志类和阻塞队列搭配实现日志的异步写入到文件中

#ifndef WEBSERVER_LOG_LOG_H_
#define WEBSERVER_LOG_LOG_H_

#include "../lock/locker.h"
#include "block_queue.h"
#include <string>
#include <iostream>
#include <stdio.h>


namespace TinyWebServer
{


class Log
{
public:
	// 单例模式
	static Log* GetInstance();
	// 异步写日志线程函数
	static void* FlushLogThread(void * arg);
	bool Init(const char* filename,int close_log,int log_buf_size=8192,int split_lines=5000000,int max_queue_size=0);
	void WriteLog(int log_level,const char* format,...);
	void Flush();
private:
	Log();
	virtual ~Log();
	void* AsyncWriteLog();
private:
	char dir_name_[128];
	char log_name_[128];
	int split_lines_;
	int log_buf_size_;
	long long count_;
	int today_;
	FILE* fp_;
	char* buf_;
	BlockQueue<std::string>* log_queue_;
	bool is_async_;
	Locker mutex_;
	int close_log_;
};

#define LOG_DEBUG(format,...) {Log::GetInstance()->WriteLog(0,format,##__VA_ARGS__); }
#define LOG_INFO(format,...) {Log::GetInstance()->WriteLog(1,format,##__VA_ARGS__); }
#define LOG_WARN(format,...) {Log::GetInstance()->WriteLog(2,format,##__VA_ARGS__); }
#define LOG_ERROR(format,...) {Log::GetInstance()->WriteLog(3,format,##__VA_ARGS__); }

}


#endif