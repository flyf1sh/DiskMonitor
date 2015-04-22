#ifndef THREADSAFEQUEUE_H
#define THREADSAFEQUEUE_H

#include "iostream"
#include <list>
using namespace std;
#include "MonitorUtil.h"

/*
template <typename C>
class CThreadSafeQueue : protected list<C>
{
public:
	CThreadSafeQueue(int nMaxCount)
	{
		m_bOverflow = false;
		m_hSemaphore = ::CreateSemaphore(
			NULL,		//不安全的属性
			0 ,			//初始值
			nMaxCount,	//最大值
			NULL);		//对象名称
		InitCS(&m_Crit);
	}
	~CThreadSafeQueue()
	{
		DestCS(&m_Crit);
		::CloseHandle(m_hSemaphore);
		m_hSemaphore = NULL;
	}

	void push(C& c)
	{
		CSLock lock(m_Crit);
		push_back(c);
		lock.Unlock();
		//用于对指定的信号量增加指定的值
		if (!::ReleaseSemaphore(m_hSemaphore, 1, NULL))
		{
			//如果信号满了,删除
			pop_back();
			if (GetLastError() == ERROR_TOO_MANY_POSTS)
			{
				m_bOverflow = true;
			}
		}
	}
	//如果不止一次调用pop(),记录信号量数。
	//FIXME: 没必要返回失败
	bool pop(C& c)
	{
		CSLock lock(m_Crit);
		//如果是空，会不会死锁?
		if (empty()) 
		{
			//这里是为了清空信号量的signal
			while (::WaitForSingleObject(m_hSemaphore, 0) != WAIT_TIMEOUT)
				1;
			return false;
		}
		c = front();
		pop_front();
		return true;
	} 

	//如果溢出使用下面的队列
	void clear()
	{
		CSLock lock(m_Crit);

		for (DWORD i=0; i<size(); i++)
			WaitForSingleObject(m_hSemaphore, 0);

		__super::clear();

		m_bOverflow = false;
	}
	//溢出
	bool overflow()
	{
		return m_bOverflow;
	}
	//获得句柄
	HANDLE GetWaitHandle() { return m_hSemaphore; }
protected:
	HANDLE m_hSemaphore;
	CRITICAL_SECTION m_Crit;
	bool m_bOverflow;
};
*/

//支持批量处理，用event代替信号灯，去掉最大的数量限制
template <typename C>
class CThreadSafeQueuePro : protected list<C>
{
public:
	CThreadSafeQueuePro()
	{
		m_event = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		InitCS(&m_Crit);
	}
	~CThreadSafeQueuePro()
	{
		DestCS(&m_Crit);
		::CloseHandle(m_event);
		m_event = NULL;
	}

	void push(C& c)
	{
		CSLock lock(m_Crit);
		push_back(c);
		::SetEvent(m_event);
	}

	bool pop(C& c)
	{
		CSLock lock(m_Crit);
		//如果是空，会不会死锁?
		if (empty()) 
		{
			return false;
		}
		c = front();
		pop_front();
		if(!empty())
			::SetEvent(m_event);	//发信号，激活下一个
		return true;
	} 

	void push_n(list<C>& li)
	{
		CSLock lock(m_Crit);
		splice(end(),li);
		::SetEvent(m_event);
	}

	void pop_all(list<C>& li)
	{
		CSLock lock(m_Crit);
		li.clear();
		swap(li);
	}

	//最后清理，不需要唤醒等待的线程
	void clear()
	{
		CSLock lock(m_Crit);
		ResetEvent(m_event);
		__super::clear();
	}

	template<typename T>
	void clear_invalid(const T & arg, bool (*isvalid)(const C & item, const T &))
	{
		dlog("in clear_invalid,", true);
		CSLock lock(m_Crit);
		//dout << "size:" << this->size() << dendl;
		list<C>::iterator it = begin();
		for (; it != end();)
		{
			it = isvalid(*it, arg) ? ++it : erase(it);
		}
	}

	bool overflow(){ return false; }
	//获得句柄
	HANDLE GetWaitHandle() { return m_event; }
protected:
	HANDLE m_event;
	CRITICAL_SECTION m_Crit;
};
#endif
