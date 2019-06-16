#include "stdafx.h"
#include "Package.h"
#include "dte.h"

#include "GoToFileDlg.h"

void CGoToFilePackage::OnMyCommand(CommandHandler* /*pSender*/, DWORD /*flags*/, VARIANT* /*pIn*/, VARIANT* /*pOut*/)
{
	CComPtr<VxDTE::_DTE> spDTE;
	CComBSTR bstrVer;

	if (SUCCEEDED(GetVsSiteCache().QueryService(SID_SDTE, &spDTE)))
	{
		CGoToFileDlg goToFileDlg(spDTE);
		goToFileDlg.DoModal();
	}

	// Get the string for the title of the message box from the resource dll.
	//CComBSTR bstrTitle;
	//VSL_CHECKBOOL_GLE(bstrTitle.LoadStringW(_AtlBaseModule.GetResourceInstance(), IDS_PROJNAME));

	// Get a pointer to the UI Shell service to show the message box.
	//CComPtr<IVsUIShell> spUiShell = this->GetVsSiteCache().GetCachedService<IVsUIShell, SID_SVsUIShell>();

	//LONG lResult;
	//hr = spUiShell->ShowMessageBox(
	//	0,
	//	CLSID_NULL,
	//	bstrTitle,
	//	W2OLE(L"Hello World"),
	//	NULL,
	//	0,
	//	OLEMSGBUTTON_OK,
	//	OLEMSGDEFBUTTON_FIRST,
	//	OLEMSGICON_INFO,
	//	0,
	//	&lResult);
	//VSL_CHECKHRESULT(hr);
}

