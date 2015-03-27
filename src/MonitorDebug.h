#ifndef MONITOR_DEBUG_H
#define MONITOR_DEBUG_H

#include <string>
using namespace std;

struct notification_t;
struct LocalNotification;

void cout2file();
void ExplainAction(DWORD dwAction, const string & sfilename, WORD id, WORD t, const string & path2=string(), bool filted=false) ;
void ExplainAction2(const notification_t & notify, int id);
void dlog(const string & msg, bool thread_id =true);
void dlog(const wstring & msg, bool thread_id=true);
void print_notify(void *arg, LocalNotification * ln);

#endif
