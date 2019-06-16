#include "stdafx.h"
#include "Package.h"
#include "dte.h"

#include "GoToFileDlg.h"
#include "GoToComplementary.h"

void CGoToFilePackage::OnGoToFileCommand(CommandHandler* /*pSender*/, DWORD /*flags*/, VARIANT* /*pIn*/, VARIANT* /*pOut*/)
{
	CComPtr<VxDTE::_DTE> spDTE;
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
}

void CGoToFilePackage::OnGoToComplementaryCommand(CommandHandler* /*pSender*/, DWORD /*flags*/, VARIANT* /*pIn*/, VARIANT* /*pOut*/)
{
	CComPtr<VxDTE::_DTE> spDTE;
	if (SUCCEEDED(GetVsSiteCache().QueryService(SID_SDTE, &spDTE)))
	{
		CGoToComplementary gotoComplementary(spDTE);
		gotoComplementary.Execute();
	}
}


