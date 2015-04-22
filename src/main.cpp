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
	cout << "创建文件:" << fn << endl;
	res = dm->DoActWithoutNotify(1, fn);
	if(res) goto fail;
	
	cout << "创建目录:" << dn << endl;
	res = dm->DoActWithoutNotify(2, dn);
	if(res) goto fail;
	cout << "创建目录:" << dn2 << endl;
	res = dm->DoActWithoutNotify(2, dn2);
	if(res) goto fail;
	//cin >> ch;
	cout << "移动文件:" << fn << " ,到:" << dn+"\\"+fn2 << endl;
	res = dm->DoActWithoutNotify(3, fn, dn+"\\"+fn2);
	if(res) goto fail;
	//return ;
	cout << "改名:" << dn+"\\"+fn2 << " ,到:" << dn+"\\"+fn << endl;
	res = dm->DoActWithoutNotify(4, dn+"\\"+fn2, dn+"\\"+fn);
	if(res) goto fail;
	cout << "拷贝 文件夹:" << dn << " ,到:" << dn2 + "\\" + dn << endl;
	res = dm->DoActWithoutNotify(5, dn, dn2 + "\\" + dn);
	if(res) goto fail;
	cout << "删除目录:" << dn << endl;
	res = dm->DoActWithoutNotify(0, dn);
	if(res) goto fail;
	cout << "移动 文件夹:" << dn2 + "\\" + dn << " ,到:" << dn << endl;
	res = dm->DoActWithoutNotify(3, dn2 + "\\" + dn, dn);
	if(res) goto fail;
	cout << "删除目录:" << dn << endl;
	res = dm->DoActWithoutNotify(0, dn);
	if(res) goto fail;
	cout << "删除目录:" << dn2 << endl;
	res = dm->DoActWithoutNotify(0, dn2);
	if(res) goto fail;
	cout << "test_shield end" << endl;
	return ;
fail:
	cout << "handle fail:" << res << endl;
}

void test_shield2(DirectoryMonitor * dm)
{
	DWORD res;
	//char ch;
	string fn = "dir1";
	string fn_new = "dir2";
	res = dm->DoActWithoutNotify(4, fn, fn_new);
	cout << "move file:" << fn << " => "<< fn_new << " , res=" << res << endl;
}

void test_create_office(DirectoryMonitor * dm)
{
	DWORD res;
	const char * files[] = {"word文档.docx", "word文档2.doc", 
		"excel文档.xlsx", "excel文档2.xls", "ppt文档.pptx", "ppt文档2.ppt"};
	for(int i=0; i < sizeof(files)/sizeof(char*); i++)
	{
		res = dm->DoActWithoutNotify(1, files[i]);
		cout << "create file:" << files[i] << " , res=" << res << endl;
	}
	/*
	wchar_t *word = L"C:\\测试盘\\项目\\myword.docx";
	res = CreateOfficeFile(word);
	wcout << L"create file:" << word << L" , res=" << res << endl;
	*/
}

void main2()
{
	char c;
	DirectoryChangeHandler * dc = new DirectoryChangeHandler(3);
	//DirectoryMonitor * proj = new DirectoryMonitor(dc, "C:\\测试盘\\项目", "proj", print_notify, NULL);
	DirectoryMonitor * proj = new DirectoryMonitor(dc, "C:\\测试盘\\项目", "proj", print_notify, NULL);
	cout << "监控proj" << endl;
	DirectoryMonitor * sync = new DirectoryMonitor(dc, "C:\\测试盘\\同步", "sync", print_notify, NULL);
	cout << "监控sync" << endl;
	
	Sleep(2000);
	//test 屏蔽的api
	//test_shield(proj);
	/*
	string dn = "test_mvdir";
	DWORD res = proj->DoActWithoutNotify(3, "test\\"+dn, dn, FOP_REPLACE);
	cout << "test mv dir overwite:" << res << endl;
	*/
	test_create_office(proj);
	//test_shield2(proj);

	cout << "input to quit:" << flush;
	cin >> c;
	delete proj;
	cout << "删除proj" << endl;
	delete sync;
	cout << "删除sync" << endl;
	cout << "go on input" << endl;
//	cin >> c;
	dc->Terminate();
	cout << "delte dc";
	cout << "go on input" << endl;
//	cin >> c;
	delete dc;
	cout << "go on input" << endl;
	cin >> c;
	return;
}

void test()
{
	wstring dir = L"C:\\测试盘\\项目";
	DWORD res = ::GetFileAttributes(dir.c_str());
	if(res == INVALID_FILE_ATTRIBUTES)
		cerr << "GetAttribute fail:" << GetLastError() << endl;
	else if(res & FILE_ATTRIBUTE_READONLY)
		cout << "has readonly attr" << endl;
	else if(res & FILE_ATTRIBUTE_SYSTEM)
		cout << "has system attr" << endl;
	else 
		cout << "normal" << endl;
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

