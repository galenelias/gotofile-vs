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
 *
 *	You may contact the author at ryansgregg@hotmail.com or visit
 *	http://nemesis.thewavelength.net/ for more information.
 */

#pragma once

#include "stdafx.h"

class CGoToComplementary
{
public:
	CGoToComplementary(CComPtr<VxDTE::_DTE> pDTE) : m_pDTE(pDTE)
	{
	}

	bool Query() const;
	bool Execute();

private:
	enum FileRestrictionFlags
	{
		FRF_None = 0x00,
		FRF_KindPhysicalFile = 0x01,
		FRF_CodeFile = 0x02,
		FRF_Default = FRF_KindPhysicalFile //| FRF_CodeFile
	};

	bool AddFileName(CComPtr<VxDTE::Document> pDocument, CComPtr<VxDTE::ProjectItem> pProjectItem, LPCWSTR lpFileName, std::vector<std::wstring>& FileNames, FileRestrictionFlags eFlags = FRF_Default);

	void GetProjectItemFiles(CComPtr<VxDTE::Document> pDocument, std::vector<std::wstring>& FileNames);
	void GetProjectItemChildren(CComPtr<VxDTE::Document> pDocument, std::vector<std::wstring>& FileNames);
	void GetProjectItemSiblings(CComPtr<VxDTE::Document> pDocument, std::vector<std::wstring>& FileNames);
	void GetProjectFiles(CComPtr<VxDTE::Document> pDocument, std::vector<std::wstring>& FileNames);

	CComPtr<VxDTE::_DTE> m_pDTE;
};