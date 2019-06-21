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
#include "GoToFileSettings.h"
#include "GoToFileDlg.h"

void GoToFileSettings::Store()
{
	int iDesktopWidth = GetSystemMetrics(SM_CXMAXTRACK);
	int iDesktopHeight = GetSystemMetrics(SM_CYMAXTRACK);

	RECT rect;
	if (GetWindowRect(goToFileDlg, &rect))
	{
		m_location.x = rect.left;
		m_location.y = rect.top;
		m_size.cx = rect.right - rect.left;
		m_size.cy = rect.bottom - rect.top;

		if (m_location.x > iDesktopWidth)
		{
			m_location.x = iDesktopWidth - goToFileDlg.GetInitialWidth();
		}
		if (m_location.y > iDesktopHeight)
		{
			m_location.y = iDesktopHeight - goToFileDlg.GetInitialHeight();
		}
		if (m_size.cx > iDesktopWidth)
		{
			m_size.cx = iDesktopWidth;
		}
		if (m_size.cy > iDesktopHeight)
		{
			m_size.cy = iDesktopHeight;
		}
	}

	CWindow wndFiles = goToFileDlg.GetDlgItem(IDC_FILES);
	if (wndFiles)
	{
		LVCOLUMN Column;
		memset(&Column, 0, sizeof(LVCOLUMN));
		Column.mask = LVCF_WIDTH;

		ListView_GetColumn(wndFiles, 0, &Column);
		m_iFileNameWidth = std::min<int>(Column.cx, iDesktopWidth);

		ListView_GetColumn(wndFiles, 1, &Column);
		m_iFilePathWidth = std::min<int>(Column.cx, iDesktopWidth);

		ListView_GetColumn(wndFiles, 2, &Column);
		m_iProjectNameWidth = std::min<int>(Column.cx, iDesktopWidth);

		ListView_GetColumn(wndFiles, 3, &Column);
		m_iProjectPathWidth = std::min<int>(Column.cx, iDesktopWidth);
	}

	m_project.clear();

	CWindow wndProjects = goToFileDlg.GetDlgItem(IDC_PROJECTS);
	if (wndProjects)
	{
		LRESULT iItem = ::SendMessage(wndProjects, CB_GETCURSEL, 0, 0);
		if (static_cast<int>(iItem) >= 0 && static_cast<int>(iItem) < CGoToFileDlg::KNOWN_FILTER_COUNT)
		{
			m_project = goToFileDlg.GetKnownFilterName(static_cast<CGoToFileDlg::EKnownFilter>(iItem));
		}
		else
		{
			const std::vector<std::unique_ptr<WCHAR[]>>& projectNames = goToFileDlg.GetProjectNames();
			if (static_cast<size_t>(iItem) >= CGoToFileDlg::KNOWN_FILTER_COUNT && static_cast<size_t>(iItem) < projectNames.size() + CGoToFileDlg::KNOWN_FILTER_COUNT)
			{
				iItem -= CGoToFileDlg::KNOWN_FILTER_COUNT;
				m_project = projectNames[iItem].get();
			}
		}
	}

	m_filter.clear();

	CWindow wndFilter = goToFileDlg.GetDlgItem(IDC_FILTER);
	if (wndFilter)
	{
		BSTR lpText = NULL;
		wndFilter.GetWindowText(lpText);
		if (lpText)
		{
			m_filter = lpText;
		}
	}

	CWindow wndViewCode = goToFileDlg.GetDlgItem(IDC_VIEWCODE);
	if (wndViewCode)
	{
		m_eViewKind = Button_GetCheck(wndViewCode) ? VIEW_KIND_CODE : VIEW_KIND_PRIMARY;
	}
}

