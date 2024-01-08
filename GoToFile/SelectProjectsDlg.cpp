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

/*
* TODO:
* - Ignore mismatched project persistence
* - Add resize logic
*/

#include "StdAfx.h"

#include <algorithm>

#include "SelectProjectsDlg.h"

CSelectProjectsDlg::CSelectProjectsDlg(const std::vector<std::unique_ptr<WCHAR[]>>& projectNames, const std::optional<std::vector<const WCHAR*>>& selectedProjects)
	: m_allProjectNames(projectNames)
{
	m_selectedProjects.resize(m_allProjectNames.size(), false);

	if (selectedProjects)
	{
		for (const WCHAR* selectedProject : *selectedProjects)
		{
			for (size_t index = 0; index < m_allProjectNames.size(); index++)
			{
				if (selectedProject == m_allProjectNames[index].get()) // Intentional pointer comparison
				{
					m_selectedProjects[index] = true;
					break;
				}
			}
		}
	}
}

CSelectProjectsDlg::~CSelectProjectsDlg()
{
}

LRESULT CSelectProjectsDlg::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CAxDialogImpl<CSelectProjectsDlg>::OnInitDialog(uMsg, wParam, lParam, bHandled);

	//HICON hIcon = LoadIcon(1, 32, 32);
	//if (hIcon)
	//{
	//	SetIcon(hIcon);
	//}

	HWND hwndProjectsList = GetDlgItem(IDC_PROJECTS);

	ListView_SetExtendedListViewStyle(hwndProjectsList, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);

	LVITEM lvI;

	// Initialize LVITEM members that are common to all items.
	lvI.mask = LVIF_TEXT | LVIF_STATE;
	lvI.stateMask = 0;
	lvI.iSubItem = 0;
	lvI.state = 0;

	// Initialize LVITEM members that are different for each item.
	for (size_t index = 0; index < m_allProjectNames.size(); index++)
	{
		lvI.iItem = static_cast<int>(index);
		lvI.iImage = static_cast<int>(index);
		lvI.pszText = m_allProjectNames[index].get();

		// Insert items into the list.
		ListView_InsertItem(hwndProjectsList, &lvI);

		// Can't add items as checked via InsertItem, it has to be done after the item is inserted
		ListView_SetCheckState(hwndProjectsList, static_cast<int>(index), m_selectedProjects[index]);
	}

	return 0;
}


LRESULT CSelectProjectsDlg::OnClickedOk(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	EndDialog(wID);
	return 0;
}

LRESULT CSelectProjectsDlg::OnClickedCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	EndDialog(wID);
	return 0;
}

LRESULT CSelectProjectsDlg::OnLvnItemchanged(int /*idCtrl*/, NMHDR* pNMHDR, BOOL& bHandled)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

	// Ignore the initial item changed notification triggered from inserting our item. In this case the old-state will be 0
	// but during normal operation it will shows as unchecked (1) or checked (2)
	if (pNMLV->uChanged & LVIF_STATE && pNMLV->uOldState != 0)
	{
		if (pNMLV->uNewState & LVIS_STATEIMAGEMASK)
		{
			const bool checked = (pNMLV->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK(2);
			m_selectedProjects[pNMLV->iItem] = checked;
		}
	}

	bHandled = false;
	return 0;
}


LRESULT CSelectProjectsDlg::OnClickedSelectAllOrNone(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
{
	const bool allSelected = std::all_of(m_selectedProjects.begin(), m_selectedProjects.end(), [](bool selected)
	{
		return selected;
	});

	const HWND hwndProjectsList = GetDlgItem(IDC_PROJECTS);
	for (size_t index = 0; index < m_selectedProjects.size(); index++)
	{
		m_selectedProjects[index] = !allSelected;
		ListView_SetCheckState(hwndProjectsList, static_cast<int>(index), m_selectedProjects[index]);
	}

	bHandled = true;
	return 0;
}



std::vector<const WCHAR*> CSelectProjectsDlg::GetSelectedProjects() const
{
	std::vector<const WCHAR*> selectedProjects;

	for (size_t index = 0; index < m_selectedProjects.size(); index++)
	{
		if (m_selectedProjects[index])
		{
			selectedProjects.push_back(m_allProjectNames[index].get());
		}
	}

	return selectedProjects;
}

