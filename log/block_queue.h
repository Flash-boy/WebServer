// 循环数组实现阻塞队列
// 线程安全的，有互斥锁保证

#ifndef WEBSERVER_LOG_BLOCK_QUEUE_H_
#define WEBSERVER_LOG_BLOCK_QUEUE_H_

#include <iostream>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
namespace TinyWebServer
{

class Locker;
class Cond;

template <class T>
class BlockQueue
{
public:
	explicit BlockQueue(int max_size);
	~BlockQueue();
	void Clear();
	bool Full();
	bool Empty();
	bool Front(T& value);
	bool Back(T& value);
	int Size();
	int MaxSize();
	bool Push(const T& item);
	bool Pop(T& item);
	bool Pop(T& item,int ms_timeout);
private:
	Locker mutex_;
	Cond cond_;
	T* array_;
	int size_;
	int max_size_;
	int front_index_;
	int back_index_;
};

template<class T>
BlockQueue<T>::BlockQueue(int max_size)
{
	assert(max_size > 0);
	max_size_ = max_size_;
	array_ = new T[max_size_];
	size_ = 0;
	front_index_ = -1;
	back_index_ = -1;
}

template <class T>
BlockQueue<T>::~BlockQueue()
{
	mutex_.Lock();
	if(array_!=NULL)
		delete []array_;
	mutex_.UnLock();
}
template <class T>
void BlockQueue<T>::Clear()
{
	mutex_.Lock();
	size_=0;
	front_index_=-1;
	back_index_=-1;
	mutex_.UnLock();
}

template <class T>
int BlockQueue<T>::Size()
{
	int tmp=0;
	mutex_.Lock();
	tmp = size_;
	mutex_.UnLock();
	return tmp;
}
template <class T>
int BlockQueue<T>::MaxSize()
{
	int tmp=0;
	mutex_.Lock();
	tmp=max_size_;
	mutex_.UnLock();
	return tmp;
}
template <class T>
bool BlockQueue<T>::Full()
{
	mutex_.Lock();
	if(size_>=max_size_)
	{
		mutex_.UnLock();
		return true;
	}
	mutex_.UnLock();
	return false;
}
template <class T>
bool BlockQueue<T>::Empty()
{
	mutex_.Lock();
	if(size_==0)
	{	
		mutex_.UnLock();
		return true;
	}
	mutex_.UnLock();
	return false;
}

template <class T>
bool BlockQueue<T>::Front(T& value)
{
	mutex_.Lock();
	if(size_==0)
	{
		mutex_.UnLock();
		return false;
	}
	value=array_[(front_index_+1)%max_size_];
	mutex_.UnLock();
	return true;
}
template <class T>
bool BlockQueue<T>::Back(T& value)
{
	mutex_.Lock();
	if(size_==0)
	{
		mutex_.UnLock();
		return false;
	}
	value=array_[back_index_];
	mutex_.UnLock();
	return true;
}

// 向队列加入元素，类似于生产者产生一个元素，需要唤醒消费者
template <class T>
bool BlockQueue<T>::Push(const T& item)
{
	mutex_.Lock();
	if(size_>=max_size_)
	{
		cond_.Broadcast();
		mutex_.UnLock();
		return false;
	}
	back_index_=(back_index_+1)%max_size_;
	array_[back_index_]=item;
	size_++;
	cond_.Signal();
	mutex_.UnLock();
	return true;
}
// 获取一个元素，没有元素则睡在条件变量上等待唤醒
template <class T>
bool BlockQueue<T>::Pop(T& item)
{
	mutex_.Lock();
	while(size_<=0)
	{
		if(!cond_.Wait(mutex_.Get()))
		{
			mutex_.UnLock();
			return false;
		}
	}
	front_index_=(front_index_+1)%max_size_;
	item=array_[front_index_];
	size_--;
	mutex_.UnLock();
	return true;
}

// 超时处理,毫秒
template <class T>
bool BlockQueue<T>::Pop(T& item,int ms_timeout)
{
	struct timespec t={0,0};
	struct timeval now={0,0};
	gettimeofday(&now,NULL);
	if(size_<=0)
	{
		t.tv_sec=now.tv_sec+ms_timeout/1000;
		t.tv_nsec=(ms_timeout%1000)*1000;
		if(!cond_.TimeWait(mutex_.Get(),t))
		{
			mutex_.UnLock();
			return false;
		}
	}

	// 超时但没有新增的项
	if(size_<=0)
	{
		mutex_.UnLock();
		return false;
	}
	front_index_=(front_index_+1)%max_size_;
	item=array_[front_index_];
	size_--;
	mutex_.UnLock();
	return true;
}

}
#endif