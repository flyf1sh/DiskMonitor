#include "MonitorUtil.h"
#include "ReadDirectoryChanges.h"
#include <fstream>
#include <io.h>
#include <iostream>
#include <string>
#include <assert.h>
#include <locale>
using namespace std;

//streambuf *outstream;

#include "DirectoryMonitor.h"
#include "DirectoryChangeHandler.h"

//@param: act = 0 删除；1 新增文件； 2 新增文件夹； 3 移动； 4 改名； 5 拷贝
void test_shield(DirectoryMonitor * dm)
{
	DWORD res;
	char ch;
	//新增文件 
	string fn = "add_file 文件1";
	string fn2 = "文件2";
	string dn = "新增文件夹A";
	string dn2 = "文件夹B\\文件夹C";
	//创建文件
	dout << "创建文件:" << fn << dendl;
	res = dm->DoActWithoutNotify(1, fn);
	if(res) goto fail;
	
	dout << "创建目录:" << dn << dendl;
	res = dm->DoActWithoutNotify(2, dn);
	if(res) goto fail;
	dout << "创建目录:" << dn2 << dendl;
	res = dm->DoActWithoutNotify(2, dn2);
	if(res) goto fail;
	//cin >> ch;
	dout << "移动文件:" << fn << " ,到:" << dn+"\\"+fn2 << dendl;
	res = dm->DoActWithoutNotify(3, fn, dn+"\\"+fn2);
	if(res) goto fail;
	//return ;
	dout << "改名:" << dn+"\\"+fn2 << " ,到:" << dn+"\\"+fn << dendl;
	res = dm->DoActWithoutNotify(4, dn+"\\"+fn2, dn+"\\"+fn);
	if(res) goto fail;
	dout << "拷贝 文件夹:" << dn << " ,到:" << dn2 + "\\" + dn << dendl;
	res = dm->DoActWithoutNotify(5, dn, dn2 + "\\" + dn);
	if(res) goto fail;
	dout << "删除目录:" << dn << dendl;
	res = dm->DoActWithoutNotify(0, dn);
	if(res) goto fail;
	dout << "移动 文件夹:" << dn2 + "\\" + dn << " ,到:" << dn << dendl;
	res = dm->DoActWithoutNotify(3, dn2 + "\\" + dn, dn);
	if(res) goto fail;
	dout << "删除目录:" << dn << dendl;
	res = dm->DoActWithoutNotify(0, dn);
	if(res) goto fail;
	dout << "删除目录:" << dn2 << dendl;
	res = dm->DoActWithoutNotify(0, dn2);
	if(res) goto fail;
	dout << "test_shield end" << dendl;
	return ;
fail:
	dout << "handle fail:" << res << dendl;
}

void test_shield2(DirectoryMonitor * dm)
{
	DWORD res;
	//char ch;
	string fn = "dir1";
	string fn_new = "dir2";
	res = dm->DoActWithoutNotify(4, fn, fn_new);
	dout << "move file:" << fn << " => "<< fn_new << " , res=" << res << dendl;
}

void test_create_office(DirectoryMonitor * dm)
{
	DWORD res;
	const char * files[] = {"word文档.docx", "word文档2.doc", 
		"excel文档.xlsx", "excel文档2.xls", "ppt文档.pptx", "ppt文档2.ppt"};
	for(int i=0; i < sizeof(files)/sizeof(char*); i++)
	{
		res = dm->DoActWithoutNotify(1, files[i]);
		dout << "create file:" << files[i] << " , res=" << res << dendl;
	}
	/*
	wchar_t *word = L"C:\\测试盘\\项目\\myword.docx";
	res = CreateOfficeFile(word);
	wdout << L"create file:" << word << L" , res=" << res << dendl;
	*/
}

void main2()
{
	char c;
	DirectoryChangeHandler * dc = new DirectoryChangeHandler(3);
	//DirectoryMonitor * proj = new DirectoryMonitor(dc, "C:\\测试盘\\项目", "proj", print_notify, NULL);
	DirectoryMonitor * proj = new DirectoryMonitor(dc, "C:\\测试盘\\项目", "proj", print_notify, NULL);
	dout << "监控proj" << dendl;
	DirectoryMonitor * sync = new DirectoryMonitor(dc, "C:\\测试盘\\同步", "sync", print_notify, NULL);
	dout << "监控sync" << dendl;
	
	Sleep(2000);
	//test 屏蔽的api
	//test_shield(proj);
	/*
	string dn = "test_mvdir";
	DWORD res = proj->DoActWithoutNotify(3, "test\\"+dn, dn, FOP_REPLACE);
	dout << "test mv dir overwite:" << res << dendl;
	*/
	test_create_office(proj);
	//test_shield2(proj);

	dout << "input to quit:" << dflush;
	cin >> c;
	delete proj;
	dout << "删除proj" << dendl;
	delete sync;
	dout << "删除sync" << dendl;
//	dout << "go on input" << dendl;
//	cin >> c;
	dc->Terminate();
	dout << "delte dc" << dendl;
//	dout << "go on input" << dendl;
//	cin >> c;
	delete dc;
	dout << "go on input" << dendl;
	cin >> c;
	return;
}

void test()
{
	wstring dir = L"C:\\测试盘\\项目";
	DWORD res = ::GetFileAttributes(dir.c_str());
	if(res == INVALID_FILE_ATTRIBUTES)
		derr << "GetAttribute fail:" << GetLastError() << dendl;
	else if(res & FILE_ATTRIBUTE_READONLY)
		dout << "has readonly attr" << dendl;
	else if(res & FILE_ATTRIBUTE_SYSTEM)
		dout << "has system attr" << dendl;
	else 
		dout << "normal" << dendl;
	char c;
	cin >> c;
	return;
}

//这里来执行
int main()
{
	wcout.imbue(locale("chs"));
	//test();
	main2();
	return 0;
}

