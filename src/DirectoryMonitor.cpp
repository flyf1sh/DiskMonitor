#include <assert.h>
#include "DirectoryMonitor.h"
#include "DirectoryChangeHandler.h"
#include <stdio.h>
#include <Shellapi.h>
#include <set>
using namespace std;

#define MAX_CACHE_SIZE 10000
#define BLACKLIST_CLEAR_FREQUENCY 1

class NotificationBlacklist_nouse
{
	typedef vector<notification_t> item_type;
	typedef map<wstring, item_type >::iterator iter_type;
public:
	NotificationBlacklist_nouse()
	{ 
		CSLock lock_init(m_guard, false, true);
		m_total = 0;
		m_nclear = 0;
	}
	~NotificationBlacklist_nouse()
	{
		DestCS(&m_guard);
	}
	void Add(const notification_t & no)
	{
		CSLock lock(m_guard);
		const wstring & pathKey = no.path;
		iter_type it = m_blist.find(pathKey);
		if(it != m_blist.end())
		{//find it, add in vector
			it->second.push_back(no);
			return;
		}
		item_type v;
		v.push_back(no);
		m_blist[pathKey] = v;
		m_total += 1;
	}
	bool Query(const notification_t & no, bool ClearWhenHit=true)
	{
		CSLock lock(m_guard);
		if(m_total == 0)
			return false;
		return QueryList(no, ClearWhenHit, m_blist) || QueryList(no, ClearWhenHit, m_blist_gen2);
	}
	void Clear()
	{
		if(++m_nclear != BLACKLIST_CLEAR_FREQUENCY)
			return;
		m_nclear = 0;
		CSLock lock(m_guard);
		m_blist_gen2.clear();
		m_blist.swap(m_blist_gen2);
		m_total = m_blist_gen2.size();
	}
private:
	//采取双资源模式, 这样可以有效的清理old 请求
	map<wstring, item_type > m_blist;
	map<wstring, item_type > m_blist_gen2;
	int m_total, m_nclear;
	CRITICAL_SECTION m_guard;
private:
	bool QueryList(const notification_t & no, bool ClearWhenHit, map<wstring, item_type > & bl)
	{
		if(bl.empty())
			return false;
		const wstring & pathKey = no.path;
		iter_type it = bl.find(pathKey);
		if(it == bl.end())
			return false;
		item_type & v = it->second;
		for(item_type::iterator it2 = v.begin(); it2 != v.end(); ++it2)	
		{
			if(isMatch(no, *it2))
			{
				if(!ClearWhenHit) return true;
				if(v.size() == 1)
				{
					bl.erase(it);
					m_total--;
					assert(m_total = m_blist.size() + m_blist_gen2.size());
				}
				else
					v.erase(it2);
				return true;
			}
		}
		return false;
	}
	bool isMatch(const notification_t & q, const notification_t & aim)
	{
		return q == aim;	
	}
};

struct BlacklistItem {
	BlacklistItem(DWORD _act, const wstring & p1, const wstring & p2=wstring())
		:act(_act), path(p1), path2(p2) { }
	DWORD act;
	wstring path;
	wstring path2;
};
bool operator == (const struct BlacklistItem & t1, const struct BlacklistItem & t2)
{
	return t1.act == t2.act && t1.path == t2.path && t1.path2 == t2.path2;
}
bool operator < (const struct BlacklistItem & t1, const struct BlacklistItem & t2)
{
	if(t1.act < t2.act) return true;
	if(t1.act > t2.act) return false;
	if(t1.path < t2.path) return true;
	if(t1.path > t2.path) return false;
	if(t1.path2 < t2.path2) return true;
	if(t1.path2 > t2.path2) return false;
	return false;
}

class NotificationBlacklist
{
	typedef struct BlacklistItem item_type;
	typedef set<item_type >::iterator iter_type;
public:
	NotificationBlacklist()
	{ 
		CSLock lock_init(m_guard, false, true);
		m_nclear = 0;
	}
	~NotificationBlacklist()
	{
		DestCS(&m_guard);
	}
	void Add(DWORD act, const wstring & path, const wstring & path2)
	{
		CSLock lock(m_guard);
		m_blist.insert(BlacklistItem(act, path, path2));
	}
	void Add(const BlacklistItem & item)
	{
		CSLock lock(m_guard);
		m_blist.insert(item);
		assert(item.act < FILE_ACTION_END);
		cout << "blacklist add nofity:(act:" << item.act 
			<< ", path:" << WideToMutilByte(item.path)<< ", path2:" << WideToMutilByte(item.path2) << ")" << endl;
	}
	bool Del(const BlacklistItem & item)
	{
		CSLock lock(m_guard);
		return QueryList(item, true, m_blist) || QueryList(item, true, m_blist_gen2);
	}
	bool Query(DWORD act, const wstring & path, const wstring & path2, bool ClearWhenHit=true)
	{
		CSLock lock(m_guard);
		if(m_blist.empty() && m_blist_gen2.empty())
			return false;
		const BlacklistItem & item = BlacklistItem(act, path, path2);
		return QueryList(item, ClearWhenHit, m_blist) || QueryList(item, ClearWhenHit, m_blist_gen2);
	}
	//timeout 时候调用，可以控制频率
	void Clear()
	{
		if(++m_nclear != BLACKLIST_CLEAR_FREQUENCY)
			return;
		m_nclear = 0;
		CSLock lock(m_guard);
		if(!m_blist_gen2.empty())
		{
			int i=1;
			cout << "blacklist not empty when clear!" << endl;
			for(iter_type it = m_blist_gen2.begin(); it != m_blist_gen2.end(); ++it, ++i)
				cout << "items: " << i << ":act = " << it->act << " path= " << WideToMutilByte(it->path)
					<< " path2=" << WideToMutilByte(it->path2) << endl;
		}
		m_blist_gen2.clear();
		m_blist.swap(m_blist_gen2);
	}
private:
	//采取双资源模式, 这样可以有效的清理old 请求, 另一个原因是
	//timeout处理线程是另一个线程，不知道它调用的周期，所以可能刚刚
	//设置就有可能被Clear(),所以这里就提供了至少一个周期的缓冲时间
	set<item_type > m_blist;
	set<item_type > m_blist_gen2;
	CRITICAL_SECTION m_guard;
	int m_nclear;	//clear的周期累计次数，用来控制名单的寿命
private:
	bool QueryList(const item_type & item, bool ClearWhenHit, set<item_type > & bl)
	{
		if(bl.empty())
			return false;
		iter_type it = bl.find(item);
		if(it == bl.end())
			return false;
		if(ClearWhenHit)
			bl.erase(it);
		return true;
	}
};

class FileSystemHelper
{
public:
	static wstring GetThreadTempdir(const wstring & homew)
	{
		DWORD threadid = ::GetCurrentThreadId();
		wchar_t _buf[MAX_PATH + 1];
		wstring hidden_dir = homew + L'\\' + TEMP_DIRNAMEW;
		swprintf_s(_buf, MAX_PATH, L"%s\\%ld", hidden_dir.c_str(), threadid);
		return _buf;
	}

