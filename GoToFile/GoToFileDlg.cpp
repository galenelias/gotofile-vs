/*
 *	GoToFile

 *	Copyright (C) 2009-2012 Ryan Gregg
 *	Copyright (C) 2019 Galen Elias
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
#include "SelectProjectsDlg.h"
#include "Stopwatch.h"
#include "shellapi.h"
#include <string_view>
#include <iterator>
#include <optional>

static int CALLBACK BrowseCallback(HWND hwnd, UINT uMsg, LPARAM /*lParam*/, LPARAM lpData)
{
	GoToFileSettings* pSettings = reinterpret_cast<GoToFileSettings*>(lpData);
	switch (uMsg)
	{
		case BFFM_INITIALIZED:
			if (!pSettings->GetBrowsePath().empty())
			{
				SendMessage(hwnd, BFFM_SETSELECTION, TRUE, reinterpret_cast<LPARAM>(pSettings->GetBrowsePath().c_str()));
			}
			break;
	}
	return 0;
}

int CGoToFileDlg::s_lpSortColumns[CGoToFileDlg::s_iMaxColumns] = { 0, 1, 2, 3 };
bool CGoToFileDlg::s_bSortDescending = false;

CGoToFileDlg::CGoToFileDlg(const CComPtr<VxDTE::_DTE>& spDTE)
	: m_bInitializing(true)
	, m_settings(*this)
	, m_spDTE(spDTE)
{
	CreateFileList();
}

CGoToFileDlg::~CGoToFileDlg()
{
	DestroyFileList();
	DestroyBrowseFileList();

	m_logfile.close();
}




LRESULT CGoToFileDlg::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CAxDialogImpl<CGoToFileDlg>::OnInitDialog(uMsg, wParam, lParam, bHandled);

	HICON hIcon = LoadIcon(1, 32, 32);
	if (hIcon)
	{
		SetIcon(hIcon);
	}

	CWindow wndFiles = GetDlgItem(IDC_FILES);

	const DWORD dwExtendedStyle = LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP;
	ListView_SetExtendedListViewStyleEx(wndFiles, dwExtendedStyle, dwExtendedStyle);

	LVCOLUMN column;
	memset(&column, 0, sizeof(LVCOLUMN));
	column.mask = LVCF_TEXT | LVCF_WIDTH;

	column.pszText = L"File Name";
	column.cx = 150;
	ListView_InsertColumn(wndFiles, 0, &column);

	column.pszText = L"File Path";
	column.cx = 250;
	ListView_InsertColumn(wndFiles, 1, &column);

	column.pszText = L"Project Name";
	column.cx = 150;
	ListView_InsertColumn(wndFiles, 2, &column);

	column.pszText = L"Project Path";
	column.cx = 250;
	ListView_InsertColumn(wndFiles, 3, &column);

	RECT clientRect;
	if (GetClientRect(&clientRect))
	{
		m_iInitialWidth = static_cast<WORD>(clientRect.right - clientRect.left);
		m_iInitialHeight = static_cast<WORD>(clientRect.bottom - clientRect.top);
	}

	CWindow wndFilter = GetDlgItem(IDC_FILTER);
	if (wndFilter)
	{
		wndFilter.SetFocus();

		// Enable shell keyboard hooks to get functionality like Ctrl+Backspace to work on our edit box
		SHAutoComplete(wndFilter, SHACF_AUTOSUGGEST_FORCE_OFF);

		WNDPROC pWndProc = reinterpret_cast<WNDPROC>(::SetWindowLongPtr(wndFilter, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(CGoToFileDlg::FilterProc)));
		if (pWndProc)
		{
			RegisterWndProc(*this, wndFilter, pWndProc);
		}
	}

	m_filesAnchor.Init(*this, wndFiles, EAnchor::Top | EAnchor::Bottom | EAnchor::Left | EAnchor::Right);
	m_filterAnchor.Init(*this, wndFilter, EAnchor::Bottom | EAnchor::Left | EAnchor::Right);
	m_projectsAnchor.Init(*this, GetDlgItem(IDC_PROJECTS), EAnchor::Top | EAnchor::Left | EAnchor::Right);
	m_viewCodeAnchor.Init(*this, GetDlgItem(IDC_VIEWCODE), EAnchor::Bottom | EAnchor::Left);
	m_exploreAnchor.Init(*this, GetDlgItem(IDC_EXPLORE), EAnchor::Bottom | EAnchor::Right);
	m_openAnchor.Init(*this, GetDlgItem(IDOK), EAnchor::Bottom | EAnchor::Right);
	m_cancelAnchor.Init(*this, GetDlgItem(IDCANCEL), EAnchor::Bottom | EAnchor::Right);
	m_usageAnchor.Init(*this, GetDlgItem(IDC_USAGE_LINK), EAnchor::Bottom | EAnchor::Right);

	RefreshProjectList();

	m_settings.Store();
	m_settings.Read();
	m_settings.Restore();

	if (m_settings.IsLoggingEnabled())
		InitializeLogFile();

	// Log out previously recorded initial CreateFileList time
	LogToFile(L"CreateFileList (%zu): Ran in %llu microseconds", m_files.size(), m_createFileListTime);

	if (GetSelectedProject() == static_cast<unsigned int>(KNOWN_FILTER_BROWSE))
	{
		Stopwatch stopwatch;

		CreateBrowseFileList(m_settings.GetBrowsePath().c_str());

		auto elapsedMicros = stopwatch.GetElapsedMicroseconds();
		LogToFile(L"CreateBrowseFileList (%zu): Ran in %llu microseconds", m_browseFiles.size(), elapsedMicros);
	}

	RefreshFileList();

	SetupUsageControl();

	m_bInitializing = false;

	bHandled = TRUE;
	return 0;
}


