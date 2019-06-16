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

#include "stdafx.h"
#include "GoToComplementary.h"

namespace
{
	CComPtr<VxDTE::ProjectItem> FindX(const CComPtr<VxDTE::ProjectItems>& pParentProjectItems, LPCWSTR lpFileName)
	{
		LONG lCount = 0;
		if (SUCCEEDED(pParentProjectItems->get_Count(&lCount)))
		{
			for (LONG i = 1; i <= lCount; i++)
			{
				CComPtr<VxDTE::ProjectItem> pProjectItem;
				if (SUCCEEDED(pParentProjectItems->Item(CComVariant(i), &pProjectItem)) && pProjectItem)
				{
					short sCount = 0;
					if (SUCCEEDED(pProjectItem->get_FileCount(&sCount)))
					{
						for (short i = 1; i <= sCount; i++)
						{
							BSTR lpFilePath = NULL;
							if (SUCCEEDED(pProjectItem->get_FileNames(i, &lpFilePath)))
							{
								if (_wcsicmp(lpFilePath, lpFileName) == 0)
								{
									return pProjectItem;
								}
							}
						}
					}
					CComPtr<VxDTE::ProjectItems> pProjectItems;
					if (SUCCEEDED(pProjectItem->get_ProjectItems(&pProjectItems)) && pProjectItems)
					{
						CComPtr<VxDTE::ProjectItem> pFoundProjectItem = FindX(pProjectItems, lpFileName);
						if (pFoundProjectItem)
						{
							return pFoundProjectItem;
						}
					}
				}
			}
		}
		return CComPtr<VxDTE::ProjectItem>();
	}
}

bool CGoToComplementary::Query() const
{
	CComPtr<VxDTE::Document> pDocument;
	if (SUCCEEDED(m_pDTE->get_ActiveDocument(&pDocument)) && pDocument)
	{
		return true;
	}
	return false;
}

bool CGoToComplementary::Execute()
{
	CComPtr<VxDTE::Document> pDocument;
	if (SUCCEEDED(m_pDTE->get_ActiveDocument(&pDocument)) && pDocument)
	{
		BSTR lpFullName = NULL;
		if (SUCCEEDED(pDocument->get_FullName(&lpFullName)))
		{
			std::vector<std::wstring> FileNames;

			if (FileNames.empty())
			{
				GetProjectItemFiles(pDocument, FileNames);
			}

			if (FileNames.empty())
			{
				GetProjectItemChildren(pDocument, FileNames);
			}

			if (FileNames.empty())
			{
				GetProjectItemSiblings(pDocument, FileNames);
			}

			if (FileNames.empty())
			{
				GetProjectFiles(pDocument, FileNames);
			}

			if (!FileNames.empty())
			{
				size_t uiIndex = 0;
				for (size_t i = 0; i < FileNames.size(); i++)
				{
					if (_wcsicmp(lpFullName, FileNames[i].c_str()) == 0)
					{
						uiIndex = (i + 1) % FileNames.size();
						break;
					}
				}

				CComPtr<VxDTE::ItemOperations> pItemOperations;
				if (SUCCEEDED(m_pDTE->get_ItemOperations(&pItemOperations)) && pItemOperations)
				{
					WCHAR lpViewKindTextView[MAX_PATH + 1];
					MultiByteToWideChar(CP_ACP, 0, VxDTE::vsViewKindTextView, -1, lpViewKindTextView, MAX_PATH + 1);
					lpViewKindTextView[MAX_PATH] = '\0';

					CComPtr<VxDTE::Window> pWindow;
					return SUCCEEDED(pItemOperations->OpenFile(const_cast<LPWSTR>(FileNames[uiIndex].c_str()), lpViewKindTextView, &pWindow));
				}
			}
		}
	}
	return false;
}