	static DWORD CreateThreadTempdir(const wstring & thread_dir)
	{
		DWORD attr = ::GetFileAttributes(thread_dir.c_str());
		bool creat_thread_dir = false;
		if( INVALID_FILE_ATTRIBUTES == attr )//不存在
			creat_thread_dir = true;
		else if(!(FILE_ATTRIBUTE_DIRECTORY & attr))
		{//文件，删除
			::DeleteFile(thread_dir.c_str());
			creat_thread_dir = true;
		}
		if(creat_thread_dir)
		{
			if(!(0 != ::CreateDirectory(thread_dir.c_str(), NULL) &&
				 0 != ::CreateDirectory((thread_dir+L"\\tmp").c_str(), NULL)))
			{
				DWORD err = GetLastError();
				cout << "Create hidden dirpath fail:" << err << endl;
				return err;
			}
		}
		return 0;
	}

	static DWORD DoActWithoutNotify_impl(int act, const wstring & homew, DWORD flag, bool isAbsPath, bool isXP,
										 const wstring & wstr_from, const wstring & wstr_to, 
										 const wstring & wstr_from_full, 
										 const wstring & wstr_from_name, const wstring & wstr_to_name)
	{
		DWORD err = 0;
		int step = 0;
		bool isOfficefile = isOffice(wstr_from);
		wstring wstr_to_full;

		//对于这些操作，由于多线程的关系，我们应该防止重名文件的冲突
		//那可以根据线程id，创建文件夹，这个线程的操作都在这个文件夹里

		//先找到对应线程id的目录
		wstring thread_dir = GetThreadTempdir(homew);
		if(err = CreateThreadTempdir(thread_dir))
			return err;

		//对于移动操作,如果是文件，目标地址如果不存在，建中间文件夹，（不同名改名）移动到对应位置
		//如果对应位置有文件，覆盖，如果对应地址有同名文件夹, 移动到该文件夹下。
		//对于目录的规则和文件一样，目标地址如果不存在，建中间文件夹，（不同名改名）移动到对应位置
		//现在目标地址存在,当做父路径处理，如果是目录，移动到这个目录下，如果目录下有同名目录，合并，有同名文件，报错126；
		//如果存在的是文件,报错 126
		//(这么说，首先这个TO地址，如果是存在的目录，那么移动到改目录下，如果目录下有同名的冲突--不同类，那么报错, 如果同类就覆盖。
		//如果存在的是文件，那么移动文件就是覆盖，移动目录就是报错。如果TO地址不存在，那么都当移动改名处理)
		SHFILEOPSTRUCT FileOp; 
		ZeroMemory((void*)&FileOp,sizeof(SHFILEOPSTRUCT));
		//这里可能会被卡住，因为由于没有出错的UI信息，可能会照成混乱，还是要用 FOF_NO_UI
		FileOp.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR;	//FOF_NOCONFIRMMKDIR 这个有作用？
		FileOp.fFlags = FOF_NO_UI;



		//第一步：预先处理, 一般是移入到隐藏目录里
		//XXX 在移入的时候直接改名, 但是注意内部的情况
		step = 1;
		bool inner_moverename = false;
		if(!isAbsPath && wstr_to_name != wstr_from_name)
			inner_moverename = true;
		wstring inner_path = thread_dir + L'\\' + (inner_moverename ? wstr_from_name : wstr_to_name) + L'\0';
		wstring inner_path2;
		wstring thread_dir2 = thread_dir + L'\0';
		bool isfile = true;
		DWORD attr = ::GetFileAttributes(wstr_from_full.c_str());
		if(attr != INVALID_FILE_ATTRIBUTES && FILE_ATTRIBUTE_DIRECTORY & attr)
			isfile = false;
		if(file_exists(inner_path.c_str()))
		{//删除
			FileOp.pFrom = inner_path.c_str();
			FileOp.wFunc = FO_DELETE; 
			::SHFileOperation(&FileOp);
		}

		switch(act)
		{
		case 1:	//新增文件
			break;
		case 2:	//新增文件夹
			isfile = false;
			if(attr != INVALID_FILE_ATTRIBUTES)
				if(FILE_ATTRIBUTE_DIRECTORY & attr)
					return 0;
				else //为文件
					return ERROR_FILE_EXISTS;
			break;
		case 0:	//删除
		case 3:	//移动
		case 4:	//改名
		case 5:	//拷贝
			if(attr == INVALID_FILE_ATTRIBUTES)
				return ERROR_FILE_EXISTS;
			FileOp.pFrom = wstr_from_full.c_str(); 
			FileOp.pTo = inner_path.c_str();	//移动，附带了重命名
			FileOp.wFunc = act==5?FO_COPY:FO_MOVE; 
			err = ::SHFileOperation(&FileOp);
			if(err) goto fail;
			break;
		}

		step = 2;
		//第二步：做处理 
		switch(act)
		{
		case 0:	//删除	
			FileOp.pFrom = inner_path.c_str();
			FileOp.wFunc = FO_DELETE; 
			if(flag & FOP_RECYCLE) //回收站
				FileOp.fFlags |= FOF_ALLOWUNDO; 
			err = ::SHFileOperation(&FileOp);
			FileOp.fFlags &= ~FOF_ALLOWUNDO; 
			break;
		case 1:	//新增文件
			if(isOfficefile)
			{
				err = CreateOfficeFile(inner_path.c_str()); //不能直接用inner_path, 因为末尾多了个\0
			}
			else
			{
				HANDLE filehd = ::CreateFile(
											 inner_path.c_str(),					// pointer to the file name
											 GENERIC_READ | GENERIC_WRITE,       // access (read/write) mode
											 FILE_SHARE_READ						// share mode
											 | FILE_SHARE_WRITE
											 | FILE_SHARE_DELETE,
											 NULL,                               // security descriptor
											 CREATE_NEW,							// how to create
											 FILE_ATTRIBUTE_NORMAL,				// file attributes
											 NULL);                              // file with attributes to copy
				if(filehd == INVALID_HANDLE_VALUE)
					err = GetLastError();
				else
					CloseHandle(filehd);
			}
			break;
		case 2:	//新增文件夹
			if(!::CreateDirectory(inner_path.c_str(), NULL))
				err = GetLastError();
			break;
		case 3:	//移动
		case 5:	//拷贝
			if(!inner_moverename)
				break;
		case 4:	//改名
			inner_path2 = thread_dir + L'\\' + wstr_to_name + L'\0';
			FileOp.pFrom = inner_path.c_str();
			FileOp.pTo = inner_path2.c_str();
			FileOp.wFunc = FO_RENAME; 
			err = ::SHFileOperation(&FileOp);
			inner_path = inner_path2;
			break;
		}
		if(err) goto fail;

		//第三步：后续处理，移出
		step = 3;
		switch(act)
		{
		case 0:	//删除
			break;
		case 1:	//新增文件
		case 2:	//新增文件夹
			assert(wstr_to.empty());
		case 3:	//移动
		case 4:	//改名
		case 5:	//拷贝
			wstr_to_full = homew + L'\\' + (wstr_to.empty() ? wstr_from : wstr_to) + L'\0';
			if(!isfile)
			{
				if((flag & (FOP_REPLACE | FOP_IGNORE_EXIST)) && file_exists(wstr_to_full.c_str()))
				{//删除old目录
					inner_path2 = thread_dir + L"\\tmp\\" + L'\0';
					FileOp.pFrom = wstr_to_full.c_str(); 
					FileOp.pTo = inner_path2.c_str();
					FileOp.wFunc = FO_MOVE; 
					err = ::SHFileOperation(&FileOp);
					if(err) goto fail;
					inner_path2 = thread_dir + L"\\tmp\\" + wstr_to_name + L'\0';
					FileOp.pFrom = inner_path2.c_str();
					FileOp.pTo = NULL;
					FileOp.wFunc = FO_DELETE; 
					err = ::SHFileOperation(&FileOp);
					if(err) goto fail;
				}
				//移动和拷贝需要是父路径存在
				if(file_exists(GetBaseDIR(wstr_to_full)))
					wstr_to_full = GetBaseDIR(wstr_to_full) + L'\0';	
			}
			else
			{//虽然可能碰到 topath这里有个同名的目录，导致移动到这个目录下了
				//但是我们相信调用者会判断这个冲突，要不我怎么办？删除这个目录？
				if(isXP && file_exists(wstr_to_full.c_str()))
				{//xp下覆盖同名文件会先触发删除文件消息
					//TODO 在消息过滤时候考虑过滤掉
					inner_path2 = thread_dir + L"\\tmp\\" + wstr_to_name + L'\0';
					FileOp.pFrom = wstr_to_full.c_str(); 
					FileOp.pTo = inner_path2.c_str();
					FileOp.wFunc = FO_MOVE; 
					err = ::SHFileOperation(&FileOp);
					if(err) goto fail;
					FileOp.pFrom = inner_path2.c_str();
					FileOp.pTo = NULL;
					FileOp.wFunc = FO_DELETE; 
					err = ::SHFileOperation(&FileOp);
					if(err) goto fail;
				}
			}
			FileOp.pFrom = inner_path.c_str();
			FileOp.pTo = wstr_to_full.c_str(); 
			FileOp.wFunc = FO_MOVE; 
			err = ::SHFileOperation(&FileOp);
			break;
		}
		if(err == 0) return 0;

fail:	//FIXME 没有完全的逆过程
		if(!inner_path.empty() && file_exists(inner_path.c_str()))
		{
			FileOp.pFrom = inner_path.c_str();
			FileOp.wFunc = FO_DELETE; 
			::SHFileOperation(&FileOp);
		}
		return err;
	}
};

