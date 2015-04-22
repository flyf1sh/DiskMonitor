#include <string>
#include <iostream>
#include <windows.h>
#include <WinBase.h>
#include <atlstr.h>
#include <stdio.h>
#include <fstream>
using namespace std;

#include "MonitorDebug.h"

#ifdef _DEBUG_MONITOR
CRITICAL_SECTION debug_cs;
CSLock lock_init(debug_cs, false, true);

//打印到文件或者stdout的开关
bool print_in_file = false;
//bool print_in_file = true;
ofstream logfile;
void cout2file()
{
	if(!print_in_file)
		return;
	logfile.open(".\\monitor_log.txt", ios::app|ios::out);	
	std::cout.rdbuf(logfile.rdbuf());
	std::cerr.rdbuf(logfile.rdbuf());
}

void ExplainAction(DWORD dwAction, const string & sfilename, WORD id, WORD t, const string & path2, bool filted) 
{
	bool iunknow = false;
	if(filted)
		dout << "filt item:" << dflush;
	string type = t==DIR_TYPE?"目录":"文件";
	switch (dwAction)
	{
	case FILE_ACTION_ADDED:
		//dout << "ok, add a file/dir, size="<<sfilename.length()<<",last:"<<sfilename[sfilename.length()-1]<<dendl;
		dout<<id<<" 添加" << type << "=>"<<sfilename<<dendl;
		break;
	case FILE_ACTION_REMOVED:
		dout<<id<<" 删除" << type << "=>"<<sfilename<<dendl;
		break;
	case FILE_ACTION_MODIFIED:
		dout<<id<<" 修改" << type << "内容=>"<<sfilename<<dendl;
		break;
	case FILE_ACTION_RENAMED_OLD_NAME:
		dout<<id<< " " << type << "重命名 旧=>"<<sfilename<<dendl;
		break;
	case FILE_ACTION_RENAMED_NEW_NAME:
		dout<<id<< " " << type << "重命名 新=>"<<sfilename<<dendl;
		break;

	case FILE_ADDED:
	case DIR_ADDED:
		dout<<id<< " 新增" << type << " " << sfilename << dendl;
		break;
	case FILE_RENAMED:
	case DIR_RENAMED:
		dout<<id<< " " << type << "重命名 " <<  path2 << " => " << sfilename << dendl;
		break;
	case FILE_MOVED:
	case DIR_MOVED:
		dout<<id<< " " << type << "移动 " << path2 << " => " << sfilename << dendl;
		break;
	case FILE_REMOVED:
	case DIR_REMOVED:
		dout<<id<<" 删除" << type << ":"<<sfilename<<dendl;
		break;
	case DIR_COPY:
		dout<<id<< " " << type << " copy: " << sfilename << dendl;
		break;
	case FILE_ACTION_END:
		dout << id << " " << type << " end operation" << dendl;
		break;
	default:
		iunknow = true;
		break;
	}
	if(iunknow && dwAction >= FILE_ACTION)
	{
		dout << id << " FILE_ACTION:" << (dwAction - FILE_ACTION) << " " << type << " " << sfilename << dflush;
		if(!path2.empty())
			dout << id << " FILE_ACTION:" << (dwAction - FILE_ACTION) 
				<< " " << type << " " << sfilename << " => " << path2 << dendl;
		else
			dout << id << " FILE_ACTION:" << (dwAction - FILE_ACTION) 
				<< " " << type << " " << sfilename << dendl;
	}
}

void ExplainAction2(const notification_t & notify, int id) 
{
	string sfilename, path2; 
	WideToMutilByte(notify.path, sfilename);
	WideToMutilByte(notify.path2, path2);
	ExplainAction(notify.act, sfilename, id, notify.isdir?DIR_TYPE:FILE_TYPE, path2, notify.filted);
}

void dlog_v1(const string & msg, bool thread_id)
{
	string msgx = msg;
	if(thread_id)
	{
		char buf[1024];
		sprintf_s(buf, 1024, "thread:%ld  %s", GetCurrentThreadId(), msg.c_str());
		msgx = buf;
	}
	CSLock lock(debug_cs);
	string now = NowInString();
	cout << now << msgx << endl;
}

void dlog(const string & msg, bool thread_id)
{
	if(thread_id)
		dout << NowInString() << "thread:" << GetCurrentThreadId() << " " << msg << dendl;
	else
		dout << NowInString() << msg << dendl;
}

void dlog(const wstring & msg, bool thread_id)
{
	return dlog(WideToMutilByte(msg), thread_id);
}

void print_notify(void *arg, LocalNotification * ln)
{
	dout << "notify: basedir:" << ln->basedir
	     << "\n        father path:" << ln->fpath
	     << "\n\thas " << ln->ops.size() << " ops" << dendl;
	int i = 0;
	for(vector<LocalOp>::iterator it = ln->ops.begin(); it != ln->ops.end(); ++it)
	{
		if(it->from != "")
			dout << "\t" << i++ << ": op=" << it->act
				<< " ,from = " << it->from << " ,to = " << it->to << dendl;
		else
			dout << "\t" << i++ << ": op=" << it->act
				<< " ,target = " << it->to << dendl;
	}
}

#else
void cout2file(){ };
void ExplainAction(DWORD dwAction, const string & sfilename, WORD id, WORD t, const wstring & path2, bool filted) { };
void ExplainAction2(const notification_t & notify, int id){ };
void dlog(const string & msg, bool thread_id){ };
void dlog(const wstring & msg, bool thread_id){ };
void print_notify(void *arg, LocalNotification * ln){ };
#endif
