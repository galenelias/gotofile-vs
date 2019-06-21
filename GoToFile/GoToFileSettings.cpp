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

	RECT Rect;
	if (GetWindowRect(goToFileDlg, &Rect))
	{
		Location.x = Rect.left;
		Location.y = Rect.top;
		Size.cx = Rect.right - Rect.left;
		Size.cy = Rect.bottom - Rect.top;

		if (Location.x > iDesktopWidth)
		{
			Location.x = iDesktopWidth - goToFileDlg.GetInitialWidth();
		}
		if (Location.y > iDesktopHeight)
		{
			Location.y = iDesktopHeight - goToFileDlg.GetInitialHeight();
		}
		if (Size.cx > iDesktopWidth)
		{
			Size.cx = iDesktopWidth;
		}
		if (Size.cy > iDesktopHeight)
		{
			Size.cy = iDesktopHeight;
		}
	}

	CWindow FilesWindow = goToFileDlg.GetDlgItem(IDC_FILES);
	if (FilesWindow)
	{
		LVCOLUMN Column;
		memset(&Column, 0, sizeof(LVCOLUMN));
		Column.mask = LVCF_WIDTH;

		ListView_GetColumn(FilesWindow, 0, &Column);
		iFileNameWidth = std::min<int>(Column.cx, iDesktopWidth);

		ListView_GetColumn(FilesWindow, 1, &Column);
		iFilePathWidth = std::min<int>(Column.cx, iDesktopWidth);

		ListView_GetColumn(FilesWindow, 2, &Column);
		iProjectNameWidth = std::min<int>(Column.cx, iDesktopWidth);

		ListView_GetColumn(FilesWindow, 3, &Column);
		iProjectPathWidth = std::min<int>(Column.cx, iDesktopWidth);
	}

	Project.clear();

	CWindow ProjectsWindow = goToFileDlg.GetDlgItem(IDC_PROJECTS);
	if (ProjectsWindow)
	{
		LRESULT iItem = ::SendMessage(ProjectsWindow, CB_GETCURSEL, 0, 0);
		if (static_cast<int>(iItem) >= 0 && static_cast<int>(iItem) < CGoToFileDlg::KNOWN_FILTER_COUNT)
		{
			Project = goToFileDlg.GetKnownFilterName(static_cast<CGoToFileDlg::EKnownFilter>(iItem));
		}
		else
		{
			const std::vector<std::unique_ptr<WCHAR[]>>& projectNames = goToFileDlg.GetProjectNames();
			if (static_cast<size_t>(iItem) >= CGoToFileDlg::KNOWN_FILTER_COUNT && static_cast<size_t>(iItem) < projectNames.size() + CGoToFileDlg::KNOWN_FILTER_COUNT)
			{
				iItem -= CGoToFileDlg::KNOWN_FILTER_COUNT;
				Project = projectNames[iItem].get();
			}
		}
	}

	Filter.clear();

	CWindow FilterWindow = goToFileDlg.GetDlgItem(IDC_FILTER);
	if (FilterWindow)
	{
		BSTR lpText = NULL;
		FilterWindow.GetWindowText(lpText);
		if (lpText)
		{
			Filter = lpText;
		}
	}

	CWindow ViewCodeWindow = goToFileDlg.GetDlgItem(IDC_VIEWCODE);
	if (ViewCodeWindow)
	{
		eViewKind = Button_GetCheck(ViewCodeWindow) ? VIEW_KIND_CODE : VIEW_KIND_PRIMARY;
	}
}

