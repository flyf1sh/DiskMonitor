#include "ReadDirectoryChanges.h"

#include "ReadDirectoryChangesPrivate.h"

//using namespace ReadDirectoryChangesPrivate;

CReadDirectoryChanges::CReadDirectoryChanges()
{
	m_hThread = NULL;
	m_dwThreadId = 0;
	m_pServer = new CReadChangesServer(this);
	m_next = NULL;
}

CReadDirectoryChanges::~CReadDirectoryChanges()
{
	Terminate();
	delete m_pServer;	//移到Terminate的线程退出时候delete更好，否则超时后delete了线程会访问无效地址
}

void CReadDirectoryChanges::Init()
{
	// Kick off the worker thread, which will be
	// managed by CReadChangesServer.
	m_hThread = (HANDLE)_beginthreadex(NULL, 
		0, 
		CReadChangesServer::ThreadStartProc,	//这个线程函数就只是单纯的sleep，但是会处理异步函数
		m_pServer, 
		0, 
		&m_dwThreadId);
}

void CReadDirectoryChanges::Release()
{
	/*
	for cb in releaseCBs:
		cb(this)
	 */
	delete this;
}

void CReadDirectoryChanges::Terminate()
{
	if (m_hThread)
	{
		HANDLE hThread = m_hThread;
		m_hThread = NULL;
		::QueueUserAPC(CReadChangesServer::TerminateProc, hThread, (ULONG_PTR)m_pServer);
		::WaitForSingleObjectEx(hThread, 10000, true);	//10s
		//XXX if timeout, terminate thread ??
		::CloseHandle(hThread);
		m_dwThreadId = 0;
		m_reqs.clear();	//already put req in lower
	}
}

void CReadDirectoryChanges::AddDirectory(
							 LPCTSTR wszDirectory, int id,
							 BOOL bWatchSubtree, DWORD dwNotifyFilter, 
							 DWORD dwBufferSize )
{
	if (!m_hThread)
	{
		Init();
	}
	CReadChangesRequest* pRequest = NULL;
	if(dwNotifyFilter != 0)
	{
		pRequest = new CReadChangesRequest(m_pServer, wszDirectory, id, bWatchSubtree, dwNotifyFilter, dwBufferSize, ALL_TYPE);
	}
	else
	{ //分离目录监听和文件监听
		pRequest = new CReadChangesRequest(m_pServer, wszDirectory, id, bWatchSubtree,
										   DIR_NOTIFY_CHANGE_FLAGS, dwBufferSize, DIR_TYPE);
		pRequest->m_subrequest = new CReadChangesRequest(m_pServer, wszDirectory, id, bWatchSubtree,
														 FILE_NOTIFY_CHANGE_FLAGS, dwBufferSize, FILE_TYPE);
		pRequest->m_subrequest->get();
	}
	pRequest->get();
	m_reqs[id] = pRequest;
	//让线程去调用这个函数，并且回调会被触发
	::QueueUserAPC(CReadChangesServer::AddDirectoryProc, m_hThread, (ULONG_PTR)pRequest);
	if(dwNotifyFilter == 0)
		::QueueUserAPC(CReadChangesServer::AddDirectoryProc, m_hThread, (ULONG_PTR)pRequest->m_subrequest);
}


bool CReadDirectoryChanges::DelDirectory(int id)
{
	map<int, CReadChangesRequest*>::iterator it = m_reqs.find(id);
	if( it == m_reqs.end())
		return false;
	CReadChangesRequest * req = it->second;
	m_reqs.erase(it);
	//dlog("CReadDirectoryChanges::DelDirectory before put req");
	//cout << "req ref:" << req->ref() << endl;
	req->put();	//XXX 因为add那时候出错的防范
	CReadChangesRequest * subreq = req->m_subrequest;
	req->m_subrequest = NULL;
	::QueueUserAPC(CReadChangesServer::DelDirectoryProc, m_hThread, (ULONG_PTR)req);
	if(subreq)
	{
		subreq->put();
		::QueueUserAPC(CReadChangesServer::DelDirectoryProc, m_hThread, (ULONG_PTR)subreq);
	}
	return true;
};

//call by inner worker thread
void CReadDirectoryChanges::Push(DWORD dwAction, CStringW& wstrFilename, WORD id, WORD type) 
{
	m_Notifications.push( TDirectoryChangeNotification(dwAction, wstrFilename, id, type) );
}

bool CReadDirectoryChanges::Pop(DWORD& dwAction, CStringW& wstrFilename, WORD& id, WORD& t) 
{
	TDirectoryChangeNotification ni;
	if (!m_Notifications.pop(ni))
		return false;

	dwAction = ni.dwAct;
	wstrFilename = ni.rPath;
	id = ni.id;
	t = ni.type;
	return true;
}

void CReadDirectoryChanges::PushN(list<TDirectoryChangeNotification> & li)
{
	m_Notifications.push_n(li);
}

void CReadDirectoryChanges::PopAll(list<TDirectoryChangeNotification> & li)
{
	m_Notifications.pop_all(li);
}

//检查是否溢出，溢出后清空？（比较武断）
bool CReadDirectoryChanges::CheckOverflow()
{
	bool b = m_Notifications.overflow();
	if (b)
		m_Notifications.clear();
	return b;
}