void GoToFileSettings::Restore()
{
	m_bRestoring = true;

	RECT rect;
	rect.left = m_location.x;
	rect.top = m_location.y;
	rect.right = rect.left + m_size.cx;
	rect.bottom = rect.top + m_size.cy;
	goToFileDlg.MoveWindow(&rect);

	CWindow wndFiles = goToFileDlg.GetDlgItem(IDC_FILES);
	if (wndFiles)
	{
		LVCOLUMN Column;
		memset(&Column, 0, sizeof(LVCOLUMN));
		Column.mask = LVCF_WIDTH;

		Column.cx = m_iFileNameWidth;
		ListView_SetColumn(wndFiles, 0, &Column);

		Column.cx = m_iFilePathWidth;
		ListView_SetColumn(wndFiles, 1, &Column);

		Column.cx = m_iProjectNameWidth;
		ListView_SetColumn(wndFiles, 2, &Column);

		Column.cx = m_iProjectPathWidth;
		ListView_SetColumn(wndFiles, 3, &Column);
	}

	if (!m_project.empty())
	{
		CWindow wndProjects= goToFileDlg.GetDlgItem(IDC_PROJECTS);
		if (wndProjects)
		{
			for (int i = 0; i < CGoToFileDlg::KNOWN_FILTER_COUNT; i++)
			{
				if (i == CGoToFileDlg::KNOWN_FILTER_BROWSE && m_browsePath.empty())
				{
					continue;
				}
				if (_wcsicmp(m_project.c_str(), goToFileDlg.GetKnownFilterName(static_cast<CGoToFileDlg::EKnownFilter>(i))) == 0)
				{
					::SendMessage(wndProjects, CB_SETCURSEL, i, 0);
					break;
				}
			}
			const std::vector<std::unique_ptr<WCHAR[]>>& projectNames = goToFileDlg.GetProjectNames();
			for (size_t i = 0; i < projectNames.size(); i++)
			{
				if (_wcsicmp(m_project.c_str(), projectNames[i].get()) == 0)
				{
					::SendMessage(wndProjects, CB_SETCURSEL, i + CGoToFileDlg::KNOWN_FILTER_COUNT, 0);
					break;
				}
			}
		}
	}

	if (!m_filter.empty())
	{
		CWindow wndFilter = goToFileDlg.GetDlgItem(IDC_FILTER);
		if (wndFilter)
		{
			wndFilter.SetWindowText(m_filter.c_str());
			wndFilter.SendMessage(EM_SETSEL, 0, -1);
		}
	}

	CWindow wndViewCode = goToFileDlg.GetDlgItem(IDC_VIEWCODE);
	if (wndViewCode)
	{
		Button_SetCheck(wndViewCode, m_eViewKind == VIEW_KIND_CODE);
	}

	m_bRestoring = false;
}

void GoToFileSettings::Read()
{
	DWORD uiSize;

	// TODO: Move to new registry hive
	HKEY hOpenNow;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Nem's Tools\\Open Now!", 0, KEY_READ, &hOpenNow) == ERROR_SUCCESS)
	{
		uiSize = sizeof(m_location);
		RegQueryValueEx(hOpenNow, L"Location", NULL, NULL, reinterpret_cast<LPBYTE>(&m_location), &uiSize);
		uiSize = sizeof(m_size);
		RegQueryValueEx(hOpenNow, L"Size", NULL, NULL, reinterpret_cast<LPBYTE>(&m_size), &uiSize);

		uiSize = sizeof(m_iFileNameWidth);
		RegQueryValueEx(hOpenNow, L"FileNameWidth", NULL, NULL, reinterpret_cast<LPBYTE>(&m_iFileNameWidth), &uiSize);
		uiSize = sizeof(m_iFilePathWidth);
		RegQueryValueEx(hOpenNow, L"FilePathWidth", NULL, NULL, reinterpret_cast<LPBYTE>(&m_iFilePathWidth), &uiSize);
		uiSize = sizeof(m_iProjectNameWidth);
		RegQueryValueEx(hOpenNow, L"ProjectNameWidth", NULL, NULL, reinterpret_cast<LPBYTE>(&m_iProjectNameWidth), &uiSize);
		uiSize = sizeof(m_iProjectPathWidth);
		RegQueryValueEx(hOpenNow, L"ProjectPathWidth", NULL, NULL, reinterpret_cast<LPBYTE>(&m_iProjectPathWidth), &uiSize);

		m_project.clear();
		uiSize = 0;
		if (RegQueryValueEx(hOpenNow, L"Project", NULL, NULL, NULL, &uiSize) == ERROR_SUCCESS && uiSize > 0)
		{
			m_project.resize((uiSize / sizeof(WCHAR)) - 1);
			RegQueryValueEx(hOpenNow, L"Project", NULL, NULL, reinterpret_cast<LPBYTE>(&m_project[0]), &uiSize);
		}

		m_filter.clear();
		uiSize = 0;
		if (RegQueryValueEx(hOpenNow, L"Filter", NULL, NULL, NULL, &uiSize) == ERROR_SUCCESS && uiSize > 0)
		{
			m_filter.resize((uiSize / sizeof(WCHAR)) - 1);
			RegQueryValueEx(hOpenNow, L"Filter", NULL, NULL, reinterpret_cast<LPBYTE>(&m_filter[0]), &uiSize);
		}

		m_browsePath.clear();
		uiSize = 0;
		if (RegQueryValueEx(hOpenNow, L"BrowsePath", NULL, NULL, NULL, &uiSize) == ERROR_SUCCESS && uiSize > 0)
		{
			m_browsePath.resize((uiSize / sizeof(WCHAR)) - 1);
			RegQueryValueEx(hOpenNow, L"BrowsePath", NULL, NULL, reinterpret_cast<LPBYTE>(&m_browsePath[0]), &uiSize);
		}

		uiSize = sizeof(m_eViewKind);
		RegQueryValueEx(hOpenNow, L"ViewKind", NULL, NULL, reinterpret_cast<LPBYTE>(&m_eViewKind), &uiSize);
	}
}

