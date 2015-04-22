#ifndef READDIRECTORYCHANGES_H
#define READDIRECTORYCHANGES_H

#include <map>
using namespace std;
#include "MonitorUtil.h"
#include "ThreadSafeQueue.h"


//namespace ReadDirectoryChangesPrivate
//{
	class CReadChangesServer;
	class CReadChangesRequest;
//}

#define MaxDirectoryChanges 10000
#define NotifyBufferSize	4*1024*1024

/*
 *	用来管理目录的改变，这个类的实例会是一个独立的线程进行处理
 *	每个实例可以同时监控多个目录，也就是说一个线程来管理多个目录的change
 *	所以，可能要考虑负载均衡的因素
 *
 *	XXX 这个类的所有调用都没有多线程保护，也就是说它默认了调用它的线程只有一个 XXX
 */
class CReadDirectoryChanges
{
public:
	CReadDirectoryChanges();
	~CReadDirectoryChanges();
	
	void Init();
	void Release();
	void Terminate();
	/// <summary>
	/// Add a new directory to be monitored.
	/// </summary>
	/// <param name="wszDirectory">Directory to monitor.</param>
	/// <param name="bWatchSubtree">True to also monitor subdirectories.</param>
	/// <param name="dwNotifyFilter">The types of file system events to monitor, such as FILE_NOTIFY_CHANGE_ATTRIBUTES.</param>
	/// <param name="dwBufferSize">The size of the buffer used for overlapped I/O.</param>
	/// <remarks>
	/// <para>
	/// This function will make an APC call to the worker thread to issue a new
	/// ReadDirectoryChangesW call for the given directory with the given flags.
	/// </para>
	/// </remarks>
	void AddDirectory(LPCTSTR wszDirectory, int id, BOOL bWatchSubtree, DWORD dwNotifyFilter, DWORD dwBufferSize=NotifyBufferSize);

	//根据id删除对应的req
	bool DelDirectory(int id);

	/// <summary>
	/// Return a handle for the Win32 Wait... functions that will be
	/// signaled when there is a queue entry.
	/// </summary>
	// 接收到的队列的句柄，可以多次取
	HANDLE GetWaitHandle() { return m_Notifications.GetWaitHandle(); }

	bool Pop(DWORD& dwAction, CStringW& wstrFilename, WORD& id, WORD& type);

	// "Push" is for usage by ReadChangesRequest.  Not intended for external usage.
	void Push(DWORD dwAction, CStringW& wstrFilename, WORD id, WORD type);

	void PushN(list<TDirectoryChangeNotification> & li);
	void PopAll(list<TDirectoryChangeNotification> & li);

	void clear_notify(int id)
	{
		m_Notifications.clear_invalid(id, valid_id);
	}

	static bool valid_id(const TDirectoryChangeNotification & ni, const int & invalid_id)
	{
		return ni.id != invalid_id ? true : false;
	}

	// Check if the queue overflowed. If so, clear it and return true.
	bool CheckOverflow();

	unsigned int GetThreadId() { return m_dwThreadId; }

	int GetRequestNum() const { return m_reqs.size();}

protected:
	CReadChangesServer* m_pServer;

	CReadDirectoryChanges * m_next;

	HANDLE m_hThread;

	unsigned int m_dwThreadId;

	CThreadSafeQueuePro<struct TDirectoryChangeNotification> m_Notifications;

	map<int, CReadChangesRequest*> m_reqs;
private:
};

#endif