void GoToFileSettings::Restore()
{
	bRestoring = true;

	RECT Rect;
	Rect.left = Location.x;
	Rect.top = Location.y;
	Rect.right = Rect.left + Size.cx;
	Rect.bottom = Rect.top + Size.cy;
	goToFileDlg.MoveWindow(&Rect);

	CWindow FilesWindow = goToFileDlg.GetDlgItem(IDC_FILES);
	if (FilesWindow)
	{
		LVCOLUMN Column;
		memset(&Column, 0, sizeof(LVCOLUMN));
		Column.mask = LVCF_WIDTH;

		Column.cx = iFileNameWidth;
		ListView_SetColumn(FilesWindow, 0, &Column);

		Column.cx = iFilePathWidth;
		ListView_SetColumn(FilesWindow, 1, &Column);

		Column.cx = iProjectNameWidth;
		ListView_SetColumn(FilesWindow, 2, &Column);

		Column.cx = iProjectPathWidth;
		ListView_SetColumn(FilesWindow, 3, &Column);
	}

	if (!Project.empty())
	{
		CWindow ProjectsWindow = goToFileDlg.GetDlgItem(IDC_PROJECTS);
		if (ProjectsWindow)
		{
			for (int i = 0; i < CGoToFileDlg::KNOWN_FILTER_COUNT; i++)
			{
				if (i == CGoToFileDlg::KNOWN_FILTER_BROWSE && BrowsePath.empty())
				{
					continue;
				}
				if (_wcsicmp(Project.c_str(), goToFileDlg.GetKnownFilterName(static_cast<CGoToFileDlg::EKnownFilter>(i))) == 0)
				{
					::SendMessage(ProjectsWindow, CB_SETCURSEL, i, 0);
					break;
				}
			}
			const std::vector<std::unique_ptr<WCHAR[]>>& projectNames = goToFileDlg.GetProjectNames();
			for (size_t i = 0; i < projectNames.size(); i++)
			{
				if (_wcsicmp(Project.c_str(), projectNames[i].get()) == 0)
				{
					::SendMessage(ProjectsWindow, CB_SETCURSEL, i + CGoToFileDlg::KNOWN_FILTER_COUNT, 0);
					break;
				}
			}
		}
	}

	if (!Filter.empty())
	{
		CWindow FilterWindow = goToFileDlg.GetDlgItem(IDC_FILTER);
		if (FilterWindow)
		{
			FilterWindow.SetWindowText(Filter.c_str());
			FilterWindow.SendMessage(EM_SETSEL, 0, -1);
		}
	}

	CWindow ViewCodeWindow = goToFileDlg.GetDlgItem(IDC_VIEWCODE);
	if (ViewCodeWindow)
	{
		Button_SetCheck(ViewCodeWindow, eViewKind == VIEW_KIND_CODE);
	}

	bRestoring = false;
}

