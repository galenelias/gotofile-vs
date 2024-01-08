/*
 *	GoToFile

 *	Copyright (C) 2024 Galen Elias
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
#include "..\GoToFileUI\Resource.h"
#include <optional>

class CSelectProjectsDlg : public CAxDialogImpl<CSelectProjectsDlg>
{
public:
	CSelectProjectsDlg(const std::vector<std::unique_ptr<WCHAR[]>>& projectNames, const std::optional<std::vector<const WCHAR*>>& selectedProjects);
	~CSelectProjectsDlg();

	std::vector<const WCHAR*> GetSelectedProjects() const;

	enum { IDD = IDD_SELECT_PROJECTS };

	BEGIN_MSG_MAP(CGoToFileDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_HANDLER(IDOK, BN_CLICKED, OnClickedOk)
		COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnClickedCancel)
		COMMAND_HANDLER(IDC_BTN_SELECT_ALL_NONE, BN_CLICKED, OnClickedSelectAllOrNone)
		NOTIFY_HANDLER(IDC_PROJECTS, LVN_ITEMCHANGED, OnLvnItemchanged)
		CHAIN_MSG_MAP(CAxDialogImpl<CSelectProjectsDlg>)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnClickedOk(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnClickedCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnClickedSelectAllOrNone(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnLvnItemchanged(int idCtrl, NMHDR* pNMHDR, BOOL& bHandled);

private:
	const std::vector<std::unique_ptr<WCHAR[]>>& m_allProjectNames;

	std::vector<bool> m_selectedProjects;

};
