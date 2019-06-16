/*
 *	GoToFile

 *	Copyright (C) 2009-2012 Ryan Gregg
 *
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "stdafx.h"
#include "GoToFileDlg.h"

//namespace
//{
//	static int CALLBACK BrowseCallback(HWND hwnd,UINT uMsg, LPARAM lParam, LPARAM lpData)
//	{
//		COpenNowSettings* pSettings = reinterpret_cast<COpenNowSettings*>(lpData);
//		switch(uMsg) 
//		{
//		case BFFM_INITIALIZED:
//			if(!pSettings->GetBrowsePath().empty())
//			{
//				SendMessage(hwnd, BFFM_SETSELECTION, TRUE, reinterpret_cast<LPARAM>(pSettings->GetBrowsePath().c_str()));
//			}
//			break;
//		}
//		return 0;
//	}
//}

int CGoToFileDlg::lpSortColumns[CGoToFileDlg::iMaxColumns] = { 0, 1, 2, 3 };
bool CGoToFileDlg::bSortDescending = false;

CGoToFileDlg::CGoToFileDlg(const CComPtr<VxDTE::_DTE>& spDTE)
	: bInitializing(true)
	, Settings(*this)
	, iInitialSize(0)
	, m_spDTE(spDTE)
{
	CreateFileList();
}

HICON CGoToFileDlg::LoadIcon(int iID, int iWidth, int iHeight)
{
	HINSTANCE hResourceInstance = _AtlBaseModule.GetResourceInstance();
	if(hResourceInstance)
	{
		HRSRC hResource = FindResource(hResourceInstance, MAKEINTRESOURCE(iID), RT_GROUP_ICON);
		if(hResource)
		{
			HGLOBAL hLoadedResource = LoadResource(hResourceInstance, hResource);
			if(hLoadedResource)
			{
				LPVOID lpLockedResource = LockResource(hLoadedResource);
				if(lpLockedResource)
				{
					int iIconID = LookupIconIdFromDirectoryEx(static_cast<PBYTE>(lpLockedResource), TRUE, iWidth, iHeight, LR_DEFAULTCOLOR);
					HRSRC hIconResource = FindResource(hResourceInstance, MAKEINTRESOURCE(iIconID), RT_ICON);
					if(hIconResource)
					{
						HGLOBAL hLoadedIconResource = LoadResource(hResourceInstance, hIconResource);
						if(hLoadedIconResource)
						{
							LPVOID lpLockedIconResource = LockResource(hLoadedIconResource);
							if(lpLockedIconResource)
							{
								DWORD dwSize = SizeofResource(hResourceInstance, hIconResource);
								return CreateIconFromResourceEx(static_cast<PBYTE>(lpLockedIconResource), dwSize, TRUE, 0x00030000, iWidth, iHeight, LR_DEFAULTCOLOR);
							}
						}
					}
				}
			}
		}
	}
	return NULL;
}

void CGoToFileDlg::CreateFileList()
{
	DestroyFileList();

	CComPtr<VxDTE::_Solution> pSolution;
	if(SUCCEEDED(m_spDTE->get_Solution(reinterpret_cast<VxDTE::Solution**>(&pSolution))) && pSolution)
	{
		SName< std::list<LPWSTR> > ProjectPath(ProjectPaths, NULL);

		CComPtr<VxDTE::Projects> pProjects;
		if(SUCCEEDED(pSolution->get_Projects(&pProjects)) && pProjects)
		{
			LONG lCount = 0;
			if(SUCCEEDED(pProjects->get_Count(&lCount)))
			{
				for(LONG i = 1; i <= lCount; i++)
				{
					CComPtr<VxDTE::Project> pProject;
					if(SUCCEEDED(pProjects->Item(CComVariant(i), &pProject)) && pProject)
					{
						CreateFileList(ProjectPath, pProject);
					}
				}
			}
		}
	}
}

void CGoToFileDlg::CreateFileList(SName< std::list<LPWSTR> >& ParentProjectPath, CComPtr<VxDTE::Project> pProject)
{
	BSTR lpProjectName = NULL;
	if(SUCCEEDED(pProject->get_Name(&lpProjectName)) && lpProjectName)
	{
		CComPtr<VxDTE::ProjectItems> pProjectItems;
		if(SUCCEEDED(pProject->get_ProjectItems(&pProjectItems)) && pProjectItems)
		{
			SName< std::vector<LPWSTR> > ProjectName(ProjectNames, lpProjectName);
			SName< std::list<LPWSTR> > ProjectPath(ParentProjectPath, lpProjectName);
			CreateFileList(ProjectName, ProjectPath, pProjectItems);
		}
	}
}

void CGoToFileDlg::CreateFileList(SName< std::vector<LPWSTR> >& ProjectName, SName< std::list<LPWSTR> >& ParentProjectPath, CComPtr<VxDTE::ProjectItems> pParentProjectItems)
{
	LONG lCount = 0;
	if(SUCCEEDED(pParentProjectItems->get_Count(&lCount)))
	{
		for(LONG i = 1; i <= lCount; i++)
		{
			CComPtr<VxDTE::ProjectItem> pProjectItem;
			if(SUCCEEDED(pParentProjectItems->Item(CComVariant(i), &pProjectItem)) && pProjectItem)
			{
				BSTR lpKind = NULL;
				if(SUCCEEDED(pProjectItem->get_Kind(&lpKind)) && lpKind)
				{
					WCHAR lpProjectItemKindPhysicalFile[MAX_PATH + 1];
					MultiByteToWideChar(CP_ACP, 0, VxDTE::vsProjectItemKindPhysicalFile, -1, lpProjectItemKindPhysicalFile, MAX_PATH + 1);
					lpProjectItemKindPhysicalFile[MAX_PATH] = '\0';

					if(_wcsicmp(lpKind, lpProjectItemKindPhysicalFile) == 0)
					{
						BSTR lpFilePath = NULL;
						if(SUCCEEDED(pProjectItem->get_FileNames(0, &lpFilePath)) && lpFilePath)
						{
							const size_t cchFilePath = wcslen(lpFilePath) + 1;
							LPWSTR lpFilePathCopy = new WCHAR[cchFilePath];
							wcscpy_s(lpFilePathCopy, cchFilePath, lpFilePath);
							Files.push_back(SFile(lpFilePathCopy, ProjectName.GetName(), ParentProjectPath.GetName()));
						}
					}
					else
					{
						CComPtr<VxDTE::ProjectItems> pProjectItems;
						if(SUCCEEDED(pProjectItem->get_ProjectItems(&pProjectItems)) && pProjectItems)
						{
							BSTR lpFolderName = NULL;
							if(SUCCEEDED(pProjectItem->get_Name(&lpFolderName)) && lpFolderName)
							{
								SName< std::list<LPWSTR> > ProjectPath(ParentProjectPath, lpFolderName);
								CreateFileList(ProjectName, ProjectPath, pProjectItems);
							}
						}
						else
						{
							CComPtr<VxDTE::Project> pProject;
							if(SUCCEEDED(pProjectItem->get_SubProject(&pProject)) && pProject)
							{
								CreateFileList(ParentProjectPath, pProject);
							}
						}
					}
				}
			}
		}
	}
}

void CGoToFileDlg::DestroyFileList()
{
	for(std::vector<LPWSTR>::iterator i = ProjectNames.begin(); i != ProjectNames.end(); ++i)
	{
		delete []*i;
	}
	ProjectNames.clear();

	for(std::list<LPWSTR>::iterator i = ProjectPaths.begin(); i != ProjectPaths.end(); ++i)
	{
		delete []*i;
	}
	ProjectPaths.clear();

	for(std::list<SFile>::iterator i = Files.begin(); i != Files.end(); ++i)
	{
		delete [](*i).lpFilePath;
	}
	Files.clear();
}

void CGoToFileDlg::CreateBrowseFileList()
{
	//DestroyBrowseFileList();

	//BROWSEINFO BrowseInfo;
	//ZeroMemory(&BrowseInfo, sizeof(BROWSEINFO));
	//BrowseInfo.hwndOwner = m_hWnd;
	//BrowseInfo.lParam = reinterpret_cast<LPARAM>(&Settings);
	//BrowseInfo.lpfn = BrowseCallback;
	//BrowseInfo.lpszTitle = L"Select a folder to browse:";
	//BrowseInfo.ulFlags = BIF_NEWDIALOGSTYLE | BIF_NONEWFOLDERBUTTON;

	//LPITEMIDLIST pFolder = SHBrowseForFolder(&BrowseInfo);
	//WCHAR lpPath[MAX_PATH];
	//if(pFolder != NULL && SHGetPathFromIDList(pFolder, lpPath))
	//{
	//	Settings.SetBrowsePath(lpPath);
	//	CreateBrowseFileList(lpPath);
	//}
}

void CGoToFileDlg::CreateBrowseFileList(LPCWSTR lpPath)
{
	const size_t cchSearch = wcslen(lpPath) + 2 + 1;
	LPWSTR lpSearch = new WCHAR[cchSearch];
	wcscpy_s(lpSearch, cchSearch, lpPath);
	wcscat_s(lpSearch, cchSearch, L"\\*");

	WIN32_FIND_DATA FindData;
	HANDLE hFindFile = FindFirstFile(lpSearch, &FindData);
	if(hFindFile != INVALID_HANDLE_VALUE)
	{
		do
		{
			if((FindData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) == 0)
			{
				bool bIgnore = true;
				for(LPCWSTR i = FindData.cFileName; *i != L'\0'; i++)
				{
					if(*i != L'.')
					{
						bIgnore = false;
						break;
					}
				}
				if(!bIgnore)
				{
					const size_t cchFileName = wcslen(lpPath) + 1 + wcslen(FindData.cFileName) + 1;
					LPWSTR lpFileName = new WCHAR[cchFileName];
					wcscpy_s(lpFileName, cchFileName, lpPath);
					wcscat_s(lpFileName, cchFileName, L"\\");
					wcscat_s(lpFileName, cchFileName, FindData.cFileName);
					if((FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
					{
						CreateBrowseFileList(lpFileName);
						delete []lpFileName;
					}
					else
					{
						BrowseFiles.push_back(SFile(lpFileName, L"<Browse>", lpFileName));
					}
				}
			}
		} while(FindNextFile(hFindFile, &FindData));
		FindClose(hFindFile);
	}

	delete []lpSearch;
}

void CGoToFileDlg::DestroyBrowseFileList()
{
	for(std::list<SFile>::iterator i = BrowseFiles.begin(); i != BrowseFiles.end(); ++i)
	{
		delete [](*i).lpFilePath;
	}
	BrowseFiles.clear();
}

LPCWSTR CGoToFileDlg::GetKnownFilterName(EKnownFilter eKnownFilter) const
{
	static LPCWSTR lpKnownFilterNames[] =
	{
		L"<All Projects>",
		L"<Browse...>",
	};
	if(eKnownFilter >= 0 && eKnownFilter < KNOWN_FILTER_COUNT)
	{
		return lpKnownFilterNames[eKnownFilter];
	}
	return NULL;
}

unsigned int CGoToFileDlg::GetSelectedProject()
{
	HWND hProjects = GetDlgItem(IDC_PROJECTS);
	if(hProjects)
	{
		LRESULT iItem = ::SendMessage(hProjects, CB_GETCURSEL, 0, 0);
		if(iItem > 0)
		{
			return static_cast<unsigned int>(iItem);
		}
	}
	return 0;
}

int CGoToFileDlg::CompareProjects(const void *pProject1, const void *pProject2)
{
	return _wcsicmp(*static_cast<const LPCWSTR*>(pProject1), *static_cast<const LPCWSTR*>(pProject2));
}

void CGoToFileDlg::RefreshProjectList()
{
	if(ProjectNames.size() > 1)
	{
		qsort(&ProjectNames.front(), ProjectNames.size(), sizeof(LPWSTR), &CGoToFileDlg::CompareProjects);
	}

	HWND hProjects = GetDlgItem(IDC_PROJECTS);
	if(!hProjects)
	{
		return;
	}

	::SendMessage(hProjects, CB_RESETCONTENT, 0, 0);

	COMBOBOXEXITEM Item;
	memset(&Item, 0, sizeof(COMBOBOXEXITEM));
	Item.mask = CBEIF_TEXT;
	
	Item.pszText = const_cast<LPWSTR>(GetKnownFilterName(KNOWN_FILTER_ALL_PROJECTS));
	Item.cchTextMax = static_cast<int>(wcslen(Item.pszText));
	::SendMessage(hProjects, CBEM_INSERTITEM, 0, reinterpret_cast<LPARAM>(&Item));

	Item.pszText = const_cast<LPWSTR>(GetKnownFilterName(KNOWN_FILTER_BROWSE));
	Item.cchTextMax = static_cast<int>(wcslen(Item.pszText));
	Item.iItem++;
	::SendMessage(hProjects, CBEM_INSERTITEM, 0, reinterpret_cast<LPARAM>(&Item));

	Item.pszText = LPSTR_TEXTCALLBACK;
	Item.cchTextMax = 0;
	for(std::vector<LPWSTR>::iterator i = ProjectNames.begin(); i != ProjectNames.end(); ++i)
	{
		Item.iItem++;
		::SendMessage(hProjects, CBEM_INSERTITEM, 0, reinterpret_cast<LPARAM>(&Item));
	}

	/*RECT ProjectsRect;
	::GetClientRect(hProjects, &ProjectsRect);
	RECT ProjectDropDownRect;
	::SendMessage(hProjects, CB_GETDROPPEDCONTROLRECT, 0, reinterpret_cast<LPARAM>(&ProjectDropDownRect));
	LPARAM iHeight = ::SendMessage(hProjects, CB_GETITEMHEIGHT, -1, 0);
	long iItems = std::min<long>(static_cast<long>(Item.iItem) + 1, 8);
	
	::MapWindowPoints(HWND_DESKTOP, *this, reinterpret_cast<LPPOINT>(&ProjectDropDownRect), 2);
	ProjectDropDownRect.bottom = ProjectDropDownRect.top + (ProjectsRect.bottom - ProjectsRect.top) + iHeight * iItems;
	::MoveWindow(hProjects, ProjectDropDownRect.left, ProjectDropDownRect.top, ProjectDropDownRect.right - ProjectDropDownRect.left, ProjectDropDownRect.bottom - ProjectDropDownRect.top, FALSE);

	ProjectsAnchor.Adjust(ProjectDropDownRect);*/

	::SendMessage(hProjects, CB_SETCURSEL, 0, 0);
}

