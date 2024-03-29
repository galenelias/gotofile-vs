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

#pragma once

#include "stdafx.h"
#include "dte.h"
#include "GoToFileSettings.h"
#include "WindowAnchor.h"
#include "SearchFilter.h"
#include "..\GoToFileUI\Resource.h"
#include <fstream>  // std::wofstream
#include <optional>

class CGoToFileDlg : public CAxDialogImpl<CGoToFileDlg>
{
public:
	CGoToFileDlg(const CComPtr<VxDTE::_DTE>& spDTE);
	~CGoToFileDlg();

	enum { IDD = IDD_GOTOFILE };

	static constexpr int s_iMaxColumns = 4;
	static int s_lpSortColumns[s_iMaxColumns];
	static bool s_bSortDescending;

	BEGIN_MSG_MAP(CGoToFileDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		COMMAND_HANDLER(IDC_EXPLORE, BN_CLICKED, OnClickedExplore)
		COMMAND_HANDLER(IDOK, BN_CLICKED, OnClickedOpen)
		COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnClickedCancel)
		COMMAND_HANDLER(IDC_FILTER, EN_CHANGE, OnChangeFilter)
		NOTIFY_HANDLER(IDC_FILES, LVN_GETDISPINFO, OnGetDispInfoFiles)
		NOTIFY_HANDLER(IDC_FILES, LVN_GETINFOTIP, OnGetInfoTipFiles)
		NOTIFY_HANDLER(IDC_FILES, LVN_COLUMNCLICK, OnColumnClickFiles)
		NOTIFY_HANDLER(IDC_FILES, NM_DBLCLK, OnDoubleClickFiles)
		NOTIFY_HANDLER(IDC_PROJECTS, CBEN_GETDISPINFO, OnGetDispInfoProjects)
		NOTIFY_HANDLER(IDC_USAGE_LINK, NM_CLICK, OnClickUsage)

		COMMAND_HANDLER(IDC_PROJECTS, CBN_SELCHANGE, OnSelChangedProjects)
		CHAIN_MSG_MAP(CAxDialogImpl<CGoToFileDlg>)
	END_MSG_MAP()