DirectoryMonitor::DirectoryMonitor(DirectoryChangeHandler * dc, 
								   const string & path, 
								   const string & type, 
								   void (*cbp)(void *arg, LocalNotification *), void * varg):
	m_dc(dc), m_home(path), m_type(type), m_cbp(cbp), m_varg(varg), m_running(0), m_id(-1), m_expert_act(0),
	m_guess_cnt(0), m_showfilt(false)
{
	MutilByteToWide(m_home, m_homew);
	//TODO:动态生成屏蔽目录
	m_silent_dir = TEMP_DIRNAMEW;
	CreateHiddenDir(m_homew + L'\\' + m_silent_dir);
	m_running = m_dc->AddDirectory(this)?1:-1;
	m_blacklist = new NotificationBlacklist();
	m_isXP = GetWinVersion() == 0;
}

DirectoryMonitor::~DirectoryMonitor() {
	dlog("in ~DirectoryMonitor()");
	Terminate(); 
	delete m_blacklist;
}

int DirectoryMonitor::Terminate()
{
	dlog("in DirectoryMonitor::Terminate()");
	DWORD err = 0;
	if(m_running >= 0)
	{
		m_running = -1;
		m_dc->DelDirectory(this);	

		//XXX clear hidden dir。 放到异步线程中去？？
		SHFILEOPSTRUCT FileOp; 
		ZeroMemory((void*)&FileOp,sizeof(SHFILEOPSTRUCT));
		wstring hidden_dir = m_homew + L'\\' + m_silent_dir + L'\0';
		FileOp.fFlags = FOF_NO_UI; //FOF_NOCONFIRMATION | FOF_NOERRORUI; 
		FileOp.pFrom = hidden_dir.c_str();
		FileOp.wFunc = FO_DELETE; 
		err = ::SHFileOperation(&FileOp);
		if(err)
			cout << "clear hidden dirpath fail:" << err << endl;
	}
	return (int)err;
}

void DirectoryMonitor::ClearBlacklist()
{
	m_blacklist->Clear();
}


DWORD DirectoryMonitor::DoActWithoutNotify2(int act, const string & from, const string & to, 
											DWORD flag, int * LPtrCancel)
{
	char msgbuf[1024];
	sprintf_s(msgbuf, 1024, "call DoActWithoutNotify: act=%d ,from = %s, to = %s", act, from.c_str(), to.c_str());
	dlog(&msgbuf[0]);

	wstring wstr_from, wstr_to, 
			wstr_from_full,  wstr_from_name, wstr_to_name;
	DWORD err = 0;

	MutilByteToWide(from, wstr_from);
	RegularPath(wstr_from);
	wstr_from_name = GetFileName(wstr_from);

	if(act == 3 || act == 4 || act == 5)
	{
		assert(!to.empty());
		MutilByteToWide(to, wstr_to);
		RegularPath(wstr_to);
		wstr_to_name = GetFileName(wstr_to);
	}
	else
		wstr_to_name = wstr_from_name;

	bool isAbsPath = from.find(':') == string::npos ? false : true;
	if(isAbsPath)
	{
		assert(act == 3 || act == 5);
		wstr_from_full = wstr_from + L'\0';		//XXX 必须这里加\0，不能用临时变量
	}
	else
		wstr_from_full = m_homew + L'\\' + wstr_from + L'\0';

	err = FileSystemHelper::DoActWithoutNotify_impl(act, m_homew, flag, isAbsPath, m_isXP, 
													wstr_from, wstr_to,  wstr_from_full, 
													wstr_from_name, wstr_to_name);
	if(err)
	{
		sprintf_s(msgbuf, 1024, "meet error in DoActWithoutNotify: error code:%d, act=%d ,from = %s, to = %s", 
				  err, act, from.c_str(), to.c_str());
		dlog(&msgbuf[0]);
	}
	return err;
}


/* windows API 操作规则
 *
 * 对于移动操作,如果是文件，目标地址如果不存在，建中间文件夹，（不同名改名）移动到对应位置
 如果对应位置有文件，覆盖，如果对应地址有同名文件夹, 移动到该文件夹下。

 * 对于目录的规则和文件一样，目标地址如果不存在，建中间文件夹，（不同名改名）移动到对应位置
 现在目标地址存在,当做父路径处理:
 如果是目录，移动到这个目录下，如果目录下有同名目录，合并，有同名文件，报错126；
 如果存在的是文件,报错 126
 * (这么说，首先这个TO地址，如果是存在的目录，那么移动到改目录下，如果目录下有同名的冲突--不同类，那么报错,
 如果同类就覆盖。如果存在的是文件，那么移动文件就是覆盖，移动目录就是报错。如果TO地址不存在，那么都当移动改名处理)
 */

//@retrun: 成功返回0， 失败返回错误码
//@param: act = 0 删除；1 新增文件； 2 新增文件夹； 3 移动； 4 改名； 5 拷贝
//@param: from 操作的文件/夹。(监控外为全路径)
//@param: to   目标地址。 (带from的文件名/目录名的地址)
//@param: flag	== FOP_REPLACE，表示移动或者拷贝过程中，如果目标文件夹存在，替换掉而不是默认的合并。
//				== FOP_RECYCLE 表示删除时候放入回收站