int CGoToFileDlg::CompareFiles(const void *pFile1, const void *pFile2)
{
	int iResult = 0;

	for(int i = 0; i < iMaxColumns && iResult == 0; i++)
	{
		switch(CGoToFileDlg::lpSortColumns[i])
		{
		case 0:
			iResult = _wcsicmp(static_cast<const SFilteredFile*>(pFile1)->pFile->lpFilePath + static_cast<const SFilteredFile*>(pFile1)->pFile->uiFileName, static_cast<const SFilteredFile*>(pFile2)->pFile->lpFilePath + static_cast<const SFilteredFile*>(pFile2)->pFile->uiFileName);
			break;
		case 1:
			iResult = _wcsicmp(static_cast<const SFilteredFile*>(pFile1)->pFile->lpFilePath, static_cast<const SFilteredFile*>(pFile2)->pFile->lpFilePath);
			break;
		case 2:
			iResult = _wcsicmp(static_cast<const SFilteredFile*>(pFile1)->pFile->lpProjectName, static_cast<const SFilteredFile*>(pFile2)->pFile->lpProjectName);
			break;
		case 3:
			iResult = _wcsicmp(static_cast<const SFilteredFile*>(pFile1)->pFile->lpProjectPath, static_cast<const SFilteredFile*>(pFile2)->pFile->lpProjectPath);
			break;
		}
	}

	if(CGoToFileDlg::bSortDescending)
	{
		iResult = -iResult;
	}

	return iResult;
}

