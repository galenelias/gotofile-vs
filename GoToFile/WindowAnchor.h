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

#include <Windows.h>

enum class EAnchor
{
	None = 0x00,
	Top = 0x01,
	Bottom = 0x02,
	Left = 0x04,
	Right = 0x08
};

DEFINE_ENUM_FLAG_OPERATORS(EAnchor)


class SAnchor
{
public:
	void Init(HWND hParent, HWND hWindowParam, EAnchor eAnchorParam)
	{
		m_hWindow = hWindowParam;
		m_eAnchor = eAnchorParam;

		::GetWindowRect(m_hWindow, &m_rect);
		::MapWindowPoints(HWND_DESKTOP, hParent, reinterpret_cast<LPPOINT>(&m_rect), 2);
	}

	void Adjust(const RECT& RectParam)
	{
		this->m_rect.right = this->m_rect.left + (RectParam.right - RectParam.left);
		this->m_rect.bottom = this->m_rect.top + (RectParam.bottom - RectParam.top);
	}

	void Update(LONG iDeltaX, LONG iDeltaY) const
	{
		if (m_hWindow != NULL)
		{
			RECT NewRect = m_rect;
			if ((m_eAnchor & EAnchor::Top) == EAnchor::None)
				NewRect.top += iDeltaY;
			if ((m_eAnchor & EAnchor::Bottom) != EAnchor::None)
				NewRect.bottom += iDeltaY;
			if ((m_eAnchor & EAnchor::Left) == EAnchor::None)
				NewRect.left += iDeltaX;
			if ((m_eAnchor & EAnchor::Right) != EAnchor::None)
				NewRect.right += iDeltaX;

			::MoveWindow(m_hWindow, NewRect.left, NewRect.top, NewRect.right - NewRect.left, NewRect.bottom - NewRect.top, TRUE);
		}
	}

private:
	HWND m_hWindow = 0;
	EAnchor m_eAnchor = EAnchor::None;
	RECT m_rect = {};
};
