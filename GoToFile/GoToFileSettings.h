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
		, bRestoring(false)
		, iFileNameWidth(150)
		, iFilePathWidth(250)
		, iProjectNameWidth(150)
		, iProjectPathWidth(250)
		, eViewKind(VIEW_KIND_PRIMARY)
	{
		Location.x = 0;
		Location.y = 0;

		Size.cx = 400;
		Size.cy = 300;
	}

	inline bool Restoring() const
	{
		return bRestoring;
	}

	inline const std::wstring& GetBrowsePath() const
	{
		return BrowsePath;
	}

	inline void SetBrowsePath(LPCWSTR lpBrowsePath)
	{
		BrowsePath = lpBrowsePath;
	}

	void Store();
	void Restore();

	void Read();
	void Write();

private:
	bool bRestoring;

	POINT Location;
	SIZE Size;

	long iFileNameWidth;
	long iFilePathWidth;
	long iProjectNameWidth;
	long iProjectPathWidth;

	std::wstring Project;
	std::wstring Filter;
	std::wstring BrowsePath;
	EViewKind eViewKind;
};