void CGoToFileDlg::SortFileList()
{
	if(FilteredFiles.size() > 1)
	{
		qsort(&FilteredFiles.front(), FilteredFiles.size(), sizeof(SFilteredFile), &CGoToFileDlg::CompareFiles);
	}
}

void CGoToFileDlg::RefreshFileList()
{
	HWND hFiles = GetDlgItem(IDC_FILES);
	if(!hFiles)
	{
		return;
	}

	unsigned int uiProjectIndex = GetSelectedProject();
	LPCWSTR lpProjectName = uiProjectIndex >= static_cast<unsigned int>(KNOWN_FILTER_COUNT) && uiProjectIndex < static_cast<unsigned int>(KNOWN_FILTER_COUNT) + ProjectNames.size() ? ProjectNames[uiProjectIndex - static_cast<unsigned int>(KNOWN_FILTER_COUNT)] : NULL;
	const std::list<SFile>& FilesToFilter = uiProjectIndex == static_cast<unsigned int>(KNOWN_FILTER_BROWSE) ? BrowseFiles : Files;

	LPWSTR lpFilterStringTable = NULL;
	std::list<SFilter> Filters;
	CreateFilterList(lpFilterStringTable, Filters);

	FilteredFiles.clear();
	FilteredFiles.reserve(FilesToFilter.size());

	unsigned int uiHasTerms = FILTER_TERM_NONE;
	for(std::list<SFilter>::const_iterator j = Filters.begin(); j != Filters.end(); ++j)
	{
		uiHasTerms |= (*j).GetFilterTerm();
	}

	int iBestMatch = -1;
	for(std::list<SFile>::const_iterator i = FilesToFilter.begin(); i != FilesToFilter.end(); ++i)
	{
		const SFile& File = *i;

		if(lpProjectName != NULL && File.lpProjectName != lpProjectName)
		{
			continue;
		}

		int iMatch = -1;
		unsigned int uiMatchTerms = uiHasTerms & FILTER_TERM_AND_MASK;
		for(std::list<SFilter>::iterator j = Filters.begin(); j != Filters.end(); ++j)
		{
			const SFilter& Filter = *j;

			int iTest = Filter.Match(File);
			unsigned int uiFilterTerm = Filter.GetFilterTerm();
			if(iTest >= 0)
			{
				if(iMatch < 0 || iTest < iMatch)
				{
					iMatch = iTest;
				}
				if(uiFilterTerm & FILTER_TERM_OR_MASK)
				{
					uiMatchTerms |= uiFilterTerm;
				}
			}
			else
			{
				if(uiFilterTerm & FILTER_TERM_AND_MASK)
				{
					uiMatchTerms &= ~uiFilterTerm;
				}
			}
		}
		if(((uiHasTerms & FILTER_TERM_OR_MASK) == 0 || (uiHasTerms & FILTER_TERM_OR_MASK & uiMatchTerms) != 0) &&
			((uiHasTerms & FILTER_TERM_AND_MASK) == 0 || (uiHasTerms & FILTER_TERM_AND_MASK) == (uiMatchTerms & FILTER_TERM_AND_MASK)))
		{
			if(iBestMatch < 0 || (iMatch >= 0 && iMatch < iBestMatch))
			{
				iBestMatch = iMatch;
			}
			FilteredFiles.push_back(SFilteredFile(&File, iMatch));
		}
	}

	DestroyFilterList(lpFilterStringTable, Filters);

	SortFileList();

	int iItem = 0;
	int iBestItem = -1;
	LVITEM Item;
	memset(&Item, 0, sizeof(LVITEM));
	Item.mask = LVIF_TEXT;
	Item.pszText = LPSTR_TEXTCALLBACK;
	int iCount = ListView_GetItemCount(hFiles);
	ListView_SetItemCountEx(hFiles, FilteredFiles.size(), 0);
	for(std::vector<SFilteredFile>::iterator i = FilteredFiles.begin(); i != FilteredFiles.end(); ++i)
	{
		const SFilteredFile& FilteredFile = *i;

		Item.iItem = iItem++;

		if(Item.iItem >= iCount)
		{
			Item.iSubItem = 0;
			ListView_SetItem(hFiles, &Item);

			Item.iSubItem = 1;
			ListView_SetItem(hFiles, &Item);

			Item.iSubItem = 2;
			ListView_SetItem(hFiles, &Item);

			Item.iSubItem = 3;
			ListView_SetItem(hFiles, &Item);
		}

		if(iBestItem == -1 || FilteredFile.iMatch < iBestMatch)
		{
			iBestItem = Item.iItem;
			iBestMatch = FilteredFile.iMatch;
		}
	}

	Select(iBestItem);

	const WCHAR* lpWindowCaption = L"Open File(s) in Solution";
	if(Files.size() > 0)
	{
		WCHAR lpWindowText[64];
		swprintf_s(lpWindowText, 64, L"%s (%u of %u)", lpWindowCaption, FilteredFiles.size(), Files.size());
		SetWindowText(lpWindowText);
	}
	else
	{
		SetWindowText(lpWindowCaption);
	}	
}