//文件的移动和拷贝，目标为文件路径，且移动是可以改变文件名
//TODO: 回收站用特殊的消息屏蔽，因为从回收站还原需要在原地还原(需要过滤列表支持)
//TODO: 失败rollback怎么办？
//TODO: 如果删除一个目录的话，那么to最好给一个监控外的临时目录

DWORD DirectoryMonitor::DoActWithoutNotify(int act, const string & from, const string & to, 
										   DWORD flag, int * LPtrCancel)
{
	char msgbuf[1024];
	sprintf_s(msgbuf, 1024, "call DoActWithoutNotify: act=%d ,from = %s, to = %s", act, from.c_str(), to.c_str());
	dlog(&msgbuf[0]);

	wstring wstr_from, wstr_to, 
			wstr_from_full, wstr_to_full, 
			wstr_from_name, wstr_to_name;
	DWORD err = 0;

	MutilByteToWide(from, wstr_from);
	RegularPath(wstr_from);
	wstr_from_name = GetFileName(wstr_from);

	if(act == 3 || act == 4 || act == 5)
	{
		assert(!to.empty());
		MutilByteToWide(to, wstr_to);
		RegularPath(wstr_to);
		wstr_to_name = GetFileName(wstr_to);
	}
	else
		wstr_to_name = wstr_from_name;

	bool isAbsPath = from.find(':') == string::npos ? false : true;
	if(isAbsPath)
	{
		assert(act == 3 || act == 5);
		wstr_from_full = wstr_from + L'\0';		//XXX 必须这里加\0，不能用临时变量
	}
	else
		wstr_from_full = m_homew + L'\\' + wstr_from + L'\0';
	wstr_to_full = m_homew + L'\\' + (wstr_to.empty() ? wstr_from : wstr_to) + L'\0';

	bool isOfficefile = isOffice(wstr_from);
	bool dest_is_exist = file_exists(wstr_to_full.c_str());
	bool isfile = true;

	SHFILEOPSTRUCT FileOp; 
	ZeroMemory((void*)&FileOp,sizeof(SHFILEOPSTRUCT));
	//这里可能会被卡住，因为由于没有出错的UI信息，可能会照成混乱，还是要用 FOF_NO_UI
	FileOp.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR;	//FOF_NOCONFIRMMKDIR 这个有作用？
	FileOp.fFlags = FOF_NO_UI;

	BlacklistItem bl_item(0, wstr_from, wstr_to);
	DWORD attr = ::GetFileAttributes(wstr_from_full.c_str());
	if(attr != INVALID_FILE_ATTRIBUTES && FILE_ATTRIBUTE_DIRECTORY & attr)
		isfile = false;

	DWORD action = 0;
	switch(act)
	{
	case 0:	//删除
		bl_item.act = FILE_ACTION_REMOVED;
		m_blacklist->Add(bl_item);
		if(isfile || flag & FOP_RECYCLE)	//删除文件
		{
			FileOp.pFrom = wstr_from_full.c_str();
			FileOp.wFunc = FO_DELETE; 
			if(flag & FOP_RECYCLE) //回收站
				FileOp.fFlags |= FOF_ALLOWUNDO; 
			err = ::SHFileOperation(&FileOp);
			//FileOp.fFlags &= ~FOF_ALLOWUNDO; 
		}
		else	//删除目录，因为有孩子，所以不走黑名单
		{
			goto do_in_tempdir;
		}
		break;
	case 1:	//新增文件
		if(isOfficefile)
		{
			//office 因为临时文件有两个操作
			//TODO:XP 导致的?
			if(m_isXP)
			{
				bl_item.act = FILE_REMOVED;
				m_blacklist->Add(bl_item);
			}
			bl_item.act = FILE_ADDED;
			m_blacklist->Add(bl_item);
			err = CreateOfficeFile(wstr_from_full.c_str());		//不能直接用因为末尾多了个\0
			if(err && m_isXP)
			{
				bl_item.act = FILE_REMOVED;
				m_blacklist->Del(bl_item);
				bl_item.act = FILE_ADDED;
			}
			if(err == ERROR_ALREADY_EXISTS)
			{
				m_blacklist->Del(bl_item);
				return 0;
			}
		}
		else
		{
			bl_item.act = FILE_ACTION_ADDED;
			m_blacklist->Add(bl_item);
			HANDLE filehd = ::CreateFile(
										 wstr_from_full.c_str(),				// pointer to the file name
										 GENERIC_READ | GENERIC_WRITE,			// access (read/write) mode
										 FILE_SHARE_READ						
										 | FILE_SHARE_WRITE
										 | FILE_SHARE_DELETE,					// share mode
										 NULL,									// security descriptor
										 //CREATE_AWAYLS,	//覆盖?
										 CREATE_NEW,							// how to create
										 FILE_ATTRIBUTE_NORMAL,					// file attributes
										 NULL);									// file with attributes to copy
			if(filehd == INVALID_HANDLE_VALUE)
				err = GetLastError();
			else
				CloseHandle(filehd);
		}
		break;
	case 2:	//新增文件夹
		if(attr != INVALID_FILE_ATTRIBUTES)
			if(FILE_ATTRIBUTE_DIRECTORY & attr)
				return 0;
			else //为文件
				return ERROR_FILE_EXISTS;
		bl_item.act = FILE_ACTION_ADDED;
		{
			vector<wstring> fathers = GetBaseDIRs(wstr_from);
			int i=0;
			for(; i<(int)fathers.size(); i++)
			{
				const wstring & fa = m_homew + L'\\' + fathers[i];
				attr = ::GetFileAttributes(fa.c_str());
				if(attr != INVALID_FILE_ATTRIBUTES)	//存在
					break;
			}
			for(--i; i>=0; i--)
			{
				const wstring & fa = m_homew + L'\\' + fathers[i];
				bl_item.path = fathers[i];
				m_blacklist->Add(bl_item);
				if(!::CreateDirectory(fa.c_str(), NULL))
					return GetLastError();
			}
			bl_item.path = wstr_from;
			m_blacklist->Add(bl_item);
			if(!::CreateDirectory(wstr_from_full.c_str(), NULL))
				return GetLastError();
		}
		break;
	case 3:	//移动
		if(attr == INVALID_FILE_ATTRIBUTES)
			return ERROR_FILE_EXISTS;
		//考虑目标存在不？
		if(dest_is_exist && !isfile)
		{
			if(flag & FOP_REPLACE)
			{//删除old目录
				wstring thread_dir = FileSystemHelper::GetThreadTempdir(m_homew);
				if(false == file_exists(thread_dir))
				{
					if(err = FileSystemHelper::CreateThreadTempdir(thread_dir))
						return err;
				}
				m_blacklist->Add(BlacklistItem(FILE_ACTION_REMOVED, wstr_to));
				wstring inner_path2 = thread_dir + L"\\tmp\\" + L'\0';
				FileOp.pFrom = wstr_to_full.c_str(); 
				FileOp.pTo = inner_path2.c_str();
				FileOp.wFunc = FO_MOVE; 
				err = ::SHFileOperation(&FileOp);
				if(err) goto fail;
				//删除掉
				inner_path2 = thread_dir + L"\\tmp\\" + wstr_to_name + L'\0';
				FileOp.pFrom = inner_path2.c_str();
				FileOp.pTo = NULL;
				FileOp.wFunc = FO_DELETE; 
				err = ::SHFileOperation(&FileOp);
				if(err)
				{//但是还继续做
					sprintf_s(msgbuf, 1024, "meet error in DoActWithoutNotify delete tempfile: error code:%d, act=%d ,from = %s, to = %s", err, act, from.c_str(), to.c_str());
					dlog(&msgbuf[0]);
				}
			}
			else
			{//覆盖
				return ERROR_FILE_EXISTS;	//FIXME 存在目录目前先这么处理吧
				//目录的目标存在的话, 按照规则取父路径,做windows默认的合并
				//wstr_to_full = GetBaseDIR(wstr_to_full.c_str()) + L'\0';
			}
		}

		if(!isAbsPath)
		{//监控内移动
			bl_item.act = FILE_ACTION_REMOVED;
			bl_item.path2 = wstring();
			m_blacklist->Add(bl_item);
		}

		action = FILE_ACTION_ADDED;
		if(dest_is_exist && isfile)
		{
			if(m_isXP)
				m_blacklist->Add(BlacklistItem(FILE_ACTION_REMOVED, wstr_to));	//xp 的覆盖会先发删除消息
			else 
				action = FILE_ACTION_MODIFIED;
		}
		m_blacklist->Add(BlacklistItem(action, wstr_to));

		FileOp.pFrom = wstr_from_full.c_str(); 
		FileOp.pTo = wstr_to_full.c_str();	//移动，附带了重命名
		FileOp.wFunc = FO_MOVE; 
		err = ::SHFileOperation(&FileOp);
		break;
	case 4:	//改名
		if(attr == INVALID_FILE_ATTRIBUTES)
			return ERROR_FILE_EXISTS;
		//XXX 不允许存在改名的目标项
		if(dest_is_exist)
			return ERROR_FILE_EXISTS;
		if(!isfile)
		{
			bl_item.act = DIR_RENAMED;
			m_blacklist->Add(bl_item);
		}
		//bl_item.act = isfile ? FILE_RENAMED : DIR_RENAMED;	//其实这里判断够了
		bl_item.act = FILE_RENAMED;	//防止意外多加一个文件改名(很小的几率, 但是改名这个操作很保险，所以多加一个判断无所谓)
		m_blacklist->Add(bl_item);
		FileOp.pFrom = wstr_from_full.c_str(); 
		FileOp.pTo = wstr_to_full.c_str();	//移动，附带了重命名
		FileOp.wFunc = FO_MOVE; 
		err = ::SHFileOperation(&FileOp);
		break;
	case 5:	//拷贝
		if(attr == INVALID_FILE_ATTRIBUTES)
			return ERROR_FILE_EXISTS;
		bl_item.act = FILE_ACTION_ADDED;
		bl_item.path = wstr_to;
		bl_item.path2 = wstring();
		m_blacklist->Add(bl_item);
		goto do_in_tempdir;
		break;
	}
fail:
	if(err)
		m_blacklist->Del(bl_item);
	return err;
do_in_tempdir:
	err = FileSystemHelper::DoActWithoutNotify_impl(act, m_homew, flag, isAbsPath, m_isXP, 
													wstr_from, wstr_to,  wstr_from_full, 
													wstr_from_name, wstr_to_name);
	if(err)
	{
		sprintf_s(msgbuf, 1024, "meet error in DoActWithoutNotify: error code:%d, act=%d ,from = %s, to = %s", 
				  err, act, from.c_str(), to.c_str());
		dlog(&msgbuf[0]);
	}
	return err;
}

