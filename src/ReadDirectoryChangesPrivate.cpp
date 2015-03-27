#include "ReadDirectoryChangesPrivate.h"
#include "ReadDirectoryChanges.h"
#include <list>
using namespace std;


CReadChangesRequest::CReadChangesRequest(CReadChangesServer* pServer, LPCTSTR sz, int id, BOOL b, DWORD dw, DWORD size, WORD ftype)
{
	m_pServer		= pServer;
	m_dwFlags		= dw;
	m_bChildren		= b;
	m_wstrDirectory	= sz;
	m_id			= id;
	m_hDirectory	= 0;
	m_ref			= 1;
	m_subrequest	= NULL;
	m_ftype			= ftype;	//dir and file

	::ZeroMemory(&m_Overlapped, sizeof(OVERLAPPED));

	// The hEvent member is not used when there is a completion
	// function, so it's ok to use it to point to the object.
	m_Overlapped.hEvent = this;

	m_Buffer.resize(size);
	m_BackupBuffer.resize(size);
}
CReadChangesRequest::~CReadChangesRequest()
{
	// RequestTermination() must have been called successfully.
	_ASSERTE(m_hDirectory == NULL);
	//_ASSERTE(m_hDirectory == NULL || m_hDirectory == INVALID_HANDLE_VALUE);
}

bool CReadChangesRequest::OpenDirectory()
{
	// Allow this routine to be called redundantly.
	if (m_hDirectory)
		return true;

	m_hDirectory = ::CreateFile(
		m_wstrDirectory,					// pointer to the file name
		FILE_LIST_DIRECTORY,                // access (read/write) mode
		FILE_SHARE_READ						// share mode
		| FILE_SHARE_WRITE
		| FILE_SHARE_DELETE,
		NULL,                               // security descriptor
		OPEN_EXISTING,                      // how to create
		FILE_FLAG_BACKUP_SEMANTICS |		// 必须加，否则创建失败
		FILE_FLAG_OVERLAPPED,				// file attributes
		NULL);                              // file with attributes to copy

	if (m_hDirectory == INVALID_HANDLE_VALUE)
	{
		cerr << "INVALID_HANDLE_VALUE of m_hDirectory" << GetLastError() << endl;
		return false;
	}

	return true;
}

void CReadChangesRequest::BeginRead()
{
	DWORD dwBytes = 0;
	// This call needs to be reissued after every APC.
	BOOL success = ::ReadDirectoryChangesW(
		m_hDirectory,						// handle to directory
		&m_Buffer[0],                       // read results buffer
		m_Buffer.size(),					// length of buffer
		m_bChildren,                        // monitoring option
		m_dwFlags,							// filter conditions
		&dwBytes,                           // bytes returned
		&m_Overlapped,                      // overlapped buffer
		&NotificationCompletion);           // completion routine
}

//线程回调这个函数，也就是说，下次消息来时候，必须等待这个函数结束
//所以，处理速度应该尽可能的快
//static
VOID CALLBACK CReadChangesRequest::NotificationCompletion(
	DWORD dwErrorCode,									// completion code
	DWORD dwNumberOfBytesTransfered,					// number of bytes transferred
	LPOVERLAPPED lpOverlapped)							// I/O information buffer
{
	CReadChangesRequest* pBlock = (CReadChangesRequest*)lpOverlapped->hEvent;

	//返回这个错误的情形:io被cancel了(CancelIo), 在这里释放request
	if (dwErrorCode == ERROR_OPERATION_ABORTED)
	{
		//CReadChangesServer* pServer = pBlock->m_pServer;
		//::InterlockedDecrement(&pServer->m_nOutstandingRequests);
		::InterlockedDecrement(&pBlock->m_pServer->m_nOutstandingRequests);
		dlog("CReadChangesRequest::NotificationCompletion, io canceled");
		//cout << "req ref:" << pBlock->ref() << endl;
		pBlock->clear_self();
		pBlock->put();
		return;
	}

	// Can't use sizeof(FILE_NOTIFY_INFORMATION) because
	// the structure is padded to 16 bytes.
	_ASSERTE(dwNumberOfBytesTransfered >= offsetof(FILE_NOTIFY_INFORMATION, FileName) + sizeof(WCHAR));

	// This might mean overflow? Not sure.
	if(!dwNumberOfBytesTransfered)
		return;

	//转移buffer
	//为防止处理不够及时，是不是考虑多个outbuffer?
	pBlock->BackupBuffer(dwNumberOfBytesTransfered);

	// 尽快的腾出buffer，重新加入新的监听
	// Get the new read issued as fast as possible. The documentation
	// says that the original OVERLAPPED structure will not be used
	// again once the completion routine is called.
	pBlock->BeginRead();

	//TODO: 这个处理可能比较慢, 但是这里处理逻辑简单
	pBlock->ProcessNotification();
}

void CReadChangesRequest::ProcessNotification()
{
	char* pBase = (char*)&m_BackupBuffer[0];
	list<TDirectoryChangeNotification> notifications;

	for (;;)
	{
		FILE_NOTIFY_INFORMATION& fni = (FILE_NOTIFY_INFORMATION&)*pBase;

		CStringW wstrFilename(fni.FileName, fni.FileNameLength/sizeof(wchar_t));
		//不组合全路径的path，直接用id代替
		/*
		// Handle a trailing backslash, such as for a root directory.
		if (m_wstrDirectory.Right(1) != L"\\")
			wstrFilename = m_wstrDirectory + L"\\" + wstrFilename;
		else
			wstrFilename = m_wstrDirectory + wstrFilename;

		// If it could be a short filename, expand it.
		LPCWSTR wszFilename = PathFindFileNameW(wstrFilename);
		int len = lstrlenW(wszFilename);
		// The maximum length of an 8.3 filename is twelve, including the dot.
		if (len <= 12 && wcschr(wszFilename, L'~'))
		{
			// Convert to the long filename form. Unfortunately, this
			// does not work for deletions, so it's an imperfect fix.
			wchar_t wbuf[MAX_PATH];
			if (::GetLongPathName(wstrFilename, wbuf, _countof (wbuf)) > 0)
				wstrFilename = wbuf;
		}

		m_pServer->m_pBase->Push(fni.Action, wstrFilename);
		*/
		//use push_n
		//m_pServer->m_pBase->Push(fni.Action, wstrFilename, m_id, m_ftype);
		notifications.push_back(TDirectoryChangeNotification(fni.Action, wstrFilename, m_id, m_ftype));

		if (!fni.NextEntryOffset)
			break;
		pBase += fni.NextEntryOffset;
	};
	//本来想用这个来表示结束符，但是发现没有规律!!放弃这个做法
	//notifications.push_back(TDirectoryChangeNotification(FILE_ACTION_END, L"", m_id, m_ftype));	
	m_pServer->m_pBase->PushN(notifications);
}

void CReadChangesRequest::clear_self()
{
	//清空自己的通知
	m_pServer->m_pBase->clear_notify(m_id);
}


void CReadChangesServer::Run()
{
	while (m_nOutstandingRequests || !m_bTerminate)
	{
		DWORD rc = ::SleepEx(INFINITE, true);
	}
	//线程退出，应该标识changes实例销毁，但是如何通知包含它的容器呢？
	m_pBase->Release();
}
