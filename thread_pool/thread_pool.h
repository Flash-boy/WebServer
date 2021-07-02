
#ifndef WEBSERVER_THREAD_POOL_THREAD_POOL_H_
#define WEBSERVER_THREAD_POOL_THREAD_POOL_H_

#include "../lock/locker.h"
#include <pthread.h>
#include <list>
#include <exception>


namespace TinyWebServer
{
class ConnectionPool;
class ConnectionRAII;

template <class T>
class ThreadPool
{
public:
	ThreadPool(int actor_model,ConnectionPool* conn_pool,int thread_number=8,int max_request = 10000);
	~ThreadPool();
	bool Append(T* request,int state);
	bool AppendP(T* request);
	
private:
	static void* WorkerThread(void* arg);
	void Run();
private:
	int thread_number_;
	int max_requests_;
	pthread_t* threads_;
	std::list<T*> request_queue_;
	Locker queue_locker_;
	Semaphore queue_stat_;
	ConnectionPool* conn_pool_;
	int actor_model_;
};

template <class T>
ThreadPool<T>::ThreadPool(int actor_model,ConnectionPool* conn_pool,int thread_number,int max_request)
	:actor_model_(actor_model),conn_pool_(conn_pool),thread_number_(thread_number),max_requests_(max_request)
{
	if(thread_number_<=0 || max_requests_<=0)
		throw std::exception();
	threads_ = new pthread_t[thread_number_];
	if(!threads_)
		throw std::exception();
	for(int i=0;i<thread_number_;i++)
	{
		if(pthread_create(threads_+i,NULL,WorkerThread,this)!=0)
		{
			delete[] threads_;
			throw std::exception();
		}
		if(pthread_detach(threads_[i]))
		{
			delete[] threads_;
			throw std::exception();
		}
	}
}

template <class T>
ThreadPool<T>::~ThreadPool()
{
	delete[] threads_;
}

template <class T>
bool ThreadPool<T>::Append(T* request,int state)
{
	queue_locker_.Lock();
	if(request_queue_.size()>=max_requests_)
	{
		queue_locker_.UnLock();
		return false;
	}
	request->state_=state;
	request_queue_.push_back(request);
	queue_locker_.UnLock();
	queue_stat_.Post();
	return true;
}

template <class T>
bool ThreadPool<T>::AppendP(T* request)
{
	queue_locker_.Lock();
	if(request_queue_.size()>=max_requests_)
	{
		queue_locker_.UnLock();
		return false;
	}
	request_queue_.push_back(request);
	queue_locker_.UnLock();
	queue_stat_.Post();
	return true;
}

template <class T>
void* ThreadPool<T>::WorkerThread(void* arg)
{
	ThreadPool* pool = (ThreadPool*)arg;
	pool->Run();
	return pool;
}
template <class T>
void ThreadPool<T>::Run()
{
	while(true)
	{
		queue_stat_.Wait();
		queue_locker_.Lock();
		if(request_queue_.empty())
		{
			queue_locker_.UnLock();
			continue;
		}
		T* request = request_queue_.front();
		request_queue_.pop_front();
		queue_locker_.UnLock();
		if(!request)
		{
			continue;
		}
		// Reactor 模式
		if(actor_model_==1)
		{
			// 读
			if(request->state_==0)
			{
				if(request->ReadOnce())
				{
					request->improv_=1;
					ConnectionRAII mysqlconn(&request->mysql_,conn_pool_);
					request->Process();
				}
				else
				{
					request->improv_=1;
					request->time_flag_ =1;
				}
			}
			// 写
			else
			{
				if(request->Write())
				{
					request->improv_= 1;
				}
				else
				{
					request->improv_=1;
					request->time_flag_ =1;
				}
			}
		}
		// PReactor 模式
		else
		{
			ConnectionRAII mysqlconn(&request->mysql_,conn_pool_);
			request->Process();
		}
	}
}

}


#endif