void DirectoryMonitor::UpdateAttributeCache(const wstring & rpathw)
{
	DWORD attr = ::GetFileAttributes((m_homew + L"/" + rpathw).c_str());
	if(attr == INVALID_FILE_ATTRIBUTES)
		attr = FILE_ATTRIBUTE_DELETED;
	map<wstring, DWORD>::iterator it = m_file_attrs.find(rpathw);
	if(it != m_file_attrs.end())
		attr = it->second | FILE_ATTRIBUTE_DELETED;
	m_file_attrs[rpathw] = attr;
}

DWORD DirectoryMonitor::GetAttributeFromCache(const wstring & rpathw, DWORD act, DWORD & self)
{
	bool isdel = ((act == FILE_ACTION_REMOVED)||(act == FILE_ACTION_RENAMED_OLD_NAME));
	//如果是new说明现在cache里如果存在那肯定是失效了的
	bool isnew = (act == FILE_ACTION_ADDED || act == FILE_ACTION_RENAMED_NEW_NAME);
	DWORD ret, f_attr, attr = 0;	//表示为删除了
	map<wstring, DWORD>::iterator it = m_file_attrs.find(rpathw);
	if(it != m_file_attrs.end())
	{
		if(isdel)
			it->second |= FILE_ATTRIBUTE_DELETED;
		if(!isnew) //新增了那就要更新这条路径上的所有记录
			return it->second;
	}

	//没找着或者是新的
	//map<wstring, DWORD> rec;
	bool meet_special = false;
	if(isdel)
		attr = FILE_ATTRIBUTE_DELETED;
	else{
		attr = ::GetFileAttributes((m_homew + L"/" + rpathw).c_str());
		if(attr == INVALID_FILE_ATTRIBUTES)
			attr = FILE_ATTRIBUTE_DELETED;
	}
	self = ret = attr;

	bool invalid = false;
	vector<wstring> fathers = GetBaseDIRs(rpathw);
	size_t s = fathers.size();
	for(int i = s-1; i >= 0; i--)	//顶到底
	{
		const wstring & fpath = fathers[i];
		it = m_file_attrs.find(fpath);
		if(!invalid)
		{
			if(it == m_file_attrs.end() || (isnew && isDel(it->second)))	//没找到或者新增但是碰到了删除的那个父目录
				invalid = true;	//这时候就要get attr
		}
		if(invalid) {	
			//没用f_attr_old, 因为无效了
			//f_attr_old = it != m_file_attrs.end() ? it->second : 0;
			f_attr = ::GetFileAttributes((m_homew + L"/" + fpath).c_str());
			if(f_attr == INVALID_FILE_ATTRIBUTES)
				f_attr = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DELETED;
			if(!meet_special)
				meet_special = isSpecial(f_attr);	//发现是special文件夹
			else
				f_attr |= FILE_ATTRIBUTE_HIDDEN;
			m_file_attrs[fpath] = f_attr;
		}else{
			assert(it != m_file_attrs.end());
			if(!meet_special)
				meet_special = isSpecial(it->second);
		}
	}
	if(meet_special)
		ret |= FILE_ATTRIBUTE_HIDDEN;
	m_file_attrs[rpathw] = ret;
	//m_file_attrs.insert(rec.begin(), rec.end()); //insert不会插入已存在的item
	return ret;
}

