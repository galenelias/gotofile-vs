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

enum EViewKind : DWORD
{
	VIEW_KIND_PRIMARY,
	VIEW_KIND_CODE,
	VIEW_KIND_DESIGNER,
	VIEW_KIND_TEXTVIEW,
	VIEW_KIND_DEBUGGING
};

class GoToFileSettings
{
public:
	class CGoToFileDlg& goToFileDlg;

	GoToFileSettings(class CGoToFileDlg& goToFileDlg)
		: goToFileDlg(goToFileDlg)
		, m_bRestoring(false)
		, m_iFileNameWidth(150)
		, m_iFilePathWidth(250)
		, m_iProjectNameWidth(150)
		, m_iProjectPathWidth(250)
		, m_eViewKind(VIEW_KIND_PRIMARY)
	{
		m_location.x = 0;
		m_location.y = 0;

		m_size.cx = 400;
		m_size.cy = 300;
	}

	bool Restoring() const
	{
		return m_bRestoring;
	}

	const std::wstring& GetBrowsePath() const
	{
		return m_browsePath;
	}

	void SetBrowsePath(LPCWSTR lpBrowsePath)
	{
		m_browsePath = lpBrowsePath;
	}

	void Store();
	void Restore();

	void Read();
	void Write();

	bool ReadFromKey(LPCWSTR pwzRegKey);
private:
	bool m_bRestoring;

	POINT m_location;
	SIZE m_size;

	long m_iFileNameWidth;
	long m_iFilePathWidth;
	long m_iProjectNameWidth;
	long m_iProjectPathWidth;

	std::wstring m_project;
	std::wstring m_filter;
	std::wstring m_browsePath;
	EViewKind m_eViewKind;
};