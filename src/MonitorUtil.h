#ifndef MONITOR_UTIL_H
#define MONITOR_UTIL_H

#include <iostream>
#include <vector>
#include <string>
#include <windows.h>
#include <WinBase.h>
#include <atlstr.h>
#include <stdio.h>
#include "MonitorDebug.h"
using namespace std;

#define ALL_NOTIFY_CHANGE_FLAGS		FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME
#define FILE_NOTIFY_CHANGE_FLAGS	FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME
#define DIR_NOTIFY_CHANGE_FLAGS		FILE_NOTIFY_CHANGE_DIR_NAME

#define DIR_TYPE	0
#define FILE_TYPE	1
#define ALL_TYPE	2

#define LOCAL_NOTIFY 100
//#define FILE_ACTION		(FILE_ACTION_ADDED+1000)
#define FILE_ACTION		1001
#define FILE_ADDED		(FILE_ACTION + 0)
#define DIR_ADDED		(FILE_ACTION + 1)
#define FILE_REMOVED	(FILE_ACTION + 2)
#define DIR_REMOVED		(FILE_ACTION + 3)
#define FILE_RENAMED	(FILE_ACTION + 4)
#define DIR_RENAMED		(FILE_ACTION + 5)
#define FILE_MOVED		(FILE_ACTION + 6)
#define DIR_MOVED		(FILE_ACTION + 7)
#define FILE_MODIFIED	(FILE_ACTION + 8)
//需要外部遍历这个目录
#define DIR_COPY		(FILE_ACTION + 9)

//通知结束标示符
#define FILE_ACTION_END	(FILE_ACTION + 100)

//内部状态
#define FILE_ACTION_ADDED_RENAMED (FILE_ACTION + 10)

#define TEMP_DIRNAME	"~.temp_zbkc2014"
#define TEMP_DIRNAMEW	L"~.temp_zbkc2014"

typedef struct TDirectoryChangeNotification{
	DWORD dwAct;
	CStringW rPath;
	WORD id;
	WORD type;
	TDirectoryChangeNotification(){};
	TDirectoryChangeNotification(DWORD action, CStringW rpath, WORD _id, WORD _type):
		dwAct(action), rPath(rpath), id(_id), type(_type){ };
}TDirectoryChangeNotification;

typedef struct notification_t{
	DWORD act;
	DWORD attr;		//file attribute
	wstring path;
	wstring path2;
	WORD id;		//同名等待个数(没有用链表的代替方案)
	WORD expert_act;	//期待的操作
	bool isdir;		//file or dir
	bool filted;	//是否被过滤了，这个是依据过滤规则过滤
	bool handled;	//转换了act	//XXX 没多大用 
	bool special;	//特殊文件目录（隐藏，系统）
	bool spec2;		//记录上一个文件属性
	bool fspec;		//父路径是特殊的，本身不是
	bool exist;		//节点存在
	notification_t(){};
	notification_t(DWORD action, const wstring & rpath, bool _isdir):
		act(action), attr(0), path(rpath), id(0), expert_act(0), 
		isdir(_isdir), filted(false), handled(false), special(false), spec2(false), fspec(false), exist(true)
		{ };
	notification_t& operator = (const notification_t & other)
	{
		act		= other.act;
		attr	= other.attr;
		path	= other.path;
		path2	= other.path2;
		expert_act = other.expert_act;
		isdir	= other.isdir;
		filted	= other.filted;
		special = other.special;
		spec2	= other.spec2;
		fspec	= other.fspec;
		exist	= other.exist;
		return *this;
	}
}notification_t;

inline bool operator == (const notification_t & no, const notification_t & other)
{
	return no.act == other.act && no.path == other.path && no.path2 == other.path2;
}

//外部数据结构
//移动拷贝改名目录 冲突时的选项 默认 merge
#define FOP_OVERWRITE_MERGE 0x01
#define FOP_REPLACE			0x02
#define FOP_RECYCLE			0x04
#define FOP_IGNORE_EXIST	0x08
#define FOP_IGNORE_ERR		0x10
#define FOP_ROLLBACK		0x20