int DirectoryMonitor::GetNotify(struct TDirectoryChangeNotification & notify)
{
	wstring rpathw(notify.rPath.GetBuffer());

	//查找是否有attr
	//bool isdir = notify.type == DIR_TYPE;
	bool isdir = false;
	DWORD self_attr = -1;
	DWORD attr = GetAttributeFromCache(rpathw, notify.dwAct, self_attr);
	if(isDir(attr))
	{
		isdir = true;
		//cout << "path:" << WideToMutilByte(rpathw) << " is dir" << endl;
	}

	notification_t no(notify.dwAct, rpathw, isdir);
	no.attr = self_attr;
	no.exist = !isDel(attr);
	if(isSpecial(attr) && !isSpecial(self_attr))
		no.fspec = true;
	if(isSpecial(attr) || isFiltType(rpathw))
	{
		no.special = true;
		//no.filted = true;	//XXX 不要设置，因为这个是用来判断行为的，最后对special进行过滤
	}
	cout << "path:" << WideToMutilByte(no.path) << " is " << \
	(no.special? "special":"normal") <<	(no.fspec? " ,father is special" : " ,father is OK") << \
		", file attr:" << attr << " , exist:" << no.exist << " ,act:" << notify.dwAct<< endl;

	if(m_blacklist->Query(no.act, no.path, no.path2))
	{
		cout << "blacklist filt nofity:(act:" << no.act << ", path:" << WideToMutilByte(no.path) << ")" << endl;
		return 0;
	}
	if(!filt_notify2(no))
		m_notifications.push_back(no);

	release_resource();
	return 0;
}

#define none_op	0
#define maybe_move_dir	1
#define maybe_move_file	2
#define maybe_out_move_dir	3
#define maybe_copy_dir	4


//实际上也就是几个地方要猜想
bool DirectoryMonitor::guess(notification_t & notify)
{
	const wstring & path = notify.path;
	DWORD act = notify.act;
	bool isdir = notify.isdir;
	m_expert_act = none_op;
	m_expert_path = L"";
	m_guess_cnt = 0;
	switch (act)
	{
	case FILE_ACTION_ADDED:
		if(isdir)
		{
			notify.expert_act = m_expert_act = maybe_out_move_dir;
			m_expert_path = notify.path;
			m_guess_cnt = 0;
			return true;
		}
		break;
	case FILE_ACTION_MODIFIED:
		break;
	case FILE_ACTION_REMOVED:
		notify.expert_act = m_expert_act = notify.isdir ? maybe_move_dir : maybe_move_file;
		m_expert_path = notify.path;
		m_guess_cnt = 0;
		return true;
		break;
	case FILE_ACTION_RENAMED_OLD_NAME:
		break;
	case FILE_ACTION_RENAMED_NEW_NAME:
		break;
	}
	return false;
}

//返回是否加入这个filter 
//如果要加入判断队列，返回 true; 如果要过滤它直接filter.filted = true
bool DirectoryMonitor::filt_old_notify(notification_t & filter, int advance)
{
	const wstring & path = filter.path;
	const wstring & path2 = filter.path2;

	vector<notification_t>::reverse_iterator it = m_notifications.rbegin(), endit = m_notifications.rend();
	while(advance--) it++;

	int change_act2 = 0;
	bool not_cmp = false;
	switch (filter.act)
	{
	case FILE_ACTION_ADDED:
	case DIR_ADDED:
		break;
	case FILE_ACTION_MODIFIED:
		for(; it != endit; ++it)
		{
			if(it->filted || it->path != path) continue;
			if(it->act == FILE_ACTION_MODIFIED || it->act == FILE_ACTION_ADDED)
			{//保留最早的那个通知(可以根据add来处理rename操作)
				filter.filted = true; //debug 清晰些
				return true;
				// return false;	//XXX 不加入, 最后用这个
			}
		}
		break;
	case FILE_REMOVED:
	case DIR_REMOVED:
	case FILE_ACTION_REMOVED:
		//for file 过滤修改的通知
		if(!filter.isdir)
		{
			for(; it != endit; ++it)
			{
				if(it->filted || it->path != path) continue;
				//这里的意思是，如果碰到改名后删除了，那么就直接删除原名的
				if(it->act == FILE_RENAMED || it->act == DIR_RENAMED)
				{
					filter.filted = true;
					it->path = it->path2;
					it->act = FILE_ACTION_REMOVED;
					//这时候需要反向过滤, 如果后面又有改变了，那这个删除操作过滤掉
					vector<notification_t>::iterator it2 = it.base();
					for(; it2 != m_notifications.end(); ++it2)
					{
						if(it2->act == FILE_ACTION_MODIFIED || it2->act == FILE_ACTION_ADDED){
							it->filted = true;
							break;
						}
					}
					//还原成原来的删除
					return true;
				}
				if(it->act == FILE_ACTION_MODIFIED || it->act == FILE_ACTION_ADDED)
				{
					it->filted = true;
					if(it->act == FILE_ACTION_ADDED) //ok, all filted，添加和删除抵销了不通知
					{
						filter.filted = true; //debug 清晰些
						return true;
					}
				}
				return true;
			}
		}
		else
		{ //for dir 过滤孩子的删除通知 ()
			for(; it != endit; ++it)
			{
				if(it->filted) continue;
				if(!isChildren(path, it->path) && path != it->path) continue;

				//rename 和 move 操作不过滤，因为涉及到另一个文件
				//要么把原来的删除(但是改变了语义)
				if(it->act == FILE_ACTION_REMOVED	|| 
				   it->act == FILE_ACTION_MODIFIED	|| 
				   it->act == FILE_ACTION_ADDED)
					it->filted = true;

				if(it->handled)
					if(
					   //it->act == FILE_ADDED	||	//不存在
					   //it->act == FILE_MODIFIED ||
					   it->act == DIR_ADDED		||
					   it->act == DIR_COPY		||
					   it->act == FILE_REMOVED	||
					   it->act == DIR_REMOVED
					  )
						it->filted = true;
			}
		}
		break;
	case FILE_ACTION_RENAMED_OLD_NAME:
		break;
	case FILE_ACTION_RENAMED_NEW_NAME:
		break;
	case FILE_RENAMED:
	case DIR_RENAMED:
		//if(filter.special || filter.spec2)	//改名不会涉及到属性
		//	break;
	case FILE_MOVED:
	case DIR_MOVED:
		for(; it != endit; ++it)
		{
			//cout << "it path:" << WideToMutilByte(it->path) << "is special:" << it->special << endl;
			if(it->filted) continue; 
			//if(it->path != path && it->path != path2) continue;

			if(it->path == path && !not_cmp)
			{
				//以前的改动都无效，被rename覆盖了
				if(it->act == FILE_ACTION_MODIFIED || it->act == FILE_ACTION_ADDED)
					it->filted = true;
				if(it->act == FILE_REMOVED || it->act == FILE_ACTION_REMOVED)	//文件的删除被过滤了
					it->filted = true;
				else
					not_cmp = true;	//简单比较
			}
			//上一个的路径是，rename的oldname
			if(it->path == path2)
			{
				if(it->act == FILE_ACTION_MODIFIED) {
					it->filted = true;
					change_act2 = FILE_ACTION_MODIFIED;
					//最后要加入modify
				} else if(it->act == FILE_ACTION_ADDED) {
					it->filted = true;
					change_act2 = FILE_ACTION_ADDED;
				} else if(it->act == filter.act) {	//可能性小, 连续改名
					it->filted = true;
					filter.path2 = it->path2;
					filter.spec2 = it->spec2;
				}
			}
		}
		if(change_act2 == FILE_ACTION_ADDED)
			filter.act = FILE_ACTION_ADDED;
		else if(change_act2 == FILE_ACTION_MODIFIED)
		{//新加一个表示modify的消息
			dlog("add a new modify because changed before rename");
			notification_t newone(filter);
			newone.act = FILE_ACTION_MODIFIED;
			m_notifications.push_back(newone);
		}
		break;
	}
	return true;
}

