#include "MonitorUtil.h"

class CreateOfficeException
{
public:
	CreateOfficeException(const wchar_t * err):m_err(err){ }
	const wchar_t* what() const throw()
	{
		return m_err.c_str();
	}
private:
	wstring m_err;
};

class OfficeTool
{
public:
	OfficeTool();
	~OfficeTool();

	int CreateWord(const wstring & path);
	int CreateExcel(const wstring & path);
	int CreatePPt(const wstring & path);
private:
	void AutoWrap(int autoType, VARIANT *pvResult, IDispatch *pDisp, LPOLESTR ptName, int cArgs...);
	int CreateTemplate(LPCWSTR path, LPCWSTR name, const CLSID & clsid, LPCWSTR docsname, bool make_visible=false);

	wchar_t msg_buf[1024];

	static bool com_init;
	static bool b_clsid_word;
	static bool b_clsid_excel;
	static bool b_clsid_ppt;
	static CLSID clsid_word;
	static CLSID clsid_excel;
	static CLSID clsid_ppt;
} ;

bool OfficeTool::com_init = false;
bool OfficeTool::b_clsid_word = false;
bool OfficeTool::b_clsid_excel = false;
bool OfficeTool::b_clsid_ppt = false;
CLSID OfficeTool::clsid_word;
CLSID OfficeTool::clsid_excel;
CLSID OfficeTool::clsid_ppt;

OfficeTool::OfficeTool()
{
	if(!OfficeTool::com_init)
	{
		HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
		if(FAILED(hr)){
			dout << "init com fail" << dendl;
		}
		/* always do init 因为com的单线程模式？我也不懂
		else
			OfficeTool::com_init = true;
		*/
	}
}
OfficeTool::~OfficeTool()
{
	CoUninitialize();
}

int CreateOfficeFile(const wstring & wpath)
{	
	if(file_exists(wpath))
		return ERROR_ALREADY_EXISTS;
	wstring ext = GetFileExt(wpath);
	OfficeTool office_maker;
	try{
		if(ext == L"docx" || ext == L"doc")
			return office_maker.CreateWord(wpath);
		if(ext == L"xlsx" || ext == L"xls")	
			return office_maker.CreateExcel(wpath);
		if(ext == L"pptx" || ext == L"ppt")	
			return office_maker.CreatePPt(wpath);
		return ERROR_BAD_FILE_TYPE;
	}
	catch(CreateOfficeException &e)
	{
		wcout << L"create file:" << wpath << L" ,meet err:"<< e.what() << endl;
	}
	return ERROR_BUSY;
}

void OfficeTool::AutoWrap(int autoType, VARIANT *pvResult, IDispatch *pDisp, 
	LPOLESTR ptName, int cArgs...) 
{
	// Begin variable-argument list
	va_list marker;
	va_start(marker, cArgs);

	if (!pDisp) 
		throw CreateOfficeException(L"pDisp is NULL");

	// Variables used
	DISPPARAMS dp = { NULL, NULL, 0, 0 };
	DISPID dispidNamed = DISPID_PROPERTYPUT;
	DISPID dispID;
	HRESULT hr;

	// Get DISPID for name passed
	hr = pDisp->GetIDsOfNames(IID_NULL, &ptName, 1, LOCALE_USER_DEFAULT, &dispID);
	if (FAILED(hr))
	{
		swprintf_s(&msg_buf[0],1024,L"IDispatch::GetIDsOfNames(\"%s\") failed w/err 0x%08lx\n", ptName, hr);
		throw CreateOfficeException(msg_buf);
	}

	// Allocate memory for arguments
	VARIANT *pArgs = new VARIANT[cArgs + 1];
	// Extract arguments...
	for(int i=0; i < cArgs; i++) 
	{
		pArgs[i] = va_arg(marker, VARIANT);
	}

	// Build DISPPARAMS
	dp.cArgs = cArgs;
	dp.rgvarg = pArgs;

	// Handle special-case for property-puts
	if (autoType & DISPATCH_PROPERTYPUT)
	{
		dp.cNamedArgs = 1;
		dp.rgdispidNamedArgs = &dispidNamed;
	}

	// Make the call
	hr = pDisp->Invoke(dispID, IID_NULL, LOCALE_SYSTEM_DEFAULT,
		autoType, &dp, pvResult, NULL, NULL);

	// End variable-argument section
	va_end(marker);

	delete[] pArgs;

	if (FAILED(hr)) 
	{
		swprintf_s(&msg_buf[0],1024,L"IDispatch::Invoke(\"%s\"=%08lx) failed w/err 0x%08lx\n", ptName, dispID, hr);
		throw CreateOfficeException(msg_buf);
	}
}