typedef struct LocalOp{
		int act;			//操作
		string from;		//如果是移动操作，是移动的相对监控的路径；如果是改名，是改名前名称； 否则为空
		string to;			//被操作的文件名/目录名；父路径由LocalNotification提供
}LocalOp;

//同一路径下通知的集合, 通知本身和子op都是时序的
typedef struct LocalNotification{
	string basedir;		//监控的路径
	string fpath;		//父路径, 根路径为""
	string t;			//标识
	vector<LocalOp> ops;	//同一目录下的操作
}LocalNotification;

inline void WideToMutilByte(const wstring& _src, string & strRet)
{
	int nBufSize = WideCharToMultiByte(GetACP(), 0, _src.c_str(), -1, NULL, 0, 0, FALSE);
	strRet.resize(nBufSize-1);
	WideCharToMultiByte(GetACP(), 0, _src.c_str(), -1, (char*)(strRet.c_str()), nBufSize-1, 0, FALSE);
}

inline void WideToMutilByteW(LPCWSTR _src, string & strRet)
{
	int nBufSize = WideCharToMultiByte(GetACP(), 0, _src, -1, NULL, 0, 0, FALSE);
	strRet.resize(nBufSize-1);
	WideCharToMultiByte(GetACP(), 0, _src, -1, (char*)(strRet.c_str()), nBufSize-1, 0, FALSE);
}

inline string WideToMutilByte(const wstring& _src)
{
	string strRet;
	WideToMutilByte(_src, strRet);
	return strRet;
}

inline void MutilByteToWide(const string& str, wstring & wstr)
{
	int len = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1/*str.length()+1*/, 0, 0) - 1;
	wstr.resize(len);
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, (wchar_t*)wstr.c_str(), len);
}

//转换/ 为 \  
inline BOOL RegularPath(string & path)
{
	BOOL ret = FALSE;
	char * start = (char *)path.c_str();
	char * p = start;
	while(*p != '\0')
	{
		if(*p == '/')
		{
			*p = '\\';
			ret = TRUE;
		}
		p++;
	}
	int endcnt = 0;
	while(--p > start && *p == '\\')
		endcnt++;
	if(endcnt)
		path = path.substr(0, path.length()-endcnt);
	return ret;
}
inline BOOL RegularPath(wstring & path)
{
	BOOL ret = FALSE;
	wchar_t * start = (wchar_t *)path.c_str();
	wchar_t * p = start;
	while(*p != L'\0')
	{
		if(*p == L'/')
		{
			*p = L'\\';
			ret = TRUE;
		}
		p++;
	}
	int endcnt = 0;
	while(--p > start && *p == L'\\')
		endcnt++;
	if(endcnt)
		path = path.substr(0, path.length()-endcnt);
	return ret;
}

//filesystem api
//XXX:应该适配\ 和/, 可以调用 PathRemoveFileSpec
//这里全部默认，已经调用了RegularPath，分隔符是\\, 这样简单一点
inline string GetBaseDIR(const string & path, char slash='\\')
{
	int loc = path.rfind(slash, path.length());
	if(loc == string::npos)
		return "";
	while(path.at(loc-1) == slash)
		loc--;
	return path.substr(0, loc);
}
inline wstring GetBaseDIR(const wstring & path, wchar_t slash=L'\\')
{
	int loc = path.rfind(slash, path.length());
	if(loc == wstring::npos)
		return L"";
	while(path.at(loc-1) == slash)
		loc--;
	return path.substr(0, loc);
}

//TODO 用windows api PathFindFileName(有麻烦的地方，参数)
inline string GetFileName(const string & path, char slash='\\')
{
	int loc = path.rfind(slash, path.length());
	if(loc == string::npos)
		return path;
	return path.substr(loc+1);
}
inline wstring GetFileName(const wstring & path, wchar_t slash=L'\\')
{
	int loc = path.rfind(slash, path.length());
	if(loc == wstring::npos)
		return path;
	return path.substr(loc+1);
}

inline string GetFileExt(const string & path)
{
	int loc = path.rfind('.', path.length());
	if(loc == string::npos)
		return "";
	return path.substr(loc+1);
}
inline wstring GetFileExt(const wstring & path)
{
	int loc = path.rfind(L'.', path.length());
	if(loc == wstring::npos)
		return L"";
	return path.substr(loc+1);
}