void GoToFileSettings::Read()
{
	DWORD uiSize;

	// TODO: Move to new registry hive
	HKEY hOpenNow;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Nem's Tools\\Open Now!", 0, KEY_READ, &hOpenNow) == ERROR_SUCCESS)
	{
		uiSize = sizeof(Location);
		RegQueryValueEx(hOpenNow, L"Location", NULL, NULL, reinterpret_cast<LPBYTE>(&Location), &uiSize);
		uiSize = sizeof(Size);
		RegQueryValueEx(hOpenNow, L"Size", NULL, NULL, reinterpret_cast<LPBYTE>(&Size), &uiSize);

		uiSize = sizeof(iFileNameWidth);
		RegQueryValueEx(hOpenNow, L"FileNameWidth", NULL, NULL, reinterpret_cast<LPBYTE>(&iFileNameWidth), &uiSize);
		uiSize = sizeof(iFilePathWidth);
		RegQueryValueEx(hOpenNow, L"FilePathWidth", NULL, NULL, reinterpret_cast<LPBYTE>(&iFilePathWidth), &uiSize);
		uiSize = sizeof(iProjectNameWidth);
		RegQueryValueEx(hOpenNow, L"ProjectNameWidth", NULL, NULL, reinterpret_cast<LPBYTE>(&iProjectNameWidth), &uiSize);
		uiSize = sizeof(iProjectPathWidth);
		RegQueryValueEx(hOpenNow, L"ProjectPathWidth", NULL, NULL, reinterpret_cast<LPBYTE>(&iProjectPathWidth), &uiSize);

		Project.clear();

		uiSize = 0;
		if (RegQueryValueEx(hOpenNow, L"Project", NULL, NULL, NULL, &uiSize) == ERROR_SUCCESS && uiSize > 0)
		{
			LPWSTR lpProject = new WCHAR[uiSize / sizeof(WCHAR)];
			RegQueryValueEx(hOpenNow, L"Project", NULL, NULL, reinterpret_cast<LPBYTE>(lpProject), &uiSize);
			lpProject[uiSize / sizeof(WCHAR) - 1] = '\0';
			Project = lpProject;
			delete []lpProject;
		}

		Filter.clear();

		uiSize = 0;
		if (RegQueryValueEx(hOpenNow, L"Filter", NULL, NULL, NULL, &uiSize) == ERROR_SUCCESS && uiSize > 0)
		{
			LPWSTR lpFilter = new WCHAR[uiSize / sizeof(WCHAR)];
			RegQueryValueEx(hOpenNow, L"Filter", NULL, NULL, reinterpret_cast<LPBYTE>(lpFilter), &uiSize);
			lpFilter[uiSize / sizeof(WCHAR) - 1] = '\0';
			Filter = lpFilter;
			delete []lpFilter;
		}

		BrowsePath.clear();

		uiSize = 0;
		if (RegQueryValueEx(hOpenNow, L"BrowsePath", NULL, NULL, NULL, &uiSize) == ERROR_SUCCESS && uiSize > 0)
		{
			LPWSTR lpBrowsePath = new WCHAR[uiSize / sizeof(WCHAR)];
			RegQueryValueEx(hOpenNow, L"BrowsePath", NULL, NULL, reinterpret_cast<LPBYTE>(lpBrowsePath), &uiSize);
			lpBrowsePath[uiSize / sizeof(WCHAR) - 1] = '\0';
			BrowsePath = lpBrowsePath;
			delete []lpBrowsePath;
		}

		uiSize = sizeof(eViewKind);
		RegQueryValueEx(hOpenNow, L"ViewKind", NULL, NULL, reinterpret_cast<LPBYTE>(&eViewKind), &uiSize);
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
			RegSetValueEx(hOpenNow, L"Location", 0, REG_BINARY, reinterpret_cast<const LPBYTE>(&Location), sizeof(Location));
			RegSetValueEx(hOpenNow, L"Size", 0, REG_BINARY, reinterpret_cast<const LPBYTE>(&Size), sizeof(Size));

			RegSetValueEx(hOpenNow, L"FileNameWidth", 0, REG_DWORD, reinterpret_cast<const LPBYTE>(&iFileNameWidth), sizeof(iFileNameWidth));
			RegSetValueEx(hOpenNow, L"FilePathWidth", 0, REG_DWORD, reinterpret_cast<const LPBYTE>(&iFilePathWidth), sizeof(iFilePathWidth));
			RegSetValueEx(hOpenNow, L"ProjectNameWidth", 0, REG_DWORD, reinterpret_cast<const LPBYTE>(&iProjectNameWidth), sizeof(iProjectNameWidth));
			RegSetValueEx(hOpenNow, L"ProjectPathWidth", 0, REG_DWORD, reinterpret_cast<const LPBYTE>(&iProjectPathWidth), sizeof(iProjectPathWidth));

			RegSetValueEx(hOpenNow, L"Project", 0, REG_SZ, reinterpret_cast<const BYTE *>(Project.c_str()), !Project.empty() ? sizeof(WCHAR) * static_cast<DWORD>(Project.size() + 1) : 0);
			RegSetValueEx(hOpenNow, L"Filter", 0, REG_SZ, reinterpret_cast<const BYTE *>(Filter.c_str()), !Filter.empty() ? sizeof(WCHAR) * static_cast<DWORD>(Filter.size() + 1) : 0);
			RegSetValueEx(hOpenNow, L"BrowsePath", 0, REG_SZ, reinterpret_cast<const BYTE *>(BrowsePath.c_str()), !BrowsePath.empty() ? sizeof(WCHAR) * static_cast<DWORD>(BrowsePath.size() + 1) : 0);

			RegSetValueEx(hOpenNow, L"ViewKind", 0, REG_DWORD, reinterpret_cast<const LPBYTE>(&eViewKind), sizeof(eViewKind));
		}
	}
}