void GoToFileSettings::Write()
{
	HKEY hSoftware;

	// TODO: Move to new registry hive
	if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software", 0, KEY_WRITE, &hSoftware) == ERROR_SUCCESS)
	{
		HKEY hOpenNow;
		if (RegCreateKeyEx(hSoftware, L"Nem's Tools\\Open Now!", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hOpenNow, NULL) == ERROR_SUCCESS)
		{
			RegSetValueEx(hOpenNow, L"Location", 0, REG_BINARY, reinterpret_cast<const LPBYTE>(&m_location), sizeof(m_location));
			RegSetValueEx(hOpenNow, L"Size", 0, REG_BINARY, reinterpret_cast<const LPBYTE>(&m_size), sizeof(m_size));

			RegSetValueEx(hOpenNow, L"FileNameWidth", 0, REG_DWORD, reinterpret_cast<const LPBYTE>(&m_iFileNameWidth), sizeof(m_iFileNameWidth));
			RegSetValueEx(hOpenNow, L"FilePathWidth", 0, REG_DWORD, reinterpret_cast<const LPBYTE>(&m_iFilePathWidth), sizeof(m_iFilePathWidth));
			RegSetValueEx(hOpenNow, L"ProjectNameWidth", 0, REG_DWORD, reinterpret_cast<const LPBYTE>(&m_iProjectNameWidth), sizeof(m_iProjectNameWidth));
			RegSetValueEx(hOpenNow, L"ProjectPathWidth", 0, REG_DWORD, reinterpret_cast<const LPBYTE>(&m_iProjectPathWidth), sizeof(m_iProjectPathWidth));

			RegSetValueEx(hOpenNow, L"Project", 0, REG_SZ, reinterpret_cast<const BYTE *>(m_project.c_str()), !m_project.empty() ? sizeof(WCHAR) * static_cast<DWORD>(m_project.size() + 1) : 0);
			RegSetValueEx(hOpenNow, L"Filter", 0, REG_SZ, reinterpret_cast<const BYTE *>(m_filter.c_str()), !m_filter.empty() ? sizeof(WCHAR) * static_cast<DWORD>(m_filter.size() + 1) : 0);
			RegSetValueEx(hOpenNow, L"BrowsePath", 0, REG_SZ, reinterpret_cast<const BYTE *>(m_browsePath.c_str()), !m_browsePath.empty() ? sizeof(WCHAR) * static_cast<DWORD>(m_browsePath.size() + 1) : 0);

			RegSetValueEx(hOpenNow, L"ViewKind", 0, REG_DWORD, reinterpret_cast<const LPBYTE>(&m_eViewKind), sizeof(m_eViewKind));
		}
	}
}