//顺序是由底到顶
inline vector<wstring> GetBaseDIRs(wstring path, wchar_t slash=L'\\')
{
	vector<wstring> fathers;
	do{
		wstring fpath = GetBaseDIR(path, slash);
		if(fpath == L"")
			break;
		fathers.push_back(fpath);
		path = fpath;
	} while(1);
	return fathers;
}

inline DWORD CreateDIRs(const wstring & path, const wstring & base, wchar_t slash=L'\\')
{
	const wstring & fullpath = base + L'\\' + path;
	DWORD attr = ::GetFileAttributes(fullpath.c_str());
	if(attr != INVALID_FILE_ATTRIBUTES)
		if(FILE_ATTRIBUTE_DIRECTORY & attr)
			return 0;
		else //为文件
			return ERROR_FILE_EXISTS;
	vector<wstring> fathers = GetBaseDIRs(path);
	int i = 0,n = fathers.size();
	for(; i<n; i++)
	{
		const wstring & fa = base + L'\\' + fathers[i];
		attr = ::GetFileAttributes(fa.c_str());
		if(attr != INVALID_FILE_ATTRIBUTES)	//存在
			break;
	}
	for(--i; i>=0; i--)
	{
		const wstring & fa = base + L'\\' + fathers[i];
		if(!::CreateDirectory(fa.c_str(), NULL))
			return GetLastError();
	}
	if(!::CreateDirectory(fullpath.c_str(), NULL))
		return GetLastError();
	return 0;
}

inline bool isChildren(const wstring & father, const wstring & child, wchar_t slash=L'\\')
{
	return child.find(father + slash) == 0; 
}

inline BOOL GetAttribute(LPCTSTR lpFilename, BOOL & isDir, BOOL & isHidden, BOOL & isSystem)
{
	DWORD res = ::GetFileAttributes(lpFilename);
	if(res == INVALID_FILE_ATTRIBUTES)
		return FALSE;
	isDir = res & FILE_ATTRIBUTE_DIRECTORY ? TRUE : FALSE;
	if(isHidden)
		isHidden = res & FILE_ATTRIBUTE_HIDDEN ? TRUE : FALSE;
	if(isSystem)
		isSystem = res & FILE_ATTRIBUTE_SYSTEM ? TRUE : FALSE;
	return TRUE;
}

inline bool file_exists(const wchar_t * path)
{
	return INVALID_FILE_ATTRIBUTES != ::GetFileAttributes(path);
}
inline bool file_exists(const wstring & path)
{
	return INVALID_FILE_ATTRIBUTES != ::GetFileAttributes(path.c_str());
}

//path must exists, or return false
inline bool hasChildren(const wstring & path, bool filt_hidden=true)
{
	WIN32_FIND_DATA findData;
	CString strTemp;
	strTemp.Format(_T("%s\\*.*"), path.c_str());	//查找指定目录下的直接的所有文件和目录

	HANDLE hFile = FindFirstFile(strTemp, &findData);
	if(hFile == INVALID_HANDLE_VALUE)
		return false;
	do
	{
		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)//如果是目录
		{
			if(findData.cFileName[0] == _T('.'))//排除.和..文件夹
				continue;
		}
		if(filt_hidden && (findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN))
			continue;
		::FindClose(hFile);
		return true;
	}while(FindNextFile(hFile, &findData));
	::FindClose(hFile);
	return false;
}

inline bool isSpecial(DWORD attr)
{
	return  attr & FILE_ATTRIBUTE_HIDDEN || attr & FILE_ATTRIBUTE_SYSTEM || attr & FILE_ATTRIBUTE_TEMPORARY;
}