void CGoToFileDlg::ExploreSelectedFiles()
{
	HWND hFiles = GetDlgItem(IDC_FILES);
	if(!hFiles)
	{
		return;
	}

	typedef HRESULT (WINAPI *SHOpenFolderAndSelectItemsProc)(LPITEMIDLIST pidlFolder, UINT cidl, LPITEMIDLIST *apidl, DWORD dwFlags);

	static bool bInitializedShell32 = false;
	static SHOpenFolderAndSelectItemsProc pSHOpenFolderAndSelectItems = NULL;
	if(!bInitializedShell32)
	{
		bInitializedShell32 = true;

		HMODULE hShell32 = GetModuleHandle(L"shell32.dll");
		if(!hShell32)
		{
			return;
		}

		pSHOpenFolderAndSelectItems = reinterpret_cast<SHOpenFolderAndSelectItemsProc>(GetProcAddress(hShell32, "SHOpenFolderAndSelectItems"));
	}

	if(pSHOpenFolderAndSelectItems == NULL)
	{
		return;
	}

	CComPtr<IShellFolder> pDesktop;
	if(FAILED(SHGetDesktopFolder(&pDesktop)))
	{
		return;
	}

	std::list<SFolderAndSelectedFiles*> FoldersAndSelectedFiles;

	int iItem = -1;
	while((iItem = ListView_GetNextItem(hFiles, iItem, LVNI_SELECTED)) >= 0)
	{
		if(iItem < static_cast<int>(FilteredFiles.size()))
		{
			const SFilteredFile& FilteredFile = FilteredFiles[iItem];

			bool bFound = false;
			for(std::list<SFolderAndSelectedFiles*>::iterator i = FoldersAndSelectedFiles.begin(); i != FoldersAndSelectedFiles.end(); ++i)
			{
				SFolderAndSelectedFiles* pFolderAndSelectedFiles = *i;
				if(pFolderAndSelectedFiles->Contains(*FilteredFile.pFile))
				{
					pFolderAndSelectedFiles->SelectedFiles.push_back(FilteredFile.pFile);
					bFound = true;
					break;
				}
			}
			if(!bFound)
			{
				FoldersAndSelectedFiles.push_back(new SFolderAndSelectedFiles(*FilteredFile.pFile));
			}
		}
	}

	for(std::list<SFolderAndSelectedFiles*>::iterator i = FoldersAndSelectedFiles.begin(); i != FoldersAndSelectedFiles.end(); ++i)
	{
		SFolderAndSelectedFiles* pFolderAndSelectedFiles = *i;

		WCHAR iChar;
		LPWSTR lpFolder = const_cast<LPWSTR>(pFolderAndSelectedFiles->Folder.lpFilePath);
		iChar = lpFolder[pFolderAndSelectedFiles->Folder.uiFileName];
		lpFolder[pFolderAndSelectedFiles->Folder.uiFileName] = L'\0';

		LPITEMIDLIST pFolder = NULL;
		if(SUCCEEDED(pDesktop->ParseDisplayName(NULL, NULL, lpFolder, NULL, &pFolder, NULL)))
		{
			lpFolder[pFolderAndSelectedFiles->Folder.uiFileName] = iChar;

			UINT uiCount = 0;
			LPITEMIDLIST* lpFiles = new LPITEMIDLIST[pFolderAndSelectedFiles->SelectedFiles.size()];
			memset(lpFiles, 0, sizeof(LPITEMIDLIST) * pFolderAndSelectedFiles->SelectedFiles.size());
			for(std::list<const SFile*>::iterator j = pFolderAndSelectedFiles->SelectedFiles.begin(); j != pFolderAndSelectedFiles->SelectedFiles.end(); ++j)
			{
				if(SUCCEEDED(pDesktop->ParseDisplayName(NULL, NULL, (*j)->lpFilePath, NULL, &lpFiles[uiCount], NULL)))
				{
					uiCount++;
				}
			}

			if(uiCount > 0)
			{
				pSHOpenFolderAndSelectItems(pFolder, uiCount, lpFiles, 0);
			}

			for(UINT j = 0; j < uiCount; j++)
			{
				CoTaskMemFree(lpFiles[j]);
			}

			CoTaskMemFree(pFolder);
		}

		lpFolder[pFolderAndSelectedFiles->Folder.uiFileName] = iChar;

		delete pFolderAndSelectedFiles;
	}
}