bool CGoToComplementary::AddFileName(CComPtr<VxDTE::Document> pDocument, CComPtr<VxDTE::ProjectItem> pProjectItem, LPCWSTR lpFileName, std::vector<std::wstring>& FileNames, FileRestrictionFlags eFlags)
{
	BSTR lpFullName = NULL;
	if (SUCCEEDED(pDocument->get_FullName(&lpFullName)))
	{
		BSTR lpName = wcsrchr(lpFullName, L'\\');
		if (lpName == NULL)
		{
			lpName = lpFullName;
		}
		else
		{
			lpName++;
		}

		BSTR lpExtension = wcschr(lpName, L'.');
		if (lpExtension != NULL && _wcsnicmp(lpFullName, lpFileName, lpExtension - lpFullName + 1) == 0)
		{
			if (pProjectItem)
			{
				if ((eFlags & FRF_KindPhysicalFile) != FRF_None)
				{
					BSTR lpKind = NULL;
					if (SUCCEEDED(pProjectItem->get_Kind(&lpKind)) && lpKind)
					{
						WCHAR lpProjectItemKindPhysicalFile[MAX_PATH + 1];
						MultiByteToWideChar(CP_ACP, 0, VxDTE::vsProjectItemKindPhysicalFile, -1, lpProjectItemKindPhysicalFile, MAX_PATH + 1);
						lpProjectItemKindPhysicalFile[MAX_PATH] = '\0';

						if (_wcsicmp(lpKind, lpProjectItemKindPhysicalFile) != 0)
						{
							return false;
						}
					}
				}
				if ((eFlags & FRF_CodeFile) != FRF_None)
				{
					CComPtr<VxDTE::FileCodeModel> pFileCodeModel;
					if (!SUCCEEDED(pProjectItem->get_FileCodeModel(&pFileCodeModel)) || !pFileCodeModel)
					{
						return false;
					}
				}
			}
			FileNames.push_back(lpFileName);
			return true;
		}
	}
	return false;
}

void CGoToComplementary::GetProjectItemFiles(CComPtr<VxDTE::Document> pDocument, std::vector<std::wstring>& FileNames)
{
	CComPtr<VxDTE::ProjectItem> pProjectItem;
	if (SUCCEEDED(pDocument->get_ProjectItem(&pProjectItem)) && pProjectItem)
	{
		short sCount = 0;
		if (SUCCEEDED(pProjectItem->get_FileCount(&sCount)) && sCount > 1)
		{
			for (short i = 1; i <= sCount; i++)
			{
				BSTR lpFilePath = NULL;
				if (SUCCEEDED(pProjectItem->get_FileNames(i, &lpFilePath)))
				{
					AddFileName(pDocument, pProjectItem, lpFilePath, FileNames);
				}
			}
		}
	}
}

void CGoToComplementary::GetProjectItemChildren(CComPtr<VxDTE::Document> pDocument, std::vector<std::wstring>& FileNames)
{
	CComPtr<VxDTE::ProjectItem> pProjectItem;
	if (SUCCEEDED(pDocument->get_ProjectItem(&pProjectItem)) && pProjectItem)
	{
		CComPtr<VxDTE::ProjectItems> pParentProjectItems;
		if (FileNames.empty())
		{
			pParentProjectItems = NULL;
			if (SUCCEEDED(pProjectItem->get_ProjectItems(&pParentProjectItems)) && pParentProjectItems)
			{
				LONG lCount = 0;
				if (SUCCEEDED(pParentProjectItems->get_Count(&lCount)) && lCount > 0)
				{
					short sCount = 0;
					if (SUCCEEDED(pProjectItem->get_FileCount(&sCount)) && sCount == 1)
					{
						BSTR lpFilePath = NULL;
						if (SUCCEEDED(pProjectItem->get_FileNames(1, &lpFilePath)))
						{
							AddFileName(pDocument, pProjectItem, lpFilePath, FileNames, static_cast<FileRestrictionFlags>(FRF_Default & ~FRF_CodeFile));
						}
					}
				}
			}
		}
		if (FileNames.empty())
		{
			pParentProjectItems = NULL;
			if (SUCCEEDED(pProjectItem->get_Collection(&pParentProjectItems)) && pParentProjectItems)
			{
				CComPtr<IDispatch> pParent;
				if (SUCCEEDED(pParentProjectItems->get_Parent(&pParent)))
				{
					CComPtr<VxDTE::ProjectItem> pParentProjectItem;
					pParentProjectItem = pParent;
					if (pParentProjectItem)
					{
						short sCount = 0;
						if (SUCCEEDED(pParentProjectItem->get_FileCount(&sCount)) && sCount == 1)
						{
							BSTR lpFilePath = NULL;
							if (SUCCEEDED(pParentProjectItem->get_FileNames(1, &lpFilePath)))
							{
								AddFileName(pDocument, pParentProjectItem, lpFilePath, FileNames, static_cast<FileRestrictionFlags>(FRF_Default & ~FRF_CodeFile));
							}
						}
					}
				}
			}
		}
		if (FileNames.size() == 1)
		{
			LONG lCount = 0;
			if (SUCCEEDED(pParentProjectItems->get_Count(&lCount)))
			{
				for (LONG i = 1; i <= lCount; i++)
				{
					CComPtr<VxDTE::ProjectItem> pSiblingProjectItem;
					if (SUCCEEDED(pParentProjectItems->Item(CComVariant(i), &pSiblingProjectItem)) && pSiblingProjectItem)
					{
						short sCount = 0;
						if (SUCCEEDED(pSiblingProjectItem->get_FileCount(&sCount)) && sCount == 1)
						{
							BSTR lpFilePath = NULL;
							if (SUCCEEDED(pSiblingProjectItem->get_FileNames(1, &lpFilePath)))
							{
								AddFileName(pDocument, pSiblingProjectItem, lpFilePath, FileNames, static_cast<FileRestrictionFlags>(FRF_Default & ~FRF_CodeFile));
							}
						}
					}
				}
				if (FileNames.size() == 1)
				{
					FileNames.clear();
				}
			}
		}
	}
}