LRESULT CGoToFileDlg::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CWindow wndFilter = GetDlgItem(IDC_FILTER);
	if (wndFilter)
	{
		CGoToFileDlg::SWndProc* pWndProc = CGoToFileDlg::GetWndProc(wndFilter);
		if (pWndProc)
		{
			::SetWindowLongPtr(wndFilter, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(pWndProc->pWndProc));
		}
	}
	UnregisterWndProc(this);

	m_settings.Store();
	m_settings.Write();

	DeleteObject(m_tooltipFont);

	return CAxDialogImpl<CGoToFileDlg>::OnDestroy(uMsg, wParam, lParam, bHandled);
}

LRESULT CGoToFileDlg::OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
{
	const LONG iWidth = static_cast<LONG>(LOWORD(lParam));
	const LONG iHeight = static_cast<LONG>(HIWORD(lParam));

	const LONG iDeltaX = iWidth - m_iInitialWidth;
	const LONG iDeltaY = iHeight - m_iInitialHeight;

	m_filesAnchor.Update(iDeltaX, iDeltaY);
	m_filterAnchor.Update(iDeltaX, iDeltaY);
	m_projectsAnchor.Update(iDeltaX, iDeltaY);
	m_viewCodeAnchor.Update(iDeltaX, iDeltaY);
	m_exploreAnchor.Update(iDeltaX, iDeltaY);
	m_openAnchor.Update(iDeltaX, iDeltaY);
	m_cancelAnchor.Update(iDeltaX, iDeltaY);
	m_usageAnchor.Update(iDeltaX, iDeltaY);
	Invalidate();

	bHandled = TRUE;
	return 0;
}

LRESULT CGoToFileDlg::OnClickedExplore(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ExploreSelectedFiles();
	EndDialog(wID);
	return 0;
}

LRESULT CGoToFileDlg::OnClickedOpen(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	OpenSelectedFiles();
	EndDialog(wID);
	return 0;
}

LRESULT CGoToFileDlg::OnClickedCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	EndDialog(wID);
	return 0;
}

LRESULT CGoToFileDlg::OnChangeFilter(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!m_bInitializing && !m_settings.Restoring())
	{
		RefreshFileList();
	}
	return 0;
}

LRESULT CGoToFileDlg::OnGetDispInfoFiles(int /*idCtrl*/, LPNMHDR pNHM, BOOL& /*bHandled*/)
{
	LV_DISPINFO* pDispInfo = reinterpret_cast<LV_DISPINFO*>(pNHM);
	if (pDispInfo->item.mask & LVIF_TEXT && pDispInfo->item.cchTextMax > 0)
	{
		if (pDispInfo->item.iItem >= 0 && pDispInfo->item.iItem < (LPARAM)m_filteredFiles.size())
		{
			const SFilteredFile& FilteredFile = m_filteredFiles[pDispInfo->item.iItem];
			switch (pDispInfo->item.iSubItem)
			{
				case 0:
					wcsncpy_s(pDispInfo->item.pszText, pDispInfo->item.cchTextMax, FilteredFile.pFile->spFilePath.get() + FilteredFile.pFile->uiFileName, pDispInfo->item.cchTextMax);
					pDispInfo->item.pszText[pDispInfo->item.cchTextMax - 1] = L'\0';
					break;
				case 1:
					wcsncpy_s(pDispInfo->item.pszText, pDispInfo->item.cchTextMax, FilteredFile.pFile->spFilePath.get(), pDispInfo->item.cchTextMax);
					pDispInfo->item.pszText[pDispInfo->item.cchTextMax - 1] = L'\0';
					break;
				case 2:
					wcsncpy_s(pDispInfo->item.pszText, pDispInfo->item.cchTextMax, FilteredFile.pFile->lpProjectName, pDispInfo->item.cchTextMax);
					pDispInfo->item.pszText[pDispInfo->item.cchTextMax - 1] = L'\0';
					break;
				case 3:
					wcsncpy_s(pDispInfo->item.pszText, pDispInfo->item.cchTextMax, FilteredFile.pFile->lpProjectPath, pDispInfo->item.cchTextMax);
					pDispInfo->item.pszText[pDispInfo->item.cchTextMax - 1] = L'\0';
					break;
			}
		}
	}
	return 0;
}

LRESULT CGoToFileDlg::OnGetInfoTipFiles(int /*idCtrl*/, LPNMHDR pNHM, BOOL& /*bHandled*/)
{
	LPNMLVGETINFOTIP pInfoTip = reinterpret_cast<LPNMLVGETINFOTIP>(pNHM);
	if (pInfoTip->cchTextMax > 0)
	{
		if (pInfoTip->iItem >= 0 && static_cast<size_t>(pInfoTip->iItem) < m_filteredFiles.size())
		{
			const SFilteredFile& FilteredFile = m_filteredFiles[pInfoTip->iItem];
			switch (pInfoTip->iSubItem)
			{
				case 0:
					wcsncpy_s(pInfoTip->pszText, pInfoTip->cchTextMax, FilteredFile.pFile->spFilePath.get(), pInfoTip->cchTextMax);
					pInfoTip->pszText[pInfoTip->cchTextMax - 1] = L'\0';
					break;
				case 2:
					wcsncpy_s(pInfoTip->pszText, pInfoTip->cchTextMax, FilteredFile.pFile->lpProjectPath, pInfoTip->cchTextMax);
					pInfoTip->pszText[pInfoTip->cchTextMax - 1] = L'\0';
					break;
			}
		}
	}
	return 0;
}

LRESULT CGoToFileDlg::OnColumnClickFiles(int /*idCtrl*/, LPNMHDR pNHM, BOOL& /*bHandled*/)
{
	NM_LISTVIEW FAR* pColumnClick = reinterpret_cast<NM_LISTVIEW FAR*>(pNHM);

	for (int i = 0; i < s_iMaxColumns; i++)
	{
		if (s_lpSortColumns[i] == pColumnClick->iSubItem)
		{
			if (i == 0)
			{
				s_bSortDescending = !s_bSortDescending;
			}
			else
			{
				for (int j = 0; j < i; j++)
				{
					s_lpSortColumns[i - j] = s_lpSortColumns[i - j - 1];
				}
				s_lpSortColumns[0] = pColumnClick->iSubItem;
			}
			break;
		}
	}
	RefreshFileList();

	return 0;
}