void CGoToFileDlg::Select(int iItem)
{
	CWindow Files = GetDlgItem(IDC_FILES);
	HWND hFiles = Files;
	if(!hFiles)
	{
		return;
	}

	ListView_SetItemState(hFiles, -1, 0, LVIS_SELECTED);
	if(iItem >= 0)
	{
		ListView_SetItemState(hFiles, iItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		RECT ItemRect;
		if(ListView_GetItemRect(hFiles, iItem, &ItemRect, LVIR_BOUNDS))
		{
			RECT FilesRect;
			if(Files.GetClientRect(&FilesRect))
			{
				// Center selection in window.
				ItemRect.top -= (FilesRect.bottom - FilesRect.top) / 2;
			}
			ListView_Scroll(hFiles, 0, ItemRect.top);
		}
	}
	else
	{
		ListView_Scroll(hFiles, 0, 0);
	}
}

bool CGoToFileDlg::OpenSelectedFiles()
{
	HWND hFiles = GetDlgItem(IDC_FILES);
	if(!hFiles)
	{
		return false;
	}

	bool bResult = false;

	const EViewKind eViewKind = Button_GetCheck(GetDlgItem(IDC_VIEWCODE)) ? VIEW_KIND_CODE : VIEW_KIND_PRIMARY;
	WCHAR lpViewKind[MAX_PATH + 1];
	switch(eViewKind)
	{
	case VIEW_KIND_CODE:
		MultiByteToWideChar(CP_ACP, 0, VxDTE::vsViewKindCode, -1, lpViewKind, MAX_PATH + 1);
		break;
	case VIEW_KIND_DESIGNER:
		MultiByteToWideChar(CP_ACP, 0, VxDTE::vsViewKindDesigner, -1, lpViewKind, MAX_PATH + 1);
		break;
	case VIEW_KIND_TEXTVIEW:
		MultiByteToWideChar(CP_ACP, 0, VxDTE::vsViewKindTextView, -1, lpViewKind, MAX_PATH + 1);
		break;
	case VIEW_KIND_DEBUGGING:
		MultiByteToWideChar(CP_ACP, 0, VxDTE::vsViewKindDebugging, -1, lpViewKind, MAX_PATH + 1);
		break;
	default:
		MultiByteToWideChar(CP_ACP, 0, VxDTE::vsViewKindPrimary, -1, lpViewKind, MAX_PATH + 1);
		break;
	}
	lpViewKind[MAX_PATH] = '\0';

	BSTR lpViewKindBStr = SysAllocString(lpViewKind);
	if(lpViewKindBStr != NULL)
	{
		CComPtr<VxDTE::ItemOperations> pItemOperations;
		if(SUCCEEDED(m_spDTE->get_ItemOperations(&pItemOperations)) && pItemOperations)
		{
			int iItem = -1;
			while((iItem = ListView_GetNextItem(hFiles, iItem, LVNI_SELECTED)) >= 0)
			{
				if(iItem < static_cast<int>(FilteredFiles.size()))
				{
					const SFilteredFile& FilteredFile = FilteredFiles[iItem];

					BSTR lpFilePathBStr = SysAllocString(FilteredFile.pFile->lpFilePath);
					if(lpFilePathBStr != NULL)
					{
						CComPtr<VxDTE::Window> pWindow;
						pItemOperations->OpenFile(lpFilePathBStr, lpViewKindBStr, &pWindow);

						SysFreeString(lpFilePathBStr);
					}

					bResult = true;
				}
			}
		}

		SysFreeString(lpViewKindBStr);
	}

	return bResult;
}

void CGoToFileDlg::CreateFilterList(LPWSTR& lpFilterStringTable, std::list<SFilter>& Filters)
{
	DestroyFilterList(lpFilterStringTable, Filters);

	BSTR lpFilter = NULL;
	GetDlgItem(IDC_FILTER).GetWindowText(lpFilter);
	if(lpFilter)
	{
		const size_t cchFilter = wcslen(lpFilter) + 1;
		lpFilterStringTable = new WCHAR[cchFilter];
		wcscpy_s(lpFilterStringTable, cchFilter, lpFilter);

		LPWSTR lpFilter = NULL;
		ESearchField eSearchField = SEARCH_FIELD_FILE_NAME;
		ELogicOperator eLogicOperator = LOGIC_OPERATOR_NONE;
		bool bNot = false;
		bool bQuoted = false;

		bool bParse = true;
		for(LPWSTR pChar = lpFilterStringTable; bParse; pChar++)
		{
			bool bDone = false;
			switch(*pChar)
			{
			case L'\0':
				bParse = false;
				bDone = true;
				break;
			case L'&':
				if(lpFilter == NULL && eLogicOperator == LOGIC_OPERATOR_NONE && !bQuoted)
				{
					eLogicOperator = LOGIC_OPERATOR_AND;
					continue;
				}
				break;
			case L'|':
				if(lpFilter == NULL && eLogicOperator == LOGIC_OPERATOR_NONE && !bQuoted)
				{
					eLogicOperator = LOGIC_OPERATOR_OR;
					continue;
				}
				break;
			case L'-':
			case L'!':
				if(lpFilter == NULL && !bNot && !bQuoted)
				{
					bNot = true;
					continue;
				}
				break;
			case L'"':
				if(lpFilter == NULL && !bQuoted)
				{
					bQuoted = true;
					continue;
				}
				else if(bQuoted)
				{
					bDone = true;
				}
				break;
			case L'\\':
			case L'/':
				if(lpFilter == NULL && !bQuoted)
				{
					if(eSearchField == SEARCH_FIELD_FILE_NAME)
					{
						eSearchField = SEARCH_FIELD_FILE_PATH;
					}
					else if(eSearchField == SEARCH_FIELD_PROJECT_NAME)
					{
						eSearchField = SEARCH_FIELD_PROJECT_PATH;
					}
					continue;
				}
				break;
			case L':':
				if(lpFilter == NULL && !bQuoted)
				{
					if(eSearchField == SEARCH_FIELD_FILE_NAME)
					{
						eSearchField = SEARCH_FIELD_PROJECT_NAME;
					}
					else if(eSearchField == SEARCH_FIELD_FILE_PATH)
					{
						eSearchField = SEARCH_FIELD_PROJECT_PATH;
					}
					continue;
				}
				break;
			case L' ':
			case L'\t':
			case L'\r':
			case L'\n':
				if(!bQuoted)
				{
					bDone = true;
				}
				break;
			}
			if(bDone)
			{
				if(lpFilter != NULL && *lpFilter != L'\0')
				{
					if(eLogicOperator == LOGIC_OPERATOR_NONE)
					{
						eLogicOperator = LOGIC_OPERATOR_AND;
					}
					if(bNot)
					{
						eLogicOperator = static_cast<ELogicOperator>(eLogicOperator << 1);
					}
					*pChar = L'\0';
					Filters.push_back(SFilter(lpFilter, eSearchField, eLogicOperator));
				}
				lpFilter = NULL;
				eSearchField = SEARCH_FIELD_FILE_NAME;
				eLogicOperator = LOGIC_OPERATOR_NONE;
				bNot = false;
				bQuoted = false;
			}
			else if(lpFilter == NULL)
			{
				lpFilter = pChar;
			}
		}
	}
}

void CGoToFileDlg::DestroyFilterList(LPWSTR& lpFilterStringTable, std::list<SFilter>& Filters)
{
	delete []lpFilterStringTable;
	lpFilterStringTable = NULL;

	Filters.clear();
}

WCHAR CGoToFileDlg::SFilter::Normalize(WCHAR cChar, ESearchField eSearchField)
{
	cChar = tolower(cChar);
	if((eSearchField == SEARCH_FIELD_FILE_PATH || eSearchField == SEARCH_FIELD_PROJECT_PATH) && cChar == L'/')
	{
		cChar = L'\\';
	}
	return cChar;
}

int CGoToFileDlg::SFilter::Like(LPCWSTR lpSearch, LPCWSTR lpFilter, ESearchField eSearchField)
{
	LPCWSTR lpSearchStart = lpSearch;
	LPCWSTR lpSearchMatch = NULL;

	while(*lpFilter)
	{
		if(*lpFilter == L'*')
		{
			if(lpFilter[1] == L'*')
			{
				lpFilter++;
				continue;
			}
			else if(lpFilter[1] == L'\0')
			{
				return 0;
			}
			else
			{
				lpFilter++;
				while(*lpSearch)
				{
					int iResult = Like(lpSearch, lpFilter, eSearchField);
					if(iResult >= 0)
					{
						return lpSearchMatch ? static_cast<int>(lpSearchMatch - lpSearchStart) : static_cast<int>(lpSearch - lpSearchStart) + iResult;
					}
					lpSearch++;
				}

				return -1;
			}
		}
		else if(*lpFilter == L'?')
		{
			if(*lpSearch == L'\0')
			{
				return -1;
			}
			lpFilter++;
			lpSearch++;
		}
		else
		{
			if(*lpSearch == L'\0')
			{
				return -1;
			}
			else if(*lpFilter != Normalize(*lpSearch, eSearchField))
			{
				return -1;
			}
			if(lpSearchMatch == NULL)
			{
				lpSearchMatch = lpSearch;
			}
			lpFilter++;
			lpSearch++;
		}
	}

	if(*lpSearch == L'\0')
	{
		return lpSearchMatch ? static_cast<int>(lpSearchMatch - lpSearchStart) : 0;
	}
	return -1;
}

int CGoToFileDlg::SFilter::Match(const SFile& File) const
{
	LPCWSTR lpSearch;
	switch(eSearchField)
	{
	case SEARCH_FIELD_FILE_NAME:
		lpSearch = File.lpFilePath + File.uiFileName;
		break;
	case SEARCH_FIELD_FILE_PATH:
		lpSearch = File.lpFilePath;
		break;
	case SEARCH_FIELD_PROJECT_NAME:
		lpSearch = File.lpProjectName;
		break;
	case SEARCH_FIELD_PROJECT_PATH:
		lpSearch = File.lpProjectPath;
		break;
	default:
		return -1;
	}

	int iMatch = -1;

	size_t iFileLength = wcslen(lpSearch);
	size_t iFilterLength = wcslen(lpFilter);
	if(bWildcard)
	{
		iMatch = Like(lpSearch, lpFilter, eSearchField);
	}
	else if(iFilterLength <= iFileLength)
	{
		for(size_t i = 0; i <= iFileLength - iFilterLength; i++)
		{
			bool bSubMatch = true;
			for(size_t j = 0; j < iFilterLength; j++)
			{
				if(Normalize(lpSearch[i + j], eSearchField) != lpFilter[j])
				{
					bSubMatch = false;
					break;
				}
			}
			if(bSubMatch)
			{
				iMatch = static_cast<int>(i);
				break;
			}
		}
	}

	if(eLogicOperator & LOGIC_OPERATOR_NOT_MASK)
	{
		if(iMatch < 0)
		{
			iMatch = static_cast<int>(iFileLength - iFilterLength);
		}
		else
		{
			iMatch = -1;
		}
	}

	return iMatch;
}

LRESULT CALLBACK CGoToFileDlg::FilterProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CGoToFileDlg::SWndProc* pWndProc = CGoToFileDlg::GetWndProc(hWnd);
	if(pWndProc)
	{
		if(uMsg == WM_KEYDOWN)
		{
			switch(wParam)
			{
			case VK_DOWN:
			case VK_UP:
			case VK_NEXT:
			case VK_PRIOR:
				SendMessage(pWndProc->OpenNow.GetDlgItem(IDC_FILES), uMsg, wParam, lParam);
				return 0;
			}
		}
		return CallWindowProc(pWndProc->pWndProc, hWnd, uMsg, wParam, lParam);
	}
	return 0;
}

