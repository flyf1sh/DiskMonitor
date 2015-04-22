#include <string>
#include <iostream>
#include <windows.h>
#include <WinBase.h>
#include <atlstr.h>
#include <stdio.h>
#include <fstream>
using namespace std;
#include "MonitorDebug.h"
#include "MonitorUtil.h"

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
	CSLock lock(debug_cs);
	bool iunknow = false;
	if(filted)
		cout << "filt item:";
	string type = t==DIR_TYPE?"目录":"文件";
	switch (dwAction)
	{
	case FILE_ACTION_ADDED:
		//cout << "ok, add a file/dir, size="<<sfilename.length()<<",last:"<<sfilename[sfilename.length()-1]<<endl;
		cout<<id<<" 添加" << type << "=>"<<sfilename<<endl;
		break;
	case FILE_ACTION_REMOVED:
		cout<<id<<" 删除" << type << "=>"<<sfilename<<endl;
		break;
	case FILE_ACTION_MODIFIED:
		cout<<id<<" 修改" << type << "内容=>"<<sfilename<<endl;
		break;
	case FILE_ACTION_RENAMED_OLD_NAME:
		cout<<id<< " " << type << "重命名 旧=>"<<sfilename<<endl;
		break;
	case FILE_ACTION_RENAMED_NEW_NAME:
		cout<<id<< " " << type << "重命名 新=>"<<sfilename<<endl;
		break;

	case FILE_ADDED:
	case DIR_ADDED:
		cout<<id<< " 新增" << type << " " << sfilename << endl;
		break;
	case FILE_RENAMED:
	case DIR_RENAMED:
		cout<<id<< " " << type << "重命名 " <<  path2 << " => " << sfilename << endl;
		break;
	case FILE_MOVED:
	case DIR_MOVED:
		cout<<id<< " " << type << "移动 " << path2 << " => " << sfilename << endl;
		break;
	case FILE_REMOVED:
	case DIR_REMOVED:
		cout<<id<<" 删除" << type << ":"<<sfilename<<endl;
		break;
	case DIR_COPY:
		cout<<id<< " " << type << " copy: " << sfilename << endl;
		break;
	case FILE_ACTION_END:
		cout << id << " " << type << " end operation" << endl;
		break;
	default:
		iunknow = true;
		break;
	}
	if(iunknow && dwAction >= FILE_ACTION)
	{
		cout << id << " FILE_ACTION:" << (dwAction - FILE_ACTION) << " " << type << " " << sfilename;
		if(!path2.empty())
			cout << " => " << path2 << endl;
		else
			cout << endl;
	}
}

void ExplainAction2(const notification_t & notify, int id) 
{
	string sfilename, path2; 
	WideToMutilByte(notify.path, sfilename);
	WideToMutilByte(notify.path2, path2);
	ExplainAction(notify.act, sfilename, id, notify.isdir?DIR_TYPE:FILE_TYPE, path2, notify.filted);
}

void dlog(const string & msg, bool thread_id)
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

void dlog(const wstring & msg, bool thread_id)
{
	return dlog(WideToMutilByte(msg), thread_id);
}

void print_notify(void *arg, LocalNotification * ln)
{
	CSLock lock(debug_cs);
	cout << "notify: basedir:" << ln->basedir << endl;
	cout << "        father path:" << ln->fpath << endl;
	cout << "\thas " << ln->ops.size() << " ops" << endl;
	int i = 0;
	for(vector<LocalOp>::iterator it = ln->ops.begin(); it != ln->ops.end(); ++it)
	{
		cout << "\t" << i++ << ": op=" << it->act;
		if(it->from != "")
			cout << " ,from = " << it->from << " ,to = " << it->to << endl;
		else
			cout << " ,target = " << it->to << endl;
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