void report_unexpert(const notification_t & unexpert, const wstring & expert_path)
{
	cout << (unexpert.isdir ? "directory:" : "file:") << WideToMutilByte(unexpert.path)  
		<< " act:" << unexpert.act <<" not we experted. we expert: path=" << WideToMutilByte(expert_path) << endl; 
}

bool DirectoryMonitor::filt_notify2(notification_t & notify)
{
	//m_showfilt = file_exists(m_homew + L'\\' + TEMP_DIRNAMEW + L'\\' + L"showfilt.flag") ? true : false;
	//return false;
	if(file_exists(m_homew + L'\\' + TEMP_DIRNAMEW + L'\\' + L"test.flag"))
		return false;	//这里就不过滤了
	const wstring & path = notify.path;
	bool ret = false;
	DWORD act = notify.act;
	//通常我们要判断前一个是否是有关联的notify，这里可以判断很多特殊情况
	size_t size = m_notifications.size();
	size_t advance = 0;

	//三个开关
	bool handle = true;	//XXX 没用到？
	bool no_need_guess = false;
	bool filt_old = true;

	//XXX ???
	//notification_t filter = notify;
	m_guess_cnt++;


	//响应猜想，如果猜想错误，继续新的猜想，正确则实现猜想
	//XXX 目前的版本来说，由于猜想必须是连续的，所以两个并发的操作会导致有些猜想失败
	//但是，只有可打断的动作才受影响，像move这样的连续通知不受影响。
	switch(m_expert_act)
	{
	case maybe_out_move_dir: //判断是否是从外部move的dir, 判断点是紧接着的目录通知或者是孩子的copy
		//XXX 在最后超时时候也要处理这个猜想
		if(m_guess_cnt == 1) {
			notification_t & last = m_notifications[size-1];
			assert(last.expert_act == m_expert_act);

			if(act == FILE_ACTION_MODIFIED && notify.isdir)
			{
				if(path == GetBaseDIR(m_expert_path)) //下一个是父路径，ok正常消息还不能作为区分条件，判断下一个去
				{//猜想, 有可能这里结束了，就一个move dir过程
					//不要在这里判断，因为可能孩子还没有copy过来，太早了
					goto filt_this;
				}
				if(path == m_expert_path)
				{//自己改变了说明这是一个添加过程
					//win7会这里的触发，导致下面的判断不会到达, 但是xp不会到这里
					m_expert_act = maybe_copy_dir;
					last.act = DIR_ADDED;
					last.handled = true;
					goto filt_this;
				}
			}

			if(act == FILE_ACTION_ADDED && m_expert_path == GetBaseDIR(path)) { 
				//win7不会到达这里, 但是xp到...
				//dlog("last dir is father, so maybe this is copydir op");
				m_expert_act = maybe_copy_dir;
				//cout << "last act == " << last.act << " , now is DIR_ADD, last path = " << WideToMutilByte(last.path) << endl;
				last.act = DIR_ADDED;
				last.handled = true;
				break;
			}
			//yes, 猜中, 其他的操作来了, 留到最后处理
		}
		else{
			report_unexpert(notify, m_expert_path);
		}
		m_expert_act = none_op;
		break;
	}

	switch(m_expert_act)
	{
	case maybe_copy_dir:
		if(act == FILE_ACTION_ADDED) {
			if(isChildren(m_expert_path, path)) //正常的目录copy
			{ //XXX 这里直接判断是文件还是目录, 暂时还是用 FILE_ACTION_ADDED，避免引入混乱
				notify.act = notify.isdir == false ? FILE_ACTION_ADDED : DIR_ADDED;
				notify.handled = true;
				return false;
			}
			//不是期待的目录的add操作, 那么上次猜想操作结束了
			m_expert_act = none_op;
		}else if(act == FILE_ACTION_MODIFIED){
			if(notify.isdir)
				goto filt_this;
			//文件改变, 压缩通知
		} else{
			//可能交叉了其他过程（因为文件拷贝是个漫长过程）
			//但是这个过程又是可以独立的，不影响
			m_expert_act = none_op;
		}
		break;
	case maybe_move_file:
	case maybe_move_dir:
		notification_t & last = m_notifications[size-m_guess_cnt];
		if(act == FILE_ACTION_MODIFIED && notify.isdir)
		{//XXX 注意如果是根目录下，就收不到这个父目录的更改通知
			if(m_guess_cnt == 1) //must be 因为我忽略了文件夹的修改
			{
				if (path == GetBaseDIR(m_expert_path))//接着来的是父路径的修改
				{
					last.act = m_expert_act == maybe_move_dir ? DIR_REMOVED : FILE_REMOVED;
					last.handled = true;
					m_expert_act = none_op;
					//这就是一个remove操作，那么过滤子文件的动作, 但不用，因为猜想的动作之后就已经过滤了
					//删除文件没什么处理的	
				}//else, ok, 这是一个move操作,这里来的是目标地址的父目录
				goto filt_this;
			}
		}
		if(act == FILE_ACTION_ADDED || act == FILE_ACTION_MODIFIED)
		{//modify是剪切粘贴时候覆盖同名文件给的通知
			if((!notify.exist || 
				(notify.isdir && m_expert_act == maybe_move_dir) ||
				m_expert_act == maybe_move_file )
			   && GetFileName(path) == GetFileName(m_expert_path)) //没法判断更多了
			{
				//cout << "in guess move act" << endl;
				if(path == m_expert_path)
				{//被切出去又切回来（office ppt)
					no_need_guess = true;
					m_expert_act = none_op;
					break;
				}
				if(last.expert_act == m_expert_act)
				{//处理
					//cout << "we get move act" << endl;
					last.filted = true;
					notify.act = notify.isdir ? DIR_MOVED : FILE_MOVED;
					notify.handled = true;
					notify.path2 = last.path;
					notify.spec2 = last.special;
					no_need_guess = true;
					m_expert_act = none_op;
					//filt_old = false;
					//本来没有用这个moved消息来过滤的，但是因为xp的文件覆盖会先产生一个删除消息
					//所以要过滤掉那个删除消息
					goto filt_record;
					break;
				}
			}
			else
			{//we see what happened 
				cout << "m_expert_act == maybe_move_dir:" << (m_expert_act == maybe_move_dir) << endl;
				cout << "isdir:" << notify.isdir << " ,exist:" << notify.exist<< " ,path:" << WideToMutilByte(path) << endl;
			}
		}
		else if(act == FILE_ACTION_REMOVED || L"" == GetBaseDIR(m_expert_path))
		{//直接来的就是另一个删除操作 或者 父路径是空没收到
			last.act = m_expert_act == maybe_move_dir ? DIR_REMOVED : FILE_REMOVED;
			last.handled = true;
			if(isChildren(m_expert_path, path))
			{//是删除目录的孩子，忽略.因为有可能出现父路径和孩子删除通知的顺序颠倒了（两个孩子的时候）
				//XXX 修改后验证是否颠倒, 好像不会
				goto filt_this;
			}
		}
		else 
		{ //可能被其他动作打断，但是原动作变成删除/添加，TODO：这里可能用多个session来改进
			report_unexpert(notify, m_expert_path);
		}
		m_expert_act = none_op;
		break;
	}


	if(handle)
	{
		//来处理各种情况
		switch (act)
		{
		case FILE_ACTION_ADDED:
			/* 不过滤，虽然文件不存在，后面的通知需要这个added，总之会被过滤掉的
			   if(!notify.exist) goto filt_this; */
			break;
		case FILE_ACTION_MODIFIED:
			if(notify.isdir || notify.fspec || !notify.exist)//文件夹或隐藏文件修改，忽略
				goto filt_this;
			no_need_guess = true;
			break;
		case FILE_ACTION_REMOVED:
			break;
		case FILE_ACTION_RENAMED_OLD_NAME:
			break;
		case FILE_ACTION_RENAMED_NEW_NAME:
			assert(size > 0);
			notification_t & last = m_notifications[size-1];
			assert(last.act == FILE_ACTION_RENAMED_OLD_NAME);

			//改名属性没有变化
			last.filted = true;
			notify.act = notify.isdir ? DIR_RENAMED : FILE_RENAMED;
			notify.handled = true;
			notify.path2 = last.path;
			notify.spec2 = last.special;
			no_need_guess = true;
			m_expert_act = none_op;
			//唯一一个需要两个路径的
			if(m_blacklist->Query(notify.act, notify.path2, notify.path))
				goto filt_this;
			break;
		}
	}

	if(!no_need_guess && m_expert_act == none_op)
	{ // 处理情况
		guess(notify);
	}

filt_record:
	if(filt_old && size >= advance)
	{
		return !filt_old_notify(notify, advance) || ret;
	}
	return ret;

filt_this:
	m_guess_cnt--;
	return true;
}