LRESULT CGoToFileDlg::OnDoubleClickFiles(int /*idCtrl*/, LPNMHDR /*pNHM*/, BOOL& /*bHandled*/)
{
	if (OpenSelectedFiles())
	{
		EndDialog(0);
	}

	return 0;
}

LRESULT CGoToFileDlg::OnGetDispInfoProjects(int /*idCtrl*/, LPNMHDR pNHM, BOOL& /*bHandled*/)
{
	PNMCOMBOBOXEX pDispInfo = reinterpret_cast<PNMCOMBOBOXEX>(pNHM);
	if (pDispInfo->ceItem.mask & CBEIF_TEXT && pDispInfo->ceItem.cchTextMax > 0)
	{
		if (pDispInfo->ceItem.iItem >= static_cast<INT>(KNOWN_FILTER_COUNT) && static_cast<size_t>(pDispInfo->ceItem.iItem) < static_cast<INT>(KNOWN_FILTER_COUNT) + m_projectNames.size())
		{
			LPCWSTR lpProject = m_projectNames[pDispInfo->ceItem.iItem - static_cast<INT>(KNOWN_FILTER_COUNT)].get();

			wcsncpy_s(pDispInfo->ceItem.pszText, pDispInfo->ceItem.cchTextMax, lpProject, pDispInfo->ceItem.cchTextMax);
			pDispInfo->ceItem.pszText[pDispInfo->ceItem.cchTextMax - 1] = L'\0';
		}
	}
	return 0;
}

LRESULT CGoToFileDlg::OnSelChangedProjects(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!m_bInitializing && !m_settings.Restoring())
	{
		if (GetSelectedProject() == static_cast<unsigned int>(KNOWN_FILTER_BROWSE))
		{
			CreateBrowseFileList();
		}
		else if (GetSelectedProject() == static_cast<unsigned int>(KNOWN_FILTER_SELECT_PROJECTS))
		{
			CSelectProjectsDlg selectProjectsDlg(m_projectNames, m_selectedProjects);
			if (selectProjectsDlg.DoModal() == IDOK)
			{
				m_selectedProjects = selectProjectsDlg.GetSelectedProjects();
			}
		}

		RefreshFileList();
	}
	return 0;
}

