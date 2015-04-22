#ifndef DIRECTORY_MONITOR_H
#define DIRECTORY_MONITOR_H

#include<vector>
#include<map>
using namespace std;
#include "MonitorUtil.h"

//class DirectoryChangeHandler;
struct TDirectoryChangeNotification;

/*
 *	用来设置监控目录的类, 每一个目录一个实例，可以选着屏蔽的文件（夹）
 *	DirectoryChangeHandler 的子线程会回调这里函数处理相应的目录变化
 */
class DirectoryMonitor
{
	friend class DirectoryChangeHandler;

public:
	DirectoryMonitor(DirectoryChangeHandler * dc, const string & path, const string& type,
					 void (*cbp)(void *arg, LocalNotification *), void * varg);
	~DirectoryMonitor();

	int State() const { return m_running;}

	//屏蔽通知消息的动作，主要用于listupdate/tree后本地的更新
	//@retrun: 成功返回0， 失败返回错误码
	//@param: act = 0 删除；1 新增文件； 2 新增文件夹； 3 移动； 4 改名； 5 拷贝；
	//@param: LPtrCancel 用来取消这个操作的flag； 初始为0，设置为1取消操作 (暂时未实现)
	//@param: from 可以多文件输入遵循SHFileOperation的输入文件规则( 但是目前用一个个的做来实现吧)
	DWORD DoActWithoutNotify(int act, const string & from, const string & to=string(), DWORD flag=0, int * LPtrCancel=NULL);
	//用blacklist来实现屏蔽
	DWORD DoActWithoutNotify2(int act, const string & from, const string & to=string(), DWORD flag=0, int * LPtrCancel=NULL);

private:
	void Pause()	{};//{m_running = 0;}
	void Resume()	{};//{m_running = 1;}

private:
	//撤销这个监控（现在还没有用到，也就是允许用户不监控这个目录）
	int Terminate();
	int GetNotify(struct TDirectoryChangeNotification & notify);
	int SendNotify(); //返回发送的数量
	bool filt_notify2(notification_t & notify);
	bool guess(notification_t & notify);
	bool filt_old_notify(notification_t & filter, int advance);
	void release_resource();
	void UpdateAttributeCache(const wstring & rpathw);
	DWORD GetAttributeFromCache(const wstring & rpathw, DWORD act, DWORD & self);

	int ClearBlacklist();
private:
	DirectoryChangeHandler * m_dc;
	string	m_home;
	wstring m_homew;
	wstring m_silent_dir;	//屏蔽消息目录
	string	m_type;							//这个监控所属的类型
	void (*m_cbp)(void *, LocalNotification *);
	void * m_varg;

	int m_id;					//inner id
	volatile int m_running;		//run flag
	

	//通知过滤部分
	vector<notification_t>	m_notifications;	//暂存的通知，用来过滤和统一发出
	map<wstring, DWORD>		m_file_attrs;		//文件目录的attr，如果不存在是1
	wstring m_expert_path;	//猜想下一个通知的路径
	WORD m_expert_act;	//猜想下一个操作
	int m_guess_cnt;	//做上次猜想进入的计数
	class NotificationBlacklist * m_blacklist;	//屏蔽消息黑名单
	//notification_t m_boss;	//上次通知启动的主	??
	//debug
	bool m_showfilt;
	bool m_isXP;	//xp系统处理有些不同
};

#endif