inline bool isDir(DWORD attr)
{
	return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

//#define FILE_ATTRIBUTE_DELETED (FILE_ATTRIBUTE_VIRTUAL << 1)
#define FILE_ATTRIBUTE_DELETED 0x00020000
inline bool isDel(DWORD attr)
{
	return 	(attr & FILE_ATTRIBUTE_DELETED) != 0;
}

inline bool isOffice(const wstring & path)
{
	wstring ext = GetFileExt(path);
	if(ext == L"docx" || ext == L"doc")
		return true;
	if(ext == L"xlsx" || ext == L"xls")	
		return true;
	if(ext == L"pptx" || ext == L"ppt")	
		return true;
	return false;
}
inline bool isFiltType(const wstring & path)
{
	wstring ext = GetFileExt(path);
	if(ext == L"tmp" || ext == L"TMP")	
		return true;
	return false;
}

inline BOOL CreateHiddenDir(const wstring & dirpath)
{
	if((!::CreateDirectory(dirpath.c_str(), NULL)))
	{
		if(ERROR_ALREADY_EXISTS != GetLastError())
		{
			cerr << "Create hidden dirpath fail:" << GetLastError() << endl;
			return FALSE;
		}
	}
	DWORD res = ::GetFileAttributes(dirpath.c_str());
	if(res == INVALID_FILE_ATTRIBUTES)
	{
		cerr << "GetAttribute fail:" << GetLastError() << endl;
		return FALSE;
	}
	else if(!(res & FILE_ATTRIBUTE_DIRECTORY))
	{
		wcerr << L"fail! path: " << dirpath << L" is file" << endl;
		return FALSE;
	}
	else if(!(res & FILE_ATTRIBUTE_HIDDEN))
	{
		::SetFileAttributes(dirpath.c_str(), res | FILE_ATTRIBUTE_HIDDEN);
	}
	return TRUE;
}

//创建office文档文件
int CreateOfficeFile(const wstring & path);

//0:xp 1:win7 2:win8 -1: <xp 
inline int GetWinVersion()
{
	OSVERSIONINFO  osversioninfo = { sizeof(OSVERSIONINFO) };
	GetVersionEx(&osversioninfo);
	DWORD major = osversioninfo.dwMajorVersion;
	DWORD minor = osversioninfo.dwMinorVersion;
	if(major < 5 || (major == 5 && minor == 0))
		return -1;
	if(major == 5 && minor == 1)
		return 0;
	if(major == 6 && minor == 0)	//vista
		return 1;
	if(major == 6 && minor == 1)	//win7
		return 1;
	return 2;
}

inline string NowInString()
{
	SYSTEMTIME sys;
	GetLocalTime(&sys);
	//wsprintf
	char TS[255] = "";
	sprintf_s(TS, 255, "%d-%d-%d %d:%d:%d.%d\t",sys.wYear,sys.wMonth,sys.wDay,sys.wHour,sys.wMinute,sys.wSecond,sys.wMilliseconds);
	return &TS[0];
}

inline void InitCS(CRITICAL_SECTION * cs){
	if(!::InitializeCriticalSectionAndSpinCount(cs, 4000))
		::InitializeCriticalSection(cs);
}

inline void DestCS(CRITICAL_SECTION * cs){
	::DeleteCriticalSection(cs);
}

class CSLock
{
public:
	CSLock(
		 _Inout_ CRITICAL_SECTION& cs,
		 _In_ bool bInitialLock = true,
		 _In_ bool bInitialcs = false);	//初始化，但是析构不释放资源
	~CSLock() throw();
	void Lock();
	void Unlock() throw();
private:
	CRITICAL_SECTION& m_cs;
	bool m_bLocked;
	// Private to avoid accidental use
	CSLock(_In_ const CSLock&) throw();
	CSLock& operator=(_In_ const CSLock&) throw();
};

inline CSLock::CSLock(
			  _Inout_ CRITICAL_SECTION& cs,
			  _In_ bool bInitialLock,
			  _In_ bool bInitialcs):
	m_cs( cs ),
	m_bLocked( false )
{
	if (bInitialcs)
		InitCS(&m_cs);
	if( bInitialLock )
		Lock();
}
inline CSLock::~CSLock() throw()
{
	if( m_bLocked )
		Unlock();
}
inline void CSLock::Lock()
{
	::EnterCriticalSection( &m_cs );
	m_bLocked = true;
}
inline void CSLock::Unlock() throw()
{
	::LeaveCriticalSection( &m_cs );
	m_bLocked = false;
}

#endif
