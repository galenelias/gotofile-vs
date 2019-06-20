// Package.h

#pragma once

#include <atlstr.h>
#include <VSLCommandTarget.h>


#include "resource.h"       // main symbols
#include "Guids.h"
#include "..\GoToFileUI\Resource.h"

#include "..\GoToFileUI\CommandIds.h"

#include <vector>
#include <string>


//using namespace VSL;


class ATL_NO_VTABLE CGoToFilePackage : 
	// CComObjectRootEx and CComCoClass are used to implement a non-thread safe COM object, and 
	// a partial implementation for IUnknown (the COM map below provides the rest).
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CGoToFilePackage, &CLSID_GoToFile>,
	// Provides the implementation for IVsPackage to make this COM object into a VS Package.
	public IVsPackageImpl<CGoToFilePackage, &CLSID_GoToFile>,
	public IOleCommandTargetImpl<CGoToFilePackage>,
	// Provides consumers of this object with the ability to determine which interfaces support
	// extended error information.
	public ATL::ISupportErrorInfoImpl<&__uuidof(IVsPackage)>
{
public:

// Provides a portion of the implementation of IUnknown, in particular the list of interfaces
// the CGoToFilePackage object will support via QueryInterface
BEGIN_COM_MAP(CGoToFilePackage)
	COM_INTERFACE_ENTRY(IVsPackage)
	COM_INTERFACE_ENTRY(IOleCommandTarget)
	COM_INTERFACE_ENTRY(ISupportErrorInfo)
END_COM_MAP()

// COM objects typically should not be cloned, and this prevents cloning by declaring the 
// copy constructor and assignment operator private (NOTE:  this macro includes the declaration of
// a private section, so everything following this macro and preceding a public or protected 
// section will be private).
VSL_DECLARE_NOT_COPYABLE(CGoToFilePackage)

public:
	CGoToFilePackage()
	{
	}
	
	~CGoToFilePackage()
	{
	}


	// This function provides the error information if it is not possible to load
	// the UI dll. It is for this reason that the resource IDS_E_BADINSTALL must
	// be defined inside this dll's resources.
	static const LoadUILibrary::ExtendedErrorInfo& GetLoadUILibraryErrorInfo()
	{
		static LoadUILibrary::ExtendedErrorInfo errorInfo(IDS_E_BADINSTALL);
		return errorInfo;
	}

	// DLL is registered with VS via a pkgdef file. Don't do anything if asked to
	// self-register.
	static HRESULT WINAPI UpdateRegistry(BOOL /*bRegister*/)
	{
		return S_OK;
	}

// NOTE - the arguments passed to these macros can not have names longer then 30 characters
// Definition of the commands handled by this package
	VSL_BEGIN_COMMAND_MAP()
		VSL_COMMAND_MAP_ENTRY(CLSID_GoToFileCmdSet, cmdidGotoFile, NULL, CommandHandler::ExecHandler(&OnGoToFileCommand))
		VSL_COMMAND_MAP_ENTRY(CLSID_GoToFileCmdSet, cmdidGotoComplementary, NULL, CommandHandler::ExecHandler(&OnGoToComplementaryCommand))
	VSL_END_VSCOMMAND_MAP()


	// Command handler called when the user selects the "My Command" command.
	void OnGoToFileCommand(CommandHandler* /*pSender*/, DWORD /*flags*/, VARIANT* /*pIn*/, VARIANT* /*pOut*/);
	void OnGoToComplementaryCommand(CommandHandler* /*pSender*/, DWORD /*flags*/, VARIANT* /*pIn*/, VARIANT* /*pOut*/);

};

// This exposes CGoToFilePackage for instantiation via DllGetClassObject; however, an instance
// can not be created by CoCreateInstance, as CGoToFilePackage is specifically registered with
// VS, not the the system in general.
OBJECT_ENTRY_AUTO(CLSID_GoToFile, CGoToFilePackage)
