#ifndef DIRECTORY_CHANGE_FACTOR_H
#define DIRECTORY_CHANGE_FACTOR_H

#include "ReadDirectoryChanges.h"
#include<map>
using namespace std;

#define MSG_WAIT_INTERVAL 500*2

//处理所有目录changes的类，是一个单件
//这个类实例有个worker线程来处理所有的改变，并且回调每个监控请求的回调
class DirectoryChangeHandler
{
	friend class DirectoryMonitor;
public:
	//有个worker线程来处理所有的请求，这样减少monitor线程的负担
	DirectoryChangeHandler(int typeNum, int threadMax=3, DWORD waittime=MSG_WAIT_INTERVAL);
	~DirectoryChangeHandler();
	BOOL Terminate();

private:
	void Init();
	BOOL AddDirectory(DirectoryMonitor * monitor);
	BOOL DelDirectory(DirectoryMonitor * monitor);

	//worker 线程处理函数
	void handle_timeout();
	void handle_notify(int index);

	DWORD NextWaitTime(){
		return m_waittime;
	}
	
	static unsigned int WINAPI WorkThreadProc(LPVOID arg);
	//APC 函数
	static void CALLBACK AddDirectoryProc(__in  ULONG_PTR arg);
	static void CALLBACK DelDirectoryProc(__in  ULONG_PTR arg);
	static void CALLBACK TerminateProc(__in  ULONG_PTR arg);
private:
	static int _id;
	int m_ntypes;	//本用来管理后台监控的数量的，但是MAXIMUM_WAIT_OBJECTS
	int m_threads;
	bool m_running;

	HANDLE m_hThread;
	unsigned int m_dwThreadId;
	DWORD m_waittime;	//等待唤醒时间
	DWORD m_def_waittime;	//默认等待时间
	map<int, DirectoryMonitor*>			m_monitors;		//回调需要这个
	map<string, CReadDirectoryChanges*> m_changes;		//type => changes

	//同步管理
	unsigned int m_nHandles;
	HANDLE					m_changeHandles[MAXIMUM_WAIT_OBJECTS];
	CReadDirectoryChanges * m_changeArr[MAXIMUM_WAIT_OBJECTS];
	HANDLE					m_event;
};
#endif
