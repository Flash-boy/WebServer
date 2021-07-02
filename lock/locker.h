#ifndef WEBSERVER_LOCK_LOCKER_H_
#define WEBSERVER_LOCK_LOCKER_H_

#include <exception>
#include <pthread.h>
#include <semaphore.h>

namespace TinyWebServer
{

class Semaphore
{
public:
	Semaphore();
	Semaphore(int num);
	~Semaphore();
	bool Wait();
	bool Post();
private:
	sem_t sem_;
};

class Locker
{
public:
	Locker();
	~Locker();
	bool Lock();
	bool UnLock();
	pthread_mutex_t* Get();
private:
	pthread_mutex_t mutex_;
};

class Cond
{
public:
	Cond();
	~Cond();
	bool Wait(pthread_mutex_t* mutex);
	bool TimeWait(pthread_mutex_t* mutex,struct timespec t);
	bool Signal();
	bool Broadcast();
private:
	pthread_cond_t cond_;
};

}

#endif