std::list<CGoToFileDlg::SWndProc*> CGoToFileDlg::WndProcs;

CGoToFileDlg::SWndProc* CGoToFileDlg::GetWndProc(HWND hWnd)
{
	for(std::list<CGoToFileDlg::SWndProc*>::iterator i = CGoToFileDlg::WndProcs.begin(); i != CGoToFileDlg::WndProcs.end(); i++)
	{
		CGoToFileDlg::SWndProc* pWndProc = *i;
		if(pWndProc->hWnd == hWnd)
		{
			return pWndProc;
		}
	}
	return NULL;
}

void CGoToFileDlg::RegisterWndProc(CGoToFileDlg& OpenNow, HWND hWnd, WNDPROC pWndProc)
{
	CGoToFileDlg::WndProcs.push_back(new SWndProc(OpenNow, hWnd, pWndProc));
}

void CGoToFileDlg::UnregisterWndProc(CGoToFileDlg* pOpenNow)
{
	for(std::list<CGoToFileDlg::SWndProc*>::iterator i = CGoToFileDlg::WndProcs.begin(); i != CGoToFileDlg::WndProcs.end(); i++)
	{
		CGoToFileDlg::SWndProc* pWndProc = *i;
		if(&pWndProc->OpenNow == pOpenNow)
		{
			delete pWndProc;
			CGoToFileDlg::WndProcs.erase(i);
			return;
		}
	}
}