void CGoToComplementary::GetProjectItemSiblings(CComPtr<VxDTE::Document> pDocument, std::vector<std::wstring>& FileNames)
{
	CComPtr<VxDTE::ProjectItem> pProjectItem;
	if (SUCCEEDED(pDocument->get_ProjectItem(&pProjectItem)) && pProjectItem)
	{
		CComPtr<VxDTE::ProjectItems> pParentProjectItems;
		if (SUCCEEDED(pProjectItem->get_Collection(&pParentProjectItems)) && pParentProjectItems)
		{
			LONG lCount = 0;
			if (SUCCEEDED(pParentProjectItems->get_Count(&lCount)))
			{
				bool bAddedProjectItem = false;
				for (LONG i = 1; i <= lCount; i++)
				{
					CComPtr<VxDTE::ProjectItem> pSiblingProjectItem;
					if (SUCCEEDED(pParentProjectItems->Item(CComVariant(i), &pSiblingProjectItem)) && pSiblingProjectItem)
					{
						short sCount = 0;
						if (SUCCEEDED(pSiblingProjectItem->get_FileCount(&sCount)) && sCount == 1)
						{
							BSTR lpFilePath = NULL;
							if (SUCCEEDED(pSiblingProjectItem->get_FileNames(1, &lpFilePath)))
							{
								if (AddFileName(pDocument, pSiblingProjectItem, lpFilePath, FileNames))
								{
									bAddedProjectItem |= pProjectItem == pSiblingProjectItem;
								}
							}
						}
					}
				}
				if (bAddedProjectItem && FileNames.size() == 1)
				{
					FileNames.clear();
				}
			}
		}
	}
}

void CGoToComplementary::GetProjectFiles(CComPtr<VxDTE::Document> pDocument, std::vector<std::wstring>& FileNames)
{
	BSTR lpFullName = NULL;
	if (SUCCEEDED(pDocument->get_FullName(&lpFullName)))
	{
		BSTR lpName = wcsrchr(lpFullName, L'\\');
		if (lpName == NULL)
		{
			lpName = lpFullName;
		}
		else
		{
			lpName++;
		}

		BSTR lpExtension = wcschr(lpName, L'.');
		if (lpExtension != NULL)
		{
			std::wstring Search(lpFullName, lpExtension - lpFullName + 1);
			Search += L'*';

			WIN32_FIND_DATA FindData;
			HANDLE hFindFile = FindFirstFile(Search.c_str(), &FindData);
			if (hFindFile != INVALID_HANDLE_VALUE)
			{
				do
				{
					if ((FindData.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_DIRECTORY)) == 0)
					{
						LPCWSTR lpFindFileExtension = wcschr(FindData.cFileName, L'.');
						if (lpFindFileExtension != NULL)
						{
							std::wstring FindFileName(lpFullName, lpExtension - lpFullName);
							FindFileName += lpFindFileExtension;

							bool bHasProject = false;
							CComPtr<VxDTE::ProjectItem> pProjectItem;
							if (SUCCEEDED(pDocument->get_ProjectItem(&pProjectItem)) && pProjectItem)
							{
								CComPtr<VxDTE::Project> pProject;
								if (SUCCEEDED(pProjectItem->get_ContainingProject(&pProject)) && pProject)
								{
									BSTR lpFullProjectName = NULL;
									if (SUCCEEDED(pProject->get_FullName(&lpFullProjectName)) && lpFullProjectName != NULL && *lpFullProjectName != L'\0')
									{
										CComPtr<VxDTE::ProjectItems> pParentProjectItems;
										if (SUCCEEDED(pProject->get_ProjectItems(&pParentProjectItems)) && pParentProjectItems)
										{
											CComPtr<VxDTE::ProjectItem> pProjectItem = FindX(pParentProjectItems, FindFileName.c_str());
											if (pProjectItem)
											{
												AddFileName(pDocument, pProjectItem, FindFileName.c_str(), FileNames);
											}
											bHasProject = true;
										}
									}
								}
							}
							if (!bHasProject)
							{
								FileNames.push_back(FindFileName);
							}
						}
					}
				} while (FindNextFile(hFindFile, &FindData));
				FindClose(hFindFile);
			}
		}
	}
}