int OfficeTool::CreateWord(const wstring & path)
{
	HRESULT hr;
	if(!OfficeTool::b_clsid_word)
	{ 
		LPCOLESTR progID = L"Word.Application";
		hr = CLSIDFromProgID(progID, &clsid_word);
		if(FAILED(hr))
		{
			swprintf_s(&msg_buf[0],1024,L"CLSIDFromProgID(\"%s\") failed w/err 0x%08lx\n", progID, hr);
			throw CreateOfficeException(msg_buf);
		}
		OfficeTool::b_clsid_word = true;
	}
	return CreateTemplate(path.c_str(), L"Word", clsid_word, L"Documents");
}

int OfficeTool::CreateExcel(const wstring & path)
{
	HRESULT hr;
	if(!OfficeTool::b_clsid_excel)
	{ 
		LPCOLESTR progID = L"Excel.Application";
		hr = CLSIDFromProgID(progID, &clsid_excel);
		if(FAILED(hr))
		{
			swprintf_s(&msg_buf[0],1024,L"CLSIDFromProgID(\"%s\") failed w/err 0x%08lx\n", progID, hr);
			throw CreateOfficeException(msg_buf);
		}
		OfficeTool::b_clsid_excel = true;
	}
	return CreateTemplate(path.c_str(), L"Excel", clsid_excel, L"Workbooks");

}

int OfficeTool::CreatePPt(const wstring & path)
{
	HRESULT hr;
	if(!OfficeTool::b_clsid_ppt)
	{ 
		LPCOLESTR progID = L"PowerPoint.Application";
		hr = CLSIDFromProgID(progID, &clsid_ppt);
		if(FAILED(hr))
		{
			swprintf_s(&msg_buf[0],1024,L"CLSIDFromProgID(\"%s\") failed w/err 0x%08lx\n", progID, hr);
			throw CreateOfficeException(msg_buf);
		}
		OfficeTool::b_clsid_ppt = true;
	}
	//return CreateTemplate(path.c_str(), L"PowerPoint", clsid_ppt, L"Presentations", false);
	return CreateTemplate(path.c_str(), L"PowerPoint", clsid_ppt, L"Presentations");
}

int OfficeTool::CreateTemplate(LPCWSTR path, LPCWSTR name, const CLSID & clsid, LPCWSTR docsname, bool make_visible)
{
	HRESULT hr;
	IDispatch *pApp = NULL;
	hr = CoCreateInstance(      // [-or-] CoCreateInstanceEx, CoGetObject
		clsid,					// CLSID of the server
		NULL,
		CLSCTX_LOCAL_SERVER,    // Excel.Application is a local server
		IID_PPV_ARGS(&pApp));

	if (FAILED(hr))
	{
		swprintf_s(&msg_buf[0],1024,L"%s is not registered properly w/err 0x%08lx\n", name, hr);
		throw CreateOfficeException(msg_buf);
	}

	IDispatch *pDocs = NULL;
	IDispatch *pDoc = NULL;
	try
	{
		if(make_visible)
		{
			VARIANT x;
			x.vt = VT_I4;
			x.lVal = 0;
			AutoWrap(DISPATCH_PROPERTYPUT, NULL, pApp, L"Visible", 1, x);
		}
		// Get the Workbooks collection
		{
			VARIANT result;
			VariantInit(&result);
			AutoWrap(DISPATCH_PROPERTYGET, &result, pApp, (LPOLESTR)docsname, 0);
			pDocs = result.pdispVal;
		}
		// Call Workbooks.Add() to get a new workbook
		{
			VARIANT result;
			VariantInit(&result);
			AutoWrap(DISPATCH_METHOD, &result, pDocs, L"Add", 0);
			pDoc = result.pdispVal;
		}

		{
			VARIANT vtFileName;
			vtFileName.vt = VT_BSTR;
			vtFileName.bstrVal = SysAllocString(path);

			// If there are more than 1 parameters passed, they MUST be pass in 
			// reversed order. Otherwise, you may get the error 0x80020009.
			try{
				AutoWrap(DISPATCH_METHOD, NULL, pDoc, L"SaveAs", 1,vtFileName);
			}
			catch(CreateOfficeException)
			{
				VariantClear(&vtFileName);
				throw;
			}
			VariantClear(&vtFileName);
		}
		
		AutoWrap(DISPATCH_METHOD, NULL, pDoc, L"Close", 0);
		// Quit theapplication. (i.e. Application.Quit())
		AutoWrap(DISPATCH_METHOD, NULL, pApp, L"Quit", 0);
	}
	catch(CreateOfficeException)
	{
		if (pDoc != NULL) pDoc->Release();
		if (pDocs != NULL) pDocs->Release();
		if (pApp != NULL) pApp->Release();
		throw;
	}
	
	if (pDoc != NULL) pDoc->Release();
	if (pDocs != NULL) pDocs->Release();
	if (pApp != NULL) pApp->Release();
	return 0;
}
