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
#include "..\GoToFileUI\Resource.h"

class CGoToFileDlg : public CAxDialogImpl<CGoToFileDlg>
{
public:
	CGoToFileDlg(const CComPtr<VxDTE::_DTE>& spDTE);

	~CGoToFileDlg()
	{
		DestroyFileList();
		DestroyBrowseFileList();
	}

	enum { IDD = IDD_GOTOFILE };

	static const int iMaxColumns = 4;
	static int lpSortColumns[iMaxColumns];
	static bool bSortDescending;

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
		COMMAND_HANDLER(IDC_PROJECTS, CBN_SELCHANGE, OnSelChangedProjects)
		CHAIN_MSG_MAP(CAxDialogImpl<CGoToFileDlg>)
	END_MSG_MAP()

	static HICON LoadIcon(int iID, int iWidth, int iHeight);

	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		CAxDialogImpl<CGoToFileDlg>::OnInitDialog(uMsg, wParam, lParam, bHandled);

		HICON hIcon = LoadIcon(1, 32, 32);
		if (hIcon)
		{
			SetIcon(hIcon);
		}

		CWindow Files = GetDlgItem(IDC_FILES);
		{
			const DWORD dwExtendedStyle = LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP;
			ListView_SetExtendedListViewStyleEx(Files, dwExtendedStyle, dwExtendedStyle);

			LVCOLUMN Column;
			memset(&Column, 0, sizeof(LVCOLUMN));
			Column.mask = LVCF_TEXT | LVCF_WIDTH;

			Column.pszText = L"File Name";
			Column.cx = 150;
			ListView_InsertColumn(Files, 0, &Column);

			Column.pszText = L"File Path";
			Column.cx = 250;
			ListView_InsertColumn(Files, 1, &Column);

			Column.pszText = L"Project Name";
			Column.cx = 150;
			ListView_InsertColumn(Files, 2, &Column);

			Column.pszText = L"Project Path";
			Column.cx = 250;
			ListView_InsertColumn(Files, 3, &Column);
		}

		RECT ClientRect;
		if (GetClientRect(&ClientRect))
		{
			WORD uiInitialWidth = static_cast<WORD>(ClientRect.right - ClientRect.left);
			WORD uiInitialHeight = static_cast<WORD>(ClientRect.bottom - ClientRect.top);
			m_iInitialSize = MAKELONG(uiInitialWidth, uiInitialHeight);
		}

		CWindow Filter = GetDlgItem(IDC_FILTER);
		if (Filter)
		{
			Filter.SetFocus();

			WNDPROC pWndProc = reinterpret_cast<WNDPROC>(::SetWindowLongPtr(Filter, GWL_WNDPROC, reinterpret_cast<LONG_PTR>(CGoToFileDlg::FilterProc)));
			if (pWndProc)
			{
				RegisterWndProc(*this, Filter, pWndProc);
			}
		}

		FilesAnchor.Init(*this, Files, static_cast<EAnchor>(ANCHOR_TOP | ANCHOR_BOTTOM | ANCHOR_LEFT | ANCHOR_RIGHT));
		FilterAnchor.Init(*this, Filter, static_cast<EAnchor>(ANCHOR_BOTTOM | ANCHOR_LEFT | ANCHOR_RIGHT));
		ProjectsAnchor.Init(*this, GetDlgItem(IDC_PROJECTS), static_cast<EAnchor>(ANCHOR_TOP | ANCHOR_LEFT | ANCHOR_RIGHT));
		ViewCodeAnchor.Init(*this, GetDlgItem(IDC_VIEWCODE), static_cast<EAnchor>(ANCHOR_BOTTOM | ANCHOR_LEFT));
		ExploreAnchor.Init(*this, GetDlgItem(IDC_EXPLORE), static_cast<EAnchor>(ANCHOR_BOTTOM | ANCHOR_RIGHT));
		OpenAnchor.Init(*this, GetDlgItem(IDOK), static_cast<EAnchor>(ANCHOR_BOTTOM | ANCHOR_RIGHT));
		CancelAnchor.Init(*this, GetDlgItem(IDCANCEL), static_cast<EAnchor>(ANCHOR_BOTTOM | ANCHOR_RIGHT));

		RefreshProjectList();

		m_settings.Store();
		m_settings.Read();
		m_settings.Restore();