	static HICON LoadIcon(int iID, int iWidth, int iHeight);

	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnClickedExplore(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnClickedOpen(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnClickedCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnChangeFilter(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnGetDispInfoFiles(int idCtrl, LPNMHDR pNHM, BOOL& bHandled);
	LRESULT OnGetInfoTipFiles(int idCtrl, LPNMHDR pNHM, BOOL& bHandled);
	LRESULT OnColumnClickFiles(int idCtrl, LPNMHDR pNHM, BOOL& bHandled);
	LRESULT OnDoubleClickFiles(int idCtrl, LPNMHDR pNHM, BOOL& bHandled);
	LRESULT OnGetDispInfoProjects(int idCtrl, LPNMHDR pNHM, BOOL& bHandled);
	LRESULT OnSelChangedProjects(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnClickUsage(int idCtrl, LPNMHDR pNHM, BOOL& bHandled);

	LONG GetInitialWidth() const { return m_iInitialWidth; }
	LONG GetInitialHeight() const { return m_iInitialHeight; }

private:
	bool m_bInitializing;
	GoToFileSettings m_settings;
	LONGLONG m_createFileListTime = 0;

	LONG m_iInitialWidth = 0;
	LONG m_iInitialHeight = 0;
	SAnchor m_filesAnchor;
	SAnchor m_filterAnchor;
	SAnchor m_projectsAnchor;
	SAnchor m_viewCodeAnchor;
	SAnchor m_exploreAnchor;
	SAnchor m_openAnchor;
	SAnchor m_cancelAnchor;
	SAnchor m_usageAnchor;

	HFONT m_tooltipFont = NULL;

	CComPtr<VxDTE::_DTE> m_spDTE;

	template<class T>
	struct SName
	{
	public:
		SName(T& Names, LPCWSTR lpName)
			: namesList(Names)
		{
			if (lpName != nullptr)
			{
				const size_t cchName = wcslen(lpName) + 1;
				this->spOwnedName = std::make_unique<WCHAR[]>(cchName);
				this->lpName = this->spOwnedName.get();
				wcscpy_s(this->lpName, cchName, lpName);
			}
		}

		SName(SName<T>& parent, LPCWSTR lpName)
			: namesList(parent.namesList)
		{
			if (lpName != nullptr)
			{
				if (parent.lpName != nullptr)
				{
					const size_t cchName = wcslen(parent.lpName) + 1 + wcslen(lpName) + 1;
					this->spOwnedName = std::make_unique<WCHAR[]>(cchName);
					this->lpName = this->spOwnedName.get();
					wcscpy_s(this->lpName, cchName, parent.lpName);
					wcscat_s(this->lpName, cchName, L"\\");
					wcscat_s(this->lpName, cchName, lpName);
				}
				else
				{
					const size_t cchName = wcslen(lpName) + 1;
					this->spOwnedName = std::make_unique<WCHAR[]>(cchName);
					this->lpName = this->spOwnedName.get();
					wcscpy_s(this->lpName, cchName, lpName);
				}
			}
			else
			{
				this->lpName = parent.lpName;
			}
		}

		SName(const SName<T>& other) = delete;
		SName(SName<T>&& other) = delete;

		SName& operator=(const SName<T>& other) = delete;
		SName& operator=(SName<T>&& other) = delete;

		LPCWSTR GetName()
		{
			if (!bUsed && lpName)
			{
				bUsed = true;
				namesList.push_back(std::move(spOwnedName));
			}
			return lpName;
		}

	private:
		T& namesList;
		bool bUsed = false;
		std::unique_ptr<WCHAR[]> spOwnedName;
		LPWSTR lpName = nullptr;
	};

	void CreateFileList();
	void CreateFileList(SName<std::vector<std::unique_ptr<WCHAR[]>>>& ParentProjectPath, VxDTE::Project* pProject);
	void CreateFileList(SName<std::vector<std::unique_ptr<WCHAR[]>>>& ProjectName, SName<std::vector<std::unique_ptr<WCHAR[]>>>& ParentProjectPath, VxDTE::ProjectItems* pParentProjectItems);
	void DestroyFileList();

	void CreateBrowseFileList();
	void CreateBrowseFileList(LPCWSTR lpPath);
	void DestroyBrowseFileList();

	void SetupUsageControl();

	unsigned int GetSelectedProject();


	void RefreshProjectList();

	void SortFileList();
	void RefreshFileList();

	void ExploreSelectedFiles();

	void Select(int iItem);
	bool OpenSelectedFiles();



	std::vector<std::unique_ptr<WCHAR[]>> m_projectNames;
	std::vector<std::unique_ptr<WCHAR[]>> m_projectPaths;
	std::vector<SFile> m_files;
	std::vector<SFile> m_browseFiles;
	std::vector<SFilteredFile> m_filteredFiles;
	std::optional<std::vector<const WCHAR*>> m_selectedProjects;

	int m_fileLineDestination = -1;
	int m_fileColumnDestination = -1;

public:
	const std::vector<std::unique_ptr<WCHAR[]>>& GetProjectNames() const
	{
		return m_projectNames;
	}

	void SetSelectedProjects(const std::vector<std::wstring_view>& selectedProjects);
	const std::optional<std::vector<const WCHAR*>>& GetSelectedProjects() const;

	enum EKnownFilter
	{
		KNOWN_FILTER_ALL_PROJECTS = 0,
		KNOWN_FILTER_BROWSE,
		KNOWN_FILTER_SELECT_PROJECTS,
		KNOWN_FILTER_COUNT
	};

	LPCWSTR GetKnownFilterName(EKnownFilter eKnownFilter) const;

private:
	static LRESULT CALLBACK FilterProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	struct SFolderAndSelectedFiles
	{
	public:
		SFolderAndSelectedFiles(const SFile& Folder)
			: Folder(Folder)
		{
			SelectedFiles.push_back(&Folder);
		}

		bool Contains(const SFile& File) const
		{
			if (Folder.uiFileName == File.uiFileName)
			{
				for (unsigned short i = 0; i < Folder.uiFileName; i++)
				{
					if (SFilter::Normalize(Folder.spFilePath[i], SEARCH_FIELD_FILE_PATH) != SFilter::Normalize(File.spFilePath[i], SEARCH_FIELD_FILE_PATH))
					{
						return false;
					}
				}
				return true;
			}
			return false;
		}

		const SFile& Folder;
		std::list<const SFile*> SelectedFiles;
	};

private:
	struct SWndProc
	{
		CGoToFileDlg& goToFileDlg;
		HWND hWnd;
		WNDPROC pWndProc;

		SWndProc(CGoToFileDlg& goToFileDlg, HWND hWnd, WNDPROC pWndProc)
			: goToFileDlg(goToFileDlg), hWnd(hWnd), pWndProc(pWndProc)
		{
		}
	};

	static std::list<SWndProc> s_wndProcs;

	static SWndProc* GetWndProc(HWND hWnd);
	static void RegisterWndProc(CGoToFileDlg& goToFileDlg, HWND hWnd, WNDPROC pWndProc);
	static void UnregisterWndProc(CGoToFileDlg* pGoToFileDlg);

	static bool CompareFiles(const SFilteredFile& file1, const SFilteredFile& file2);

	void InitializeLogFile();
	void LogToFile(LPCWSTR pwzFormat, ...);

	std::wofstream m_logfile;
};