#include "locker.h"

namespace TinyWebServer
{

// 信号量
Semaphore::Semaphore()
{
	if (sem_init(&sem_, 0, 0) != 0)
		throw std::exception();
}
Semaphore::Semaphore(int num)
{
	if (sem_init(&sem_, 0, num) != 0)
		throw std::exception();
}
Semaphore::~Semaphore()
{
	sem_destroy(&sem_);
}
bool Semaphore::Wait()
{
	return sem_wait(&sem_) == 0;
}
bool Semaphore::Post()
{
	return sem_post(&sem_) == 0;
}

// 互斥锁
Locker::Locker()
{
	if (pthread_mutex_init(&mutex_, NULL) != 0)
		throw std::exception();
}
Locker::~Locker()
{
	pthread_mutex_destroy(&mutex_);
}
bool Locker::Lock()
{
	return pthread_mutex_lock(&mutex_) == 0;
}
bool Locker::UnLock()
{
	return pthread_mutex_unlock(&mutex_) == 0;
}
pthread_mutex_t* Locker::Get()
{
	return &mutex_;
}

// 条件变量
Cond::Cond()
{
	if (pthread_cond_init(&cond_, NULL) != 0)
		throw std::exception();
}
Cond::~Cond()
{
	pthread_cond_destroy(&cond_);
}
bool Cond::Wait(pthread_mutex_t* mutex)
{
	int ret = 0;
	ret = pthread_cond_wait(&cond_, mutex);
	return ret == 0;
}
bool Cond::TimeWait(pthread_mutex_t* mutex,struct timespec t)
{
	int ret = 0;
	ret = pthread_cond_timedwait(&cond_, mutex, &t);
	return ret == 0;
}

bool Cond::Signal()
{
	return pthread_cond_signal(&cond_) == 0;
}
bool Cond::Broadcast()
{
	return pthread_cond_broadcast(&cond_) == 0;
}

}