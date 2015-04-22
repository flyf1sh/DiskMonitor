#ifndef MONITOR_DEBUG_H
#define MONITOR_DEBUG_H

#include "MonitorUtil.h"
#include <string>
using namespace std;

#define LOG_INFO 10
#define LOG_WARNING 15 
#define LOG_ERROR 20
#define LOG_CRITICAL 30
#define LOG_CLOSE 30

extern CRITICAL_SECTION debug_cs;

//user setting
#define LOG_LEVEL 10
#define LOG_DEBUG 10

#define dout_impl(v)						\
	do{										\
		if(v >= LOG_LEVEL){					\
			CSLock lock(debug_cs);			\
			if(0){							\
				char a[((v>-1)||(v<=50))?1:-1];	\
				a[0] = 0;					\
			}
#define dendl	std::endl;} }while(0)
#define dflush	std::flush;} }while(0)

#define dout dout_impl(LOG_DEBUG) std::cout
#define derr dout_impl(LOG_DEBUG) std::cerr
#define dout1(v) dout_impl(v) std::cout


//打印的开关
//#ifdef _DEBUG
#ifndef _DEBUG_MONITOR
#define _DEBUG_MONITOR
#endif
//#endif

struct notification_t;
struct LocalNotification;

void cout2file();
void ExplainAction(DWORD dwAction, const string & sfilename, WORD id, WORD t, const string & path2=string(), bool filted=false) ;
void ExplainAction2(const notification_t & notify, int id);
void dlog(const string & msg, bool thread_id =true);
void dlog(const wstring & msg, bool thread_id=true);
void print_notify(void *arg, LocalNotification * ln);

#endif