HICON CGoToFileDlg::LoadIcon(int iID, int iWidth, int iHeight)
{
	HINSTANCE hResourceInstance = _AtlBaseModule.GetResourceInstance();
	if (hResourceInstance)
	{
		HRSRC hResource = FindResource(hResourceInstance, MAKEINTRESOURCE(iID), RT_GROUP_ICON);
		if (hResource)
		{
			HGLOBAL hLoadedResource = LoadResource(hResourceInstance, hResource);
			if (hLoadedResource)
			{
				LPVOID lpLockedResource = LockResource(hLoadedResource);
				if (lpLockedResource)
				{
					int iIconID = LookupIconIdFromDirectoryEx(static_cast<PBYTE>(lpLockedResource), TRUE, iWidth, iHeight, LR_DEFAULTCOLOR);
					HRSRC hIconResource = FindResource(hResourceInstance, MAKEINTRESOURCE(iIconID), RT_ICON);
					if (hIconResource)
					{
						HGLOBAL hLoadedIconResource = LoadResource(hResourceInstance, hIconResource);
						if (hLoadedIconResource)
						{
							LPVOID lpLockedIconResource = LockResource(hLoadedIconResource);
							if (lpLockedIconResource)
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
	return nullptr;
}

static WCHAR s_lpProjectItemKindPhysicalFile[MAX_PATH + 1];
static WCHAR s_lpProjectItemKindSolutionItemFile[MAX_PATH + 1];
static WCHAR s_lpProjectKindSolutionItems[MAX_PATH + 1];
static WCHAR s_isProjectKindsInitialized = false;

void CGoToFileDlg::CreateFileList()
{
	Stopwatch stopwatch;

	DestroyFileList();

	// Get a wide string version of the specific project kinds we care about, but only do the conversion once
	if (!s_isProjectKindsInitialized)
	{
		MultiByteToWideChar(CP_ACP, 0, VxDTE::vsProjectItemKindPhysicalFile, -1, s_lpProjectItemKindPhysicalFile, MAX_PATH + 1);
		s_lpProjectItemKindPhysicalFile[MAX_PATH] = '\0';

		MultiByteToWideChar(CP_ACP, 0, VxDTE::vsProjectItemKindSolutionItems, -1, s_lpProjectItemKindSolutionItemFile, MAX_PATH + 1);
		s_lpProjectItemKindSolutionItemFile[MAX_PATH] = '\0';

		MultiByteToWideChar(CP_ACP, 0, VxDTE::vsProjectKindSolutionItems, -1, s_lpProjectKindSolutionItems, MAX_PATH + 1);
		s_lpProjectKindSolutionItems[MAX_PATH] = '\0';

		s_isProjectKindsInitialized = true;
	}

	CComPtr<VxDTE::_Solution> spSolution;
	if (SUCCEEDED(m_spDTE->get_Solution(reinterpret_cast<VxDTE::Solution**>(&spSolution))) && spSolution)
	{
		SName<std::vector<std::unique_ptr<WCHAR[]>>> projectPath(m_projectPaths, nullptr);

		CComPtr<VxDTE::Projects> spProjects;
		if (SUCCEEDED(spSolution->get_Projects(&spProjects)) && spProjects)
		{
			LONG lCount = 0;
			if (SUCCEEDED(spProjects->get_Count(&lCount)))
			{
				for (LONG i = 1; i <= lCount; i++)
				{
					CComPtr<VxDTE::Project> spProject;
					if (SUCCEEDED(spProjects->Item(CComVariant(i), &spProject)) && spProject)
					{
						CreateFileList(projectPath, spProject);
					}
				}
			}
		}
	}

	// We can't log here, as our settings haven't been loaded yet, so just record this and then potentially log it later
	m_createFileListTime = stopwatch.GetElapsedMicroseconds();
}

void CGoToFileDlg::CreateFileList(SName<std::vector<std::unique_ptr<WCHAR[]>>>& parentProjectPath, VxDTE::Project* pProject)
{
	CComBSTR spProjectName;

	if (SUCCEEDED(pProject->get_Name(&spProjectName)) && spProjectName)
	{
		CComPtr<VxDTE::ProjectItems> spProjectItems;
		if (SUCCEEDED(pProject->get_ProjectItems(&spProjectItems)) && spProjectItems)
		{
			SName<std::vector<std::unique_ptr<WCHAR[]>>> projectName(m_projectNames, spProjectName);
			SName<std::vector<std::unique_ptr<WCHAR[]>>> projectPath(parentProjectPath, spProjectName);
			CreateFileList(projectName, projectPath, spProjectItems);
		}
	}
}

void CGoToFileDlg::CreateFileList(SName<std::vector<std::unique_ptr<WCHAR[]>>>& projectName, SName<std::vector<std::unique_ptr<WCHAR[]>>>& parentProjectPath, VxDTE::ProjectItems* pParentProjectItems)
{
	LONG lCount = 0;
	if (SUCCEEDED(pParentProjectItems->get_Count(&lCount)))
	{
		m_files.reserve(m_files.size() + lCount);
		for (LONG i = 1; i <= lCount; i++)
		{
			CComPtr<VxDTE::ProjectItem> spProjectItem;
			if (SUCCEEDED(pParentProjectItems->Item(CComVariant(i), &spProjectItem)) && spProjectItem)
			{
				CComBSTR spKind;
				if (SUCCEEDED(spProjectItem->get_Kind(&spKind)) && spKind)
				{
					// Solution Level Items show up as different kind than physical files, despite being physical files
					if (_wcsicmp(spKind, s_lpProjectItemKindPhysicalFile) == 0
						|| _wcsicmp(spKind, s_lpProjectItemKindSolutionItemFile) == 0)
					{
						short fileCount = 0;
						[[maybe_unused]] HRESULT hr = spProjectItem->get_FileCount(&fileCount);

						CComBSTR spFilePath;
						if (SUCCEEDED(spProjectItem->get_FileNames(1, &spFilePath)) && spFilePath)
						{
							const size_t cchFilePath = spFilePath.Length() + 1;
							std::unique_ptr<WCHAR[]> spFilePathCopy = std::make_unique<WCHAR[]>(cchFilePath);
							wcscpy_s(spFilePathCopy.get(), cchFilePath, spFilePath);
							m_files.emplace_back(std::move(spFilePathCopy), projectName.GetName(), parentProjectPath.GetName());
						}

						// Sub-folders under Solution Items appear as SubProjects, but we want to treat them more like just a tradtional hierarchy item
						// so expand them explicitly here
						if (spFilePath == nullptr && _wcsicmp(spKind, s_lpProjectItemKindSolutionItemFile) == 0)
						{
							CComPtr<VxDTE::Project> spProject;
							if (SUCCEEDED(spProjectItem->get_SubProject(&spProject)) && spProject)
							{

								CComBSTR spProjectName;
								if (SUCCEEDED(spProject->get_Name(&spProjectName)) && spProjectName)
								{
									CComPtr<VxDTE::ProjectItems> spProjectItems;
									if (SUCCEEDED(spProject->get_ProjectItems(&spProjectItems)) && spProjectItems)
									{
										SName<std::vector<std::unique_ptr<WCHAR[]>>> projectPath(parentProjectPath, spProjectName);

										CComBSTR spSubProjectKind;
										spProject->get_Kind(&spSubProjectKind);

										// For sub-folders of solution items, just group then under the parent 'project name' rather than the sub-folder name
										// Not sure if this is what people expect, but seems excessive to project for each sub-folder
										// However, for actual sub-projects which reside under solution item folders, pull those into a new top level project name
										if (spSubProjectKind && _wcsicmp(spSubProjectKind, s_lpProjectKindSolutionItems) == 0)
										{
											CreateFileList(projectName, projectPath, spProjectItems);
										}
										else
										{
											SName<std::vector<std::unique_ptr<WCHAR[]>>> subProjectName(m_projectNames, spProjectName);
											CreateFileList(subProjectName, projectPath, spProjectItems);
										}
									}
								}
							}
						}
					}
					else
					{
						CComPtr<VxDTE::ProjectItems> spProjectItems;
						if (SUCCEEDED(spProjectItem->get_ProjectItems(&spProjectItems)) && spProjectItems)
						{
							CComBSTR spFolderName;
							if (SUCCEEDED(spProjectItem->get_Name(&spFolderName)) && spFolderName)
							{
								SName<std::vector<std::unique_ptr<WCHAR[]>>> projectPath(parentProjectPath, spFolderName);
								CreateFileList(projectName, projectPath, spProjectItems);
							}
						}
						else
						{
							CComPtr<VxDTE::Project> spProject;
							if (SUCCEEDED(spProjectItem->get_SubProject(&spProject)) && spProject)
							{
								CreateFileList(parentProjectPath, spProject);
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
	m_projectNames.clear();
	m_projectPaths.clear();
	m_files.clear();
}

void CGoToFileDlg::CreateBrowseFileList()
{
	DestroyBrowseFileList();

	BROWSEINFO BrowseInfo;
	ZeroMemory(&BrowseInfo, sizeof(BROWSEINFO));
	BrowseInfo.hwndOwner = m_hWnd;
	BrowseInfo.lParam = reinterpret_cast<LPARAM>(&m_settings);
	BrowseInfo.lpfn = BrowseCallback;
	BrowseInfo.lpszTitle = L"Select a folder to browse:";
	BrowseInfo.ulFlags = BIF_NEWDIALOGSTYLE | BIF_NONEWFOLDERBUTTON;

	LPITEMIDLIST pFolder = SHBrowseForFolder(&BrowseInfo);
	WCHAR lpPath[MAX_PATH];
	if (pFolder != nullptr && SHGetPathFromIDList(pFolder, lpPath))
	{
		m_settings.SetBrowsePath(lpPath);

		Stopwatch stopwatch;
		CreateBrowseFileList(lpPath);
		LogToFile(L"CreateBrowseFileList (%zu): Ran in %llu microseconds", m_browseFiles.size(), stopwatch.GetElapsedMicroseconds());
	}
}

void CGoToFileDlg::CreateBrowseFileList(LPCWSTR lpPath)
{
	std::wstring wstSearch = std::wstring(lpPath) + L"\\*";

	WIN32_FIND_DATA findData;
	HANDLE hFindFile = FindFirstFile(wstSearch.c_str(), &findData);
	if (hFindFile != INVALID_HANDLE_VALUE)
	{
		do
		{
			if ((findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) == 0)
			{
				bool bIgnore = true;
				for (LPCWSTR i = findData.cFileName; *i != L'\0'; i++)
				{
					if (*i != L'.')
					{
						bIgnore = false;
						break;
					}
				}

				if (!bIgnore)
				{
					const size_t cchFileName = wcslen(lpPath) + 1 + wcslen(findData.cFileName) + 1;
					std::unique_ptr<WCHAR[]> spFileName = std::make_unique<WCHAR[]>(cchFileName);
					wcscpy_s(spFileName.get(), cchFileName, lpPath);
					wcscat_s(spFileName.get(), cchFileName, L"\\");
					wcscat_s(spFileName.get(), cchFileName, findData.cFileName);
					if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
					{
						CreateBrowseFileList(spFileName.get());
					}
					else
					{
						LPCWSTR pwzFileName = spFileName.get();
						m_browseFiles.emplace_back(std::move(spFileName), L"<Browse>", pwzFileName);
					}
				}
			}
		} while (FindNextFile(hFindFile, &findData));

		FindClose(hFindFile);
	}
}

void CGoToFileDlg::DestroyBrowseFileList()
{
	m_browseFiles.clear();
}

LPCWSTR CGoToFileDlg::GetKnownFilterName(EKnownFilter eKnownFilter) const
{
	static constexpr LPCWSTR lpKnownFilterNames[] =
	{
		L"<All Projects>",
		L"<Browse...>",
		L"<Select Projects...>",
	};

	if (eKnownFilter >= 0 && eKnownFilter < KNOWN_FILTER_COUNT)
	{
		return lpKnownFilterNames[eKnownFilter];
	}
	return nullptr;
}

unsigned int CGoToFileDlg::GetSelectedProject()
{
	HWND hProjects = GetDlgItem(IDC_PROJECTS);
	if (hProjects)
	{
		LRESULT iItem = ::SendMessage(hProjects, CB_GETCURSEL, 0, 0);
		if (iItem > 0)
		{
			return static_cast<unsigned int>(iItem);
		}
	}
	return 0;
}

const std::optional<std::vector<const WCHAR*>>& CGoToFileDlg::GetSelectedProjects() const
{
	return m_selectedProjects;
}


void CGoToFileDlg::SetSelectedProjects(const std::vector<std::wstring_view>& selectedProjectsViews)
{
	// For each project in 'selectedProjectsVector', find the index of the project in 'm_projectNames' and set the corresponding index in 'm_selectedProjects' to true
	m_selectedProjects.reset();
	std::vector<const WCHAR*> selectedProjects;
	for (const auto& selectedProject : selectedProjectsViews)
	{
		for (size_t i = 0; i < m_projectNames.size(); ++i)
		{
			if (selectedProject == m_projectNames[i].get())
			{
				selectedProjects.push_back(m_projectNames[i].get());
				break;
			}
		}
	}

	if (!selectedProjects.empty())
	{
		m_selectedProjects = std::move(selectedProjects);
	}
}


static bool CompareProjects(const std::unique_ptr<WCHAR[]>& spProject1, const std::unique_ptr<WCHAR[]>& spProject2)
{
	return _wcsicmp(spProject1.get(), spProject2.get()) < 0;
}


void CGoToFileDlg::RefreshProjectList()
{
	if (m_projectNames.size() > 1)
	{
		std::sort(m_projectNames.begin(), m_projectNames.end(), &CompareProjects);
	}

	HWND hProjects = GetDlgItem(IDC_PROJECTS);
	if (!hProjects)
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

	Item.pszText = const_cast<LPWSTR>(GetKnownFilterName(KNOWN_FILTER_SELECT_PROJECTS));
	Item.cchTextMax = static_cast<int>(wcslen(Item.pszText));
	Item.iItem++;
	::SendMessage(hProjects, CBEM_INSERTITEM, 0, reinterpret_cast<LPARAM>(&Item));

	Item.pszText = LPSTR_TEXTCALLBACK;
	Item.cchTextMax = 0;

	for (size_t i = 0; i < m_projectNames.size(); ++i)
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


bool CGoToFileDlg::CompareFiles(const SFilteredFile& file1, const SFilteredFile& file2)
{
	int iResult = 0;

	for (int i = 0; i < s_iMaxColumns && iResult == 0; i++)
	{
		switch (CGoToFileDlg::s_lpSortColumns[i])
		{
			case 0:
				iResult = _wcsicmp(file1.pFile->spFilePath.get() + file1.pFile->uiFileName, file2.pFile->spFilePath.get() + file2.pFile->uiFileName);
				break;
			case 1:
				iResult = _wcsicmp(file1.pFile->spFilePath.get(), file2.pFile->spFilePath.get());
				break;
			case 2:
				iResult = _wcsicmp(file1.pFile->lpProjectName, file2.pFile->lpProjectName);
				break;
			case 3:
				iResult = _wcsicmp(file1.pFile->lpProjectPath, file2.pFile->lpProjectPath);
				break;
		}
	}

	if (CGoToFileDlg::s_bSortDescending)
	{
		iResult = -iResult;
	}

	return iResult < 0;
}


void CGoToFileDlg::SortFileList()
{
	if (m_filteredFiles.size() > 1)
	{
		std::sort(m_filteredFiles.begin(), m_filteredFiles.end(), &CGoToFileDlg::CompareFiles);
	}
}


void CGoToFileDlg::RefreshFileList()
{
	HWND hFiles = GetDlgItem(IDC_FILES);
	if (!hFiles)
		return;

	Stopwatch stopwatch;

	unsigned int uiProjectIndex = GetSelectedProject();
	LPCWSTR lpProjectName = uiProjectIndex >= static_cast<unsigned int>(KNOWN_FILTER_COUNT) && uiProjectIndex < static_cast<unsigned int>(KNOWN_FILTER_COUNT) + m_projectNames.size() ? m_projectNames[uiProjectIndex - static_cast<unsigned int>(KNOWN_FILTER_COUNT)].get() : nullptr;
	const std::vector<SFile>& filesToFilter = uiProjectIndex == static_cast<unsigned int>(KNOWN_FILTER_BROWSE) ? m_browseFiles : m_files;

	bool filterOnSelectedProjects = false;
	if (uiProjectIndex == KNOWN_FILTER_SELECT_PROJECTS)
	{
		if (m_selectedProjects.has_value())
		{
			filterOnSelectedProjects = true;
		}
	}

	std::unique_ptr<WCHAR[]> spFilterStringTable;
	std::vector<SFilter> filters;

	CComBSTR spFilter;
	GetDlgItem(IDC_FILTER).GetWindowText(&spFilter);

	if (spFilter)
		CreateFilterList(spFilter, spFilterStringTable, filters, &m_fileLineDestination, &m_fileColumnDestination);

	m_filteredFiles.clear();
	m_filteredFiles.reserve(filesToFilter.size());

	unsigned int uiHasTerms = FILTER_TERM_NONE;
	for (const SFilter& filter : filters)
	{
		uiHasTerms |= filter.GetFilterTerm();
	}

	int iBestMatch = -1;
	for (const SFile& file : filesToFilter)
	{
		if (lpProjectName != nullptr && file.lpProjectName != lpProjectName)
		{
			continue;
		}

		if (filterOnSelectedProjects)
		{
			if (std::find(m_selectedProjects->begin(), m_selectedProjects->end(), file.lpProjectName) == m_selectedProjects->end())
			{
				continue;
			}
		}

		int iMatch = -1;
		unsigned int uiMatchTerms = uiHasTerms & FILTER_TERM_AND_MASK;
		for (const SFilter& filter : filters)
		{
			const int iTest = filter.Match(file);
			const unsigned int uiFilterTerm = filter.GetFilterTerm();
			if (iTest >= 0)
			{
				if (iMatch < 0 || iTest < iMatch)
				{
					iMatch = iTest;
				}
				if (uiFilterTerm & FILTER_TERM_OR_MASK)
				{
					uiMatchTerms |= uiFilterTerm;
				}
			}
			else
			{
				if (uiFilterTerm & FILTER_TERM_AND_MASK)
				{
					uiMatchTerms &= ~uiFilterTerm;
				}
			}
		}
		if (((uiHasTerms & FILTER_TERM_OR_MASK) == 0 || (uiHasTerms & FILTER_TERM_OR_MASK & uiMatchTerms) != 0) &&
			((uiHasTerms & FILTER_TERM_AND_MASK) == 0 || (uiHasTerms & FILTER_TERM_AND_MASK) == (uiMatchTerms & FILTER_TERM_AND_MASK)))
		{
			if (iBestMatch < 0 || (iMatch >= 0 && iMatch < iBestMatch))
			{
				iBestMatch = iMatch;
			}
			m_filteredFiles.emplace_back(&file, iMatch);
		}
	}

	SortFileList();

	int iItem = 0;
	int iBestItem = -1;
	ListView_SetItemCountEx(hFiles, m_filteredFiles.size(), 0);

	for (const SFilteredFile& filteredFile : m_filteredFiles)
	{
		if (iBestItem == -1 || filteredFile.iMatch < iBestMatch)
		{
			iBestItem = iItem;
			iBestMatch = filteredFile.iMatch;
		}

		iItem++;
	}

	Select(iBestItem);

	const WCHAR* lpWindowCaption = L"Go To File(s) in Solution";
	if (m_files.size() > 0)
	{
		WCHAR lpWindowText[128];
		swprintf_s(lpWindowText, _countof(lpWindowText), L"%s (%zu of %zu)", lpWindowCaption, m_filteredFiles.size(), m_files.size());
		SetWindowText(lpWindowText);
	}
	else
	{
		SetWindowText(lpWindowCaption);
	}

	LogToFile(L"RefreshFileList (%zu of %zu): Ran in %llu microseconds", m_filteredFiles.size(), m_files.size(), stopwatch.GetElapsedMicroseconds());
}


void CGoToFileDlg::ExploreSelectedFiles()
{
	HWND hFiles = GetDlgItem(IDC_FILES);
	if (!hFiles)
	{
		return;
	}

	CComPtr<IShellFolder> spDesktop;
	if (FAILED(SHGetDesktopFolder(&spDesktop)))
		return;

	std::vector<SFolderAndSelectedFiles> foldersAndSelectedFiles;

	int iItem = -1;
	while ((iItem = ListView_GetNextItem(hFiles, iItem, LVNI_SELECTED)) >= 0)
	{
		if (iItem < static_cast<int>(m_filteredFiles.size()))
		{
			const SFilteredFile& FilteredFile = m_filteredFiles[iItem];

			bool bFound = false;
			for (SFolderAndSelectedFiles& folderAndSelectedFiles : foldersAndSelectedFiles)
			{
				if (folderAndSelectedFiles.Contains(*FilteredFile.pFile))
				{
					folderAndSelectedFiles.SelectedFiles.push_back(FilteredFile.pFile);
					bFound = true;
					break;
				}
			}
			if (!bFound)
			{
				foldersAndSelectedFiles.emplace_back(*FilteredFile.pFile);
			}
		}
	}

	for (SFolderAndSelectedFiles& folderAndSelectedFiles : foldersAndSelectedFiles)
	{
		WCHAR iChar;
		LPWSTR lpFolder = const_cast<LPWSTR>(folderAndSelectedFiles.Folder.spFilePath.get());
		iChar = lpFolder[folderAndSelectedFiles.Folder.uiFileName];
		lpFolder[folderAndSelectedFiles.Folder.uiFileName] = L'\0';

		LPITEMIDLIST pFolder = nullptr;
		if (SUCCEEDED(spDesktop->ParseDisplayName(nullptr, nullptr, lpFolder, nullptr, &pFolder, nullptr)))
		{
			lpFolder[folderAndSelectedFiles.Folder.uiFileName] = iChar;

			UINT uiCount = 0;

			std::vector<LPITEMIDLIST> itemIds(folderAndSelectedFiles.SelectedFiles.size());
			for (const SFile* pFile : folderAndSelectedFiles.SelectedFiles)
			{
				if (SUCCEEDED(spDesktop->ParseDisplayName(nullptr, nullptr, pFile->spFilePath.get(), nullptr, &itemIds[uiCount], nullptr)))
				{
					uiCount++;
				}
			}

			if (uiCount > 0)
			{
				LPCITEMIDLIST* pConstItemList = const_cast<LPCITEMIDLIST*>(itemIds.data());
				SHOpenFolderAndSelectItems(pFolder, uiCount, pConstItemList, 0);
			}

			for (LPITEMIDLIST itemId : itemIds)
			{
				CoTaskMemFree(itemId);
			}

			CoTaskMemFree(pFolder);
		}

		lpFolder[folderAndSelectedFiles.Folder.uiFileName] = iChar;
	}
}


void CGoToFileDlg::Select(int iItem)
{
	CWindow wndFiles = GetDlgItem(IDC_FILES);
	HWND hFiles = wndFiles;
	if (!hFiles)
	{
		return;
	}

	ListView_SetItemState(hFiles, -1, 0, LVIS_SELECTED);
	if (iItem >= 0)
	{
		ListView_SetItemState(hFiles, iItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		RECT ItemRect;
		if (ListView_GetItemRect(hFiles, iItem, &ItemRect, LVIR_BOUNDS))
		{
			RECT FilesRect;
			if (wndFiles.GetClientRect(&FilesRect))
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
	if (!hFiles)
	{
		return false;
	}

	bool bResult = false;

	const EViewKind eViewKind = Button_GetCheck(GetDlgItem(IDC_VIEWCODE)) ? VIEW_KIND_CODE : VIEW_KIND_PRIMARY;
	WCHAR lpViewKind[MAX_PATH + 1];
	switch (eViewKind)
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

	CComBSTR spViewKindBStr = lpViewKind;
	if (spViewKindBStr != nullptr)
	{
		CComPtr<VxDTE::ItemOperations> spItemOperations;
		if (SUCCEEDED(m_spDTE->get_ItemOperations(&spItemOperations)) && spItemOperations)
		{
			int iItem = -1;
			while ((iItem = ListView_GetNextItem(hFiles, iItem, LVNI_SELECTED)) >= 0)
			{
				if (iItem < static_cast<int>(m_filteredFiles.size()))
				{
					const SFilteredFile& FilteredFile = m_filteredFiles[iItem];

					CComBSTR spFilePathBStr = SysAllocString(FilteredFile.pFile->spFilePath.get());
					if (spFilePathBStr != nullptr)
					{
						CComPtr<VxDTE::Window> spWindow;
						spItemOperations->OpenFile(spFilePathBStr, spViewKindBStr, &spWindow);

						if (spWindow != nullptr && m_fileLineDestination != -1)
						{
							CComPtr<VxDTE::Document> spDocument;
							spWindow->get_Document(&spDocument);

							CComPtr<VxDTE::TextSelection> spSelection;
							spDocument->get_Selection(reinterpret_cast<IDispatch**>(&spSelection));

							const int columnDestination = (m_fileColumnDestination != -1) ? m_fileColumnDestination : 1;
							spSelection->MoveToLineAndOffset(m_fileLineDestination, columnDestination);
						}
					}

					bResult = true;
				}
			}
		}
	}

	return bResult;
}


LRESULT CALLBACK CGoToFileDlg::FilterProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CGoToFileDlg::SWndProc* pWndProc = CGoToFileDlg::GetWndProc(hWnd);
	if (pWndProc != nullptr)
	{
		if (uMsg == WM_KEYDOWN)
		{
			switch (wParam)
			{
				case VK_DOWN:
				case VK_UP:
				case VK_NEXT:
				case VK_PRIOR:
					SendMessage(pWndProc->goToFileDlg.GetDlgItem(IDC_FILES), uMsg, wParam, lParam);
					return 0;
			}
		}
		return CallWindowProc(pWndProc->pWndProc, hWnd, uMsg, wParam, lParam);
	}
	return 0;
}

std::list<CGoToFileDlg::SWndProc> CGoToFileDlg::s_wndProcs;

CGoToFileDlg::SWndProc* CGoToFileDlg::GetWndProc(HWND hWnd)
{
	for (CGoToFileDlg::SWndProc& wndProc : CGoToFileDlg::s_wndProcs)
	{
		if (wndProc.hWnd == hWnd)
		{
			return &wndProc;
		}
	}
	return nullptr;
}


void CGoToFileDlg::RegisterWndProc(CGoToFileDlg& goToFileDlg, HWND hWnd, WNDPROC pWndProc)
{
	CGoToFileDlg::s_wndProcs.emplace_back(goToFileDlg, hWnd, pWndProc);
}


void CGoToFileDlg::UnregisterWndProc(CGoToFileDlg* pGoToFileDlg)
{
	for (auto iter = CGoToFileDlg::s_wndProcs.begin(); iter != CGoToFileDlg::s_wndProcs.end(); ++iter)
	{
		const CGoToFileDlg::SWndProc& wndProc = *iter;
		if (&(wndProc.goToFileDlg) == pGoToFileDlg)
		{
			CGoToFileDlg::s_wndProcs.erase(iter);
			break;
		}
	}
}


void CGoToFileDlg::SetupUsageControl()
{
	// We need a monospace font so that our usage text lines up accurately.
	LOGFONT logFont;
	memset(&logFont, 0, sizeof(LOGFONT));
	logFont.lfHeight = -12;
	logFont.lfWeight = FW_NORMAL;
	wcscpy_s(logFont.lfFaceName, _countof(logFont.lfFaceName), L"Consolas");
	m_tooltipFont = CreateFontIndirect(&logFont);

	HWND hwndUsageLink = GetDlgItem(IDC_USAGE_LINK);

	// Set the usage hyperlink in code, as having it in the resource is a bit unwieldy.
	PTSTR pwzUsageText = L"<A HREF=\"https://github.com/galenelias/gotofile-vs?tab=readme-ov-file#special-characters\">?</A>";
	SendMessage(hwndUsageLink, WM_SETTEXT, 0, (LPARAM)pwzUsageText);

	// Create the tooltip.
	HWND hwndTip = CreateWindowEx(NULL, TOOLTIPS_CLASS, NULL,
		WS_POPUP | TTS_ALWAYSTIP | TTS_BALLOON,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		m_hWnd, NULL,
		_AtlBaseModule.GetResourceInstance(), NULL);

	if (!hwndTip)
		return;

	// Copy the special character usage text from https://github.com/galenelias/gotofile-vs?tab=readme-ov-file#special-characters
	// for convenience.
	PTSTR pwzUsageTooltip =
		L"Example                     Description\n"
		L"------------------------------------------------------------\n"
		L"substring.cpp(row[,col])  - Opens the selected file navigating to row/col.\n"
		L"substring.cpp:row[:col]   - Opens the selected file navigating to row/col.\n"
		L"substring .h              - Find all files containing substring in all .h files.\n"
		L"substring |.cpp |.h       - Find all files containing substring in all .cpp and .h files.\n"
		L"substring -.h             - Find all files containing substring in all files except .h files.\n"
		L"substring -.cpp -.h       - Find all files containing substring in all files except .cpp and .h files.\n"
		L"substring \\\\include\\      - Find all files containing substring in any folder called include.\n"
		L"substring :\"header files\" - Find all files containing substring in any filter containing header files.\n"
		L":substring                - Find all files in any project containing substring.\n"
		L"substring *.h             - Find all files containing substring in all files ending in .h.\n"
		;

	// Associate the tooltip with the tool.
	TOOLINFO toolInfo = { 0 };
	toolInfo.cbSize = sizeof(toolInfo);
	toolInfo.hwnd = m_hWnd;
	toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
	toolInfo.uId = (UINT_PTR)hwndUsageLink;
	toolInfo.lpszText = pwzUsageTooltip;
	SendMessage(hwndTip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);
	SendMessage(hwndTip, TTM_SETMAXTIPWIDTH, 0, 32676); // Uncap the text so our super long text can fit and go multi-line
	SendMessage(hwndTip, WM_SETFONT, (WPARAM)m_tooltipFont, TRUE);
}


LRESULT CGoToFileDlg::OnClickUsage(int /*idCtrl*/, LPNMHDR pNHM, BOOL& bHandled)
{
	PNMLINK pNMLink = (PNMLINK)pNHM;
	LITEM   item = pNMLink->item;

	if (item.iLink == 0)
	{
		ShellExecute(NULL, L"open", item.szUrl, NULL, NULL, SW_SHOW);
	}

	bHandled = TRUE;
	return 0;
}


void CGoToFileDlg::InitializeLogFile()
{
	static CHAR s_szLogFilePath[MAX_PATH];
	static bool s_initializedLogFilePath = false;

	if (!s_initializedLogFilePath)
	{
		WCHAR wzLogFilePath[MAX_PATH];
		PWSTR pwzAppDataPath = nullptr;
		SHGetKnownFolderPath(FOLDERID_LocalAppData, 0 /*dwFlags*/, NULL /*hToken*/, &pwzAppDataPath);

		wcscpy_s(wzLogFilePath, _countof(wzLogFilePath), pwzAppDataPath);
		wcscat_s(wzLogFilePath, _countof(wzLogFilePath), L"\\GoToFile");
		CoTaskMemFree(pwzAppDataPath);

		// Ensure our directory exists
		CreateDirectory(wzLogFilePath, nullptr /*lpSecurityAttributes*/);

		wcscat_s(wzLogFilePath, _countof(wzLogFilePath), L"\\GoToFile.log");

		WideCharToMultiByte(CP_ACP, 0, wzLogFilePath, -1, s_szLogFilePath, _countof(s_szLogFilePath), NULL /*lpDefaultChar*/, NULL /*lpUsedDefaultChar*/);
		s_initializedLogFilePath = true;
	}

	m_logfile.open(s_szLogFilePath, std::ofstream::out | std::ofstream::app);
}


void CGoToFileDlg::LogToFile(LPCWSTR pwzFormat, ...)
{
	if (!m_settings.IsLoggingEnabled())
		return;

	SYSTEMTIME systemTime;
	GetLocalTime(&systemTime);

	WCHAR wzDateString[64], wzTimeString[64];
	GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &systemTime, nullptr /*lpFormat*/, wzDateString, _countof(wzDateString), nullptr /*lpCalendar*/);
	GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &systemTime, nullptr /*lpFormat*/, wzTimeString, _countof(wzTimeString));

	va_list argList;
	va_start(argList, pwzFormat);

	WCHAR wzLogLine[1024];
	vswprintf_s(wzLogLine, pwzFormat, argList);
	m_logfile << wzDateString << " " << wzTimeString << ": " << wzLogLine << std::endl;

	va_end(argList);
}