		if (GetSelectedProject() == static_cast<unsigned int>(KNOWN_FILTER_BROWSE))
		{
			CreateBrowseFileList(m_settings.GetBrowsePath().c_str());
		}

		RefreshFileList();

		m_bInitializing = false;

		bHandled = TRUE;
		return 0;
	}

	LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		CWindow Filter = GetDlgItem(IDC_FILTER);
		if (Filter)
		{
			CGoToFileDlg::SWndProc* pWndProc = CGoToFileDlg::GetWndProc(Filter);
			if (pWndProc)
			{
				::SetWindowLongPtr(Filter, GWL_WNDPROC, reinterpret_cast<LONG_PTR>(pWndProc->pWndProc));
			}
		}
		UnregisterWndProc(this);

		m_settings.Store();
		m_settings.Write();

		return CAxDialogImpl<CGoToFileDlg>::OnDestroy(uMsg, wParam, lParam, bHandled);
	}

	LRESULT OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		LONG iWidth = static_cast<LONG>(LOWORD(lParam));
		LONG iHeight = static_cast<LONG>(HIWORD(lParam));

		LONG iInitialWidth = GetInitialWidth();
		LONG iInitialHeight = GetInitialHeight();

		LONG iDeltaX = iWidth - iInitialWidth;
		LONG iDeltaY = iHeight - iInitialHeight;

		FilesAnchor.Update(iDeltaX, iDeltaY);
		FilterAnchor.Update(iDeltaX, iDeltaY);
		ProjectsAnchor.Update(iDeltaX, iDeltaY);
		ViewCodeAnchor.Update(iDeltaX, iDeltaY);
		ExploreAnchor.Update(iDeltaX, iDeltaY);
		OpenAnchor.Update(iDeltaX, iDeltaY);
		CancelAnchor.Update(iDeltaX, iDeltaY);
		Invalidate();

		bHandled = TRUE;
		return 0;
	}

	LRESULT OnClickedExplore(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		ExploreSelectedFiles();
		EndDialog(wID);
		return 0;
	}

	LRESULT OnClickedOpen(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		OpenSelectedFiles();
		EndDialog(wID);
		return 0;
	}

	LRESULT OnClickedCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		EndDialog(wID);
		return 0;
	}

	LRESULT OnChangeFilter(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if (!m_bInitializing && !m_settings.Restoring())
		{
			RefreshFileList();
		}
		return 0;
	}

	LRESULT OnGetDispInfoFiles(int /*idCtrl*/, LPNMHDR pNHM, BOOL& /*bHandled*/)
	{
		LV_DISPINFO* pDispInfo = reinterpret_cast<LV_DISPINFO*>(pNHM);
		if (pDispInfo->item.mask & LVIF_TEXT && pDispInfo->item.cchTextMax > 0)
		{
			if (pDispInfo->item.iItem >= 0 && pDispInfo->item.iItem < (LPARAM)m_filteredFiles.size())
			{
				const SFilteredFile& FilteredFile = m_filteredFiles[pDispInfo->item.iItem];
				switch(pDispInfo->item.iSubItem)
				{
				case 0:
					wcsncpy_s(pDispInfo->item.pszText, pDispInfo->item.cchTextMax, FilteredFile.pFile->lpFilePath + FilteredFile.pFile->uiFileName, pDispInfo->item.cchTextMax);
					pDispInfo->item.pszText[pDispInfo->item.cchTextMax - 1] = L'\0';
					break;
				case 1:
					wcsncpy_s(pDispInfo->item.pszText, pDispInfo->item.cchTextMax, FilteredFile.pFile->lpFilePath, pDispInfo->item.cchTextMax);
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

	LRESULT OnGetInfoTipFiles(int /*idCtrl*/, LPNMHDR pNHM, BOOL& /*bHandled*/)
	{
		LPNMLVGETINFOTIP pInfoTip = reinterpret_cast<LPNMLVGETINFOTIP>(pNHM);
		if (pInfoTip->cchTextMax > 0)
		{
			if (pInfoTip->iItem >= 0 && static_cast<size_t>(pInfoTip->iItem) < m_filteredFiles.size())
			{
				const SFilteredFile& FilteredFile = m_filteredFiles[pInfoTip->iItem];
				switch(pInfoTip->iSubItem)
				{
				case 0:
					wcsncpy_s(pInfoTip->pszText, pInfoTip->cchTextMax, FilteredFile.pFile->lpFilePath, pInfoTip->cchTextMax);
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

	LRESULT OnColumnClickFiles(int /*idCtrl*/, LPNMHDR pNHM, BOOL& /*bHandled*/)
	{
		NM_LISTVIEW FAR* pColumnClick = reinterpret_cast<NM_LISTVIEW FAR*>(pNHM);

		for (int i = 0; i < iMaxColumns; i++)
		{
			if (lpSortColumns[i] == pColumnClick->iSubItem)
			{
				if (i == 0)
				{
					bSortDescending = !bSortDescending;
				}
				else
				{
					for (int j = 0; j < i; j++)
					{
						lpSortColumns[i - j] = lpSortColumns[i - j - 1];
					}
					lpSortColumns[0] = pColumnClick->iSubItem;
				}
				break;
			}
		}
		RefreshFileList();

		return 0;
	}

	LRESULT OnDoubleClickFiles(int /*idCtrl*/, LPNMHDR pNHM, BOOL& /*bHandled*/)
	{
		if (OpenSelectedFiles())
		{
			EndDialog(0);
		}

		return 0;
	}

	LRESULT OnGetDispInfoProjects(int /*idCtrl*/, LPNMHDR pNHM, BOOL& /*bHandled*/)
	{
		PNMCOMBOBOXEX pDispInfo = reinterpret_cast<PNMCOMBOBOXEX>(pNHM);
		if (pDispInfo->ceItem.mask & CBEIF_TEXT && pDispInfo->ceItem.cchTextMax > 0)
		{
			if (pDispInfo->ceItem.iItem >= static_cast<INT>(KNOWN_FILTER_COUNT) && static_cast<size_t>(pDispInfo->ceItem.iItem) < static_cast<INT>(KNOWN_FILTER_COUNT) + m_projectNames.size())
			{
				LPCWSTR lpProject = m_projectNames[pDispInfo->ceItem.iItem - static_cast<INT>(KNOWN_FILTER_COUNT)];

				wcsncpy_s(pDispInfo->ceItem.pszText, pDispInfo->ceItem.cchTextMax, lpProject, pDispInfo->ceItem.cchTextMax);
				pDispInfo->ceItem.pszText[pDispInfo->ceItem.cchTextMax - 1] = L'\0';
			}
		}
		return 0;
	}

	LRESULT OnSelChangedProjects(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if (!m_bInitializing && !m_settings.Restoring())
		{
			if (GetSelectedProject() == static_cast<unsigned int>(KNOWN_FILTER_BROWSE))
			{
				CreateBrowseFileList();
			}
			RefreshFileList();
		}
		return 0;
	}

	LONG GetInitialWidth() const
	{
		return static_cast<LONG>(LOWORD(m_iInitialSize));
	}

	LONG GetInitialHeight() const
	{
		return static_cast<LONG>(HIWORD(m_iInitialSize));
	}

private:
	bool m_bInitializing;
	GoToFileSettings m_settings;

	enum EAnchor
	{
		ANCHOR_NONE = 0x00,
		ANCHOR_TOP = 0x01,
		ANCHOR_BOTTOM = 0x02,
		ANCHOR_LEFT = 0x04,
		ANCHOR_RIGHT = 0x08
	};

	struct SAnchor 
	{
		SAnchor() : hWindow(0), eAnchor(ANCHOR_NONE)
		{
		}

		void Init(HWND hParent, HWND hWindow, EAnchor eAnchor)
		{
			this->hWindow = hWindow;
			this->eAnchor = eAnchor;

			::GetWindowRect(hWindow, &Rect);
			::MapWindowPoints(HWND_DESKTOP, hParent, reinterpret_cast<LPPOINT>(&Rect), 2);
		}

		void Adjust(RECT& Rect)
		{
			this->Rect.right = this->Rect.left + (Rect.right - Rect.left);
			this->Rect.bottom = this->Rect.top + (Rect.bottom - Rect.top);
		}

		void Update(LONG iDeltaX, LONG iDeltaY)
		{
			if (hWindow)
			{
				RECT NewRect = Rect;
				if ((eAnchor & ANCHOR_TOP) == 0)
				{
					NewRect.top += iDeltaY;
				}
				if ((eAnchor & ANCHOR_BOTTOM) != 0)
				{
					NewRect.bottom += iDeltaY;
				}
				if ((eAnchor & ANCHOR_LEFT) == 0)
				{
					NewRect.left += iDeltaX;
				}
				if ((eAnchor & ANCHOR_RIGHT) != 0)
				{
					NewRect.right += iDeltaX;
				}
				::MoveWindow(hWindow, NewRect.left, NewRect.top, NewRect.right - NewRect.left, NewRect.bottom - NewRect.top, TRUE);
			}
		}

	private:
		HWND hWindow;
		EAnchor eAnchor;
		RECT Rect;
	};

	LONG m_iInitialSize;
	SAnchor FilesAnchor;
	SAnchor FilterAnchor;
	SAnchor ProjectsAnchor;
	SAnchor ViewCodeAnchor;
	SAnchor ExploreAnchor;
	SAnchor OpenAnchor;
	SAnchor CancelAnchor;

	CComPtr<VxDTE::_DTE> m_spDTE;

	template<class T>
	struct SName
	{
	public:
		SName(T& Names, LPCWSTR lpName)
			: Names(Names), bUsed(false), lpName(nullptr)
		{
			if (lpName != nullptr)
			{
				const size_t cchName = wcslen(lpName) + 1;
				this->lpName = new WCHAR[cchName];
				wcscpy_s(this->lpName, cchName, lpName);
			}
		}

		SName(SName<T>& Parent, LPCWSTR lpName)
			: Names(Parent.Names), bUsed(false), lpName(nullptr)
		{
			if (lpName != nullptr)
			{
				if (Parent.lpName != nullptr)
				{
					const size_t cchName = wcslen(Parent.lpName) + 1 + wcslen(lpName) + 1;
					this->lpName = new WCHAR[cchName];
					wcscpy_s(this->lpName, cchName, Parent.lpName);
					wcscat_s(this->lpName, cchName, L"\\");
					wcscat_s(this->lpName, cchName, lpName);
				}
				else
				{
					const size_t cchName = wcslen(lpName) + 1;
					this->lpName = new WCHAR[cchName];
					wcscpy_s(this->lpName, cchName, lpName);
				}
			}
			else
			{
				bUsed = true;
				lpName = Parent.lpName;
			}
		}

		~SName()
		{
			if (!bUsed)
			{
				delete []lpName;
			}
		}

		LPCWSTR GetName()
		{
			if (!bUsed && lpName)
			{
				bUsed = true;
				Names.push_back(lpName);
			}
			return lpName;
		}

	private:
		bool bUsed;
		T& Names;
		LPWSTR lpName;
	};

	void CreateFileList();
	void CreateFileList(SName<std::list<LPWSTR>>& ParentProjectPath, VxDTE::Project* pProject);
	void CreateFileList(SName<std::vector<LPWSTR>>& ProjectName, SName<std::list<LPWSTR>>& ParentProjectPath, VxDTE::ProjectItems* pParentProjectItems);
	void DestroyFileList();

	void CreateBrowseFileList();
	void CreateBrowseFileList(LPCWSTR lpPath);
	void DestroyBrowseFileList();

	unsigned int GetSelectedProject();

	void RefreshProjectList();

	void SortFileList();
	void RefreshFileList();

	void ExploreSelectedFiles();

	void Select(int iItem);
	bool OpenSelectedFiles();

	struct SFile
	{
		SFile(LPWSTR lpFilePath, LPCWSTR lpProjectName, LPCWSTR lpProjectPath)
			: lpFilePath(lpFilePath), uiFileName(0), lpProjectName(lpProjectName), lpProjectPath(lpProjectPath)
		{
			LPWSTR lpFileName = std::max<LPWSTR>(wcsrchr(lpFilePath, L'\\'), wcsrchr(lpFilePath, L'/'));
			if (lpFileName)
			{
				uiFileName = static_cast<unsigned short>(lpFileName - lpFilePath) + 1;
			}
		}

		SFile(LPWSTR lpFilePath, unsigned short uiFileName, LPCWSTR lpProjectName, LPCWSTR lpProjectPath)
			: lpFilePath(lpFilePath), uiFileName(uiFileName), lpProjectName(lpProjectName), lpProjectPath(lpProjectPath)
		{
		}

		SFile(const SFile& File)
			: lpFilePath(File.lpFilePath), uiFileName(File.uiFileName), lpProjectName(File.lpProjectName), lpProjectPath(File.lpProjectPath)
		{
		}

		SFile& operator=(const SFile& Other)
		{
			if (this != &Other)
			{
				lpFilePath = Other.lpFilePath;
				uiFileName = Other.uiFileName;
				lpProjectName = Other.lpProjectName;
				lpProjectPath = Other.lpProjectPath;
			}
			return *this;
		}

		LPWSTR lpFilePath;
		unsigned short uiFileName;
		LPCWSTR lpProjectName;
		LPCWSTR lpProjectPath;
	};

	struct SFilteredFile
	{
		SFilteredFile(const SFile* pFile, int iMatch)
			: pFile(pFile), iMatch(iMatch)
		{
		}

		const SFile* pFile;
		int iMatch;
	};

	std::vector<LPWSTR> m_projectNames;
	std::list<LPWSTR> m_projectPaths;
	std::list<SFile> m_files;
	std::list<SFile> m_browseFiles;
	std::vector<SFilteredFile> m_filteredFiles;

public:
	const std::vector<LPWSTR>& GetProjectNames() const
	{
		return m_projectNames;
	}

	enum EKnownFilter
	{
		KNOWN_FILTER_ALL_PROJECTS = 0,
		KNOWN_FILTER_BROWSE,
		KNOWN_FILTER_COUNT
	};

	LPCWSTR GetKnownFilterName(EKnownFilter eKnownFilter) const;

private:
	enum ESearchField
	{
		SEARCH_FIELD_FILE_NAME		= 0,
		SEARCH_FIELD_FILE_PATH		= 4,
		SEARCH_FIELD_PROJECT_NAME	= 8,
		SEARCH_FIELD_PROJECT_PATH	= 12
	};

	enum ELogicOperator
	{
		LOGIC_OPERATOR_NONE		= 0x00,
		LOGIC_OPERATOR_AND		= 0x01,
		LOGIC_OPERATOR_AND_NOT	= 0x02,
		LOGIC_OPERATOR_OR		= 0x04,
		LOGIC_OPERATOR_OR_NOT	= 0x08,
		LOGIC_OPERATOR_NOT_MASK	= LOGIC_OPERATOR_AND_NOT | LOGIC_OPERATOR_OR_NOT
	};

	enum EFilterTerm
	{
		FILTER_TERM_NONE					= 0x00,
		FILTER_TERM_FILE_NAME_AND			= LOGIC_OPERATOR_AND << SEARCH_FIELD_FILE_NAME,
		FILTER_TERM_FILE_NAME_AND_NOT		= LOGIC_OPERATOR_AND_NOT << SEARCH_FIELD_FILE_NAME,
		FILTER_TERM_FILE_NAME_OR			= LOGIC_OPERATOR_OR << SEARCH_FIELD_FILE_NAME,
		FILTER_TERM_FILE_NAME_OR_NOT		= LOGIC_OPERATOR_OR_NOT << SEARCH_FIELD_FILE_NAME,
		FILTER_TERM_FILE_PATH_AND			= LOGIC_OPERATOR_AND << SEARCH_FIELD_FILE_PATH,
		FILTER_TERM_FILE_PATH_AND_NOT		= LOGIC_OPERATOR_AND_NOT << SEARCH_FIELD_FILE_PATH,
		FILTER_TERM_FILE_PATH_OR			= LOGIC_OPERATOR_OR << SEARCH_FIELD_FILE_PATH,
		FILTER_TERM_FILE_PATH_OR_NOT		= LOGIC_OPERATOR_OR_NOT << SEARCH_FIELD_FILE_PATH,
		FILTER_TERM_PROJECT_NAME_AND		= LOGIC_OPERATOR_AND << SEARCH_FIELD_PROJECT_NAME,
		FILTER_TERM_PROJECT_NAME_AND_NOT	= LOGIC_OPERATOR_AND_NOT << SEARCH_FIELD_PROJECT_NAME,
		FILTER_TERM_PROJECT_NAME_OR			= LOGIC_OPERATOR_OR << SEARCH_FIELD_PROJECT_NAME,
		FILTER_TERM_PROJECT_NAME_OR_NOT		= LOGIC_OPERATOR_OR_NOT << SEARCH_FIELD_PROJECT_NAME,
		FILTER_TERM_PROJECT_PATH_AND		= LOGIC_OPERATOR_AND << SEARCH_FIELD_PROJECT_PATH,
		FILTER_TERM_PROJECT_PATH_AND_NOT	= LOGIC_OPERATOR_AND_NOT << SEARCH_FIELD_PROJECT_PATH,
		FILTER_TERM_PROJECT_PATH_OR			= LOGIC_OPERATOR_OR << SEARCH_FIELD_PROJECT_PATH,
		FILTER_TERM_PROJECT_PATH_OR_NOT		= LOGIC_OPERATOR_OR_NOT << SEARCH_FIELD_PROJECT_PATH,
		FILTER_TERM_AND_MASK				= FILTER_TERM_FILE_NAME_AND | FILTER_TERM_FILE_NAME_AND_NOT | FILTER_TERM_FILE_PATH_AND | FILTER_TERM_FILE_PATH_AND_NOT | FILTER_TERM_PROJECT_NAME_AND | FILTER_TERM_PROJECT_NAME_AND_NOT | FILTER_TERM_PROJECT_PATH_AND | FILTER_TERM_PROJECT_PATH_AND_NOT,
		FILTER_TERM_OR_MASK					= FILTER_TERM_FILE_NAME_OR | FILTER_TERM_FILE_NAME_OR_NOT | FILTER_TERM_FILE_PATH_OR | FILTER_TERM_FILE_PATH_OR_NOT | FILTER_TERM_PROJECT_NAME_OR | FILTER_TERM_PROJECT_NAME_OR_NOT | FILTER_TERM_PROJECT_PATH_OR | FILTER_TERM_PROJECT_PATH_OR_NOT
	};

	struct SFilter
	{
	public:
		SFilter(LPWSTR lpFilter, ESearchField eSearchField, ELogicOperator eLogicOperator)
			: lpFilter(lpFilter), eSearchField(eSearchField), eLogicOperator(eLogicOperator), bWildcard(false)
		{
			while(*lpFilter)
			{
				*lpFilter = Normalize(*lpFilter, eSearchField);
				if (*lpFilter == L'*' || *lpFilter == L'?')
				{
					bWildcard = true;
				}
				lpFilter++;
			}
		}

		SFilter(const SFilter& Filter)
			: lpFilter(Filter.lpFilter), eSearchField(Filter.eSearchField), eLogicOperator(Filter.eLogicOperator), bWildcard(Filter.bWildcard)
		{
		}

		EFilterTerm GetFilterTerm() const
		{
			return static_cast<EFilterTerm>(eLogicOperator << eSearchField);
		}

		static WCHAR Normalize(WCHAR cChar, ESearchField eSearchField);
		static int Like(LPCWSTR lpSearch, LPCWSTR lpFilter, ESearchField eSearchField);
		int Match(const SFile& File) const;

		LPWSTR lpFilter;
		ESearchField eSearchField;
		ELogicOperator eLogicOperator;

	private:
		bool bWildcard;
	};

	void CreateFilterList(LPWSTR& lpFilterStringTable, std::list<SFilter>& Filters);
	void DestroyFilterList(LPWSTR& lpFilterStringTable, std::list<SFilter>& Filters);

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
					if (SFilter::Normalize(Folder.lpFilePath[i], SEARCH_FIELD_FILE_PATH) != SFilter::Normalize(File.lpFilePath[i], SEARCH_FIELD_FILE_PATH))
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

public:
	struct SWndProc
	{
		CGoToFileDlg& goToFileDlg;
		HWND hWnd;
		WNDPROC pWndProc;

		SWndProc(CGoToFileDlg& goToFileDlg, HWND hWnd, WNDPROC pWndProc)
			: goToFileDlg(goToFileDlg), hWnd(hWnd), pWndProc(pWndProc)
		{
		}

		SWndProc(SWndProc& WndProc)
			: goToFileDlg(WndProc.goToFileDlg), hWnd(WndProc.hWnd), pWndProc(WndProc.pWndProc)
		{
		}
	};

	static SWndProc* GetWndProc(HWND hWnd);

private:
	static std::list<SWndProc*> s_wndProcs;

	static void RegisterWndProc(CGoToFileDlg& goToFileDlg, HWND hWnd, WNDPROC pWndProc);
	static void UnregisterWndProc(CGoToFileDlg* pGoToFileDlg);


private:
	static bool CompareFiles(const SFilteredFile& file1, const SFilteredFile& file2);

};