//返回发送的数量
int DirectoryMonitor::SendNotify()
{
	m_expert_act = none_op; //新的开始
	size_t size = m_notifications.size();
	LocalNotification ln;
	ln.basedir = m_home;
	ln.t = m_type;

	for(vector<notification_t>::iterator it	= m_notifications.begin(); it != m_notifications.end(); ++it)
	{
		//ExplainAction2(*it, m_id);

		//屏蔽消息目录的消息全部过滤
		if(isChildren(m_silent_dir, it->path))
			continue;
		if(
		   (it->act == FILE_RENAMED || 
			it->act == DIR_RENAMED || 
			it->act == FILE_MOVED || 
			it->act == DIR_MOVED) && isChildren(m_silent_dir, it->path2)
		  )
			continue;
		if((it->act == FILE_MOVED || it->act == DIR_MOVED)){
			if(it->special && !it->spec2)	//移动到隐藏目录，当删除
			{
				it->act = it->act==FILE_MOVED ? FILE_REMOVED : DIR_REMOVED;
				it->special = false;
				it->path = it->path2;
			}
			if(!it->special && it->spec2)	//从隐藏目录移动到实际的目录，当新增
				it->act = it->act==FILE_MOVED ? FILE_ADDED : 
					(hasChildren(m_homew + L'\\' + it->path) ? DIR_COPY : DIR_ADDED);
		}
		if(it->act == FILE_RENAMED){	//case tmp文件我们过滤掉了
			if(it->special && !it->spec2)	//当删除
			{
				if(isOffice(it->path2))
				{//XXX office 文件特殊处理
					cout << "office rename filed: from " << WideToMutilByte(it->path2) 
						<< " ,to " << WideToMutilByte(it->path) << endl;
					it->filted = true;
				}
				else
				{
					it->act = FILE_REMOVED;
					it->special = false;
					it->path = it->path2;
				}
			}
			if(!it->special && it->spec2)	//当新增
			{
				cout << "office rename as NEW: from " << WideToMutilByte(it->path2) 
					<< " ,to " << WideToMutilByte(it->path) << endl;
				it->act = FILE_ADDED;
				if(isOffice(it->path))
					it->act = FILE_MODIFIED;
			}
		}

		if(it->special && ! it->filted) 
		{
			it->filted = true;
			
			if(it->isdir || it->fspec || isFiltType(it->path))
			{
				it->filted = true;
			}
			else if(it->act == FILE_ACTION_ADDED || it->act == FILE_ACTION_MODIFIED)
			{
				//自己是隐藏的, 目前只是验证文件修改没
				if(it->attr != -1 && isSpecial(it->attr))
				{
					DWORD attr = ::GetFileAttributes((m_homew + L"/" + it->path).c_str());
					if(!isSpecial(attr))
					{
						it->filted = false;
						UpdateAttributeCache(it->path);
						cout << "specal flag is changed!" << endl;
					}
				}
			}
		}
		//打印log
		ExplainAction2(*it, m_id);

		if(!m_showfilt && it->filted) //m_showfilt for debug
			continue;

		if(m_cbp)
		{
			LocalOp op;
			WideToMutilByte(it->path, op.to);
			string fpath = GetBaseDIR(op.to);
			op.to = GetFileName(op.to);
			switch(it->act)
			{
			case FILE_ACTION_ADDED:
				if(!file_exists(m_homew + L'\\' + it->path))
					continue;
				it->act = it->isdir == false ? FILE_ADDED : 
					hasChildren(m_homew + L'\\' + it->path) ? DIR_COPY : DIR_ADDED;
				break;
			case FILE_ACTION_REMOVED:
				it->act = FILE_REMOVED;
				break;
			case FILE_ACTION_MODIFIED:
				it->act = FILE_MODIFIED;
				break;
			case FILE_RENAMED:
			case DIR_RENAMED:
				WideToMutilByte(it->path2, op.from);
				op.from = GetFileName(op.from);
				break;
			case FILE_MOVED:
			case DIR_MOVED:
				WideToMutilByte(it->path2, op.from);
				break;
			}
			op.act = it->act;

			//用来过滤转变后的屏蔽消息，算是最后通知前最后一道防线
			switch(it->act)
			{
			case FILE_RENAMED:
			case DIR_RENAMED:
			case FILE_MOVED:
			case DIR_MOVED:
				if(m_blacklist->Query(it->act, it->path2, it->path))
					continue;
				break;
			default:
				if(m_blacklist->Query(it->act, it->path, wstring()))
					continue;
				break;
			}
			if(fpath != ln.fpath)
			{//父路径不同,通知客户端
				if(!ln.ops.empty())
				{
					//TODO: 支持批量操作
					m_cbp(m_varg, &ln);
					ln.ops.clear();
				}
				ln.fpath = fpath;
			}
			ln.ops.push_back(op);
		}
	}
	if(m_cbp && !ln.ops.empty())
	{
		m_cbp(m_varg, &ln);
		//for debug
		if(m_cbp != print_notify)
			print_notify(m_varg, &ln);
	}
	m_notifications.clear();
	//cout << "clear" << endl;
	return size;
}

//在超时时候释放资源
void DirectoryMonitor::release_resource()
{
	if(m_file_attrs.size() > MAX_CACHE_SIZE)
		m_file_attrs.clear();
}
