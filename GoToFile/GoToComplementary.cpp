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
#include "GoToComplementary.h"

namespace {
CComPtr<VxDTE::ProjectItem> FindProjectItem(const CComPtr<VxDTE::ProjectItems>& spParentProjectItems, LPCWSTR lpFileName)
{
	LONG lCount = 0;
	if (SUCCEEDED(spParentProjectItems->get_Count(&lCount)))
	{
		for (LONG i = 1; i <= lCount; i++)
		{
			CComPtr<VxDTE::ProjectItem> spProjectItem;
			if (SUCCEEDED(spParentProjectItems->Item(CComVariant(i), &spProjectItem)) && spProjectItem)
			{
				short sCount = 0;
				if (SUCCEEDED(spProjectItem->get_FileCount(&sCount)))
				{
					for (short j = 1; i <= sCount; i++)
					{
						CComBSTR spFilePath;
						if (SUCCEEDED(spProjectItem->get_FileNames(j, &spFilePath)))
						{
							if (_wcsicmp(spFilePath, lpFileName) == 0)
							{
								return spProjectItem;
							}
						}
					}
				}
				CComPtr<VxDTE::ProjectItems> spProjectItems;
				if (SUCCEEDED(spProjectItem->get_ProjectItems(&spProjectItems)) && spProjectItems)
				{
					CComPtr<VxDTE::ProjectItem> spFoundProjectItem = FindProjectItem(spProjectItems, lpFileName);
					if (spFoundProjectItem)
					{
						return spFoundProjectItem;
					}
				}
			}
		}
	}
	return CComPtr<VxDTE::ProjectItem>();
}
}

void CGoToComplementary::CacheStrings()
{
	MultiByteToWideChar(CP_ACP, 0, VxDTE::vsProjectItemKindPhysicalFile, -1, m_wzProjectItemKindPhysicalFile, MAX_PATH + 1);
	m_wzProjectItemKindPhysicalFile[MAX_PATH] = '\0';
}


bool CGoToComplementary::Query() const
{
	CComPtr<VxDTE::Document> spDocument;
	if (SUCCEEDED(m_spDTE->get_ActiveDocument(&spDocument)) && spDocument)
	{
		return true;
	}
	return false;
}

bool CGoToComplementary::Execute()
{
	CacheStrings();

	CComPtr<VxDTE::Document> spDocument;
	if (SUCCEEDED(m_spDTE->get_ActiveDocument(&spDocument)) && spDocument)
	{
		CComBSTR spFullName;
		if (SUCCEEDED(spDocument->get_FullName(&spFullName)))
		{
			std::vector<std::wstring> fileNames;

			if (fileNames.empty())
			{
				GetProjectItemFiles(spDocument, fileNames);
			}

			if (fileNames.empty())
			{
				GetProjectItemChildren(spDocument, fileNames);
			}

			if (fileNames.empty())
			{
				GetProjectItemSiblings(spDocument, fileNames);
			}

			if (fileNames.empty())
			{
				GetProjectFiles(spDocument, fileNames);
			}

			if (!fileNames.empty())
			{
				size_t uiIndex = 0;
				for (size_t i = 0; i < fileNames.size(); i++)
				{
					if (_wcsicmp(spFullName, fileNames[i].c_str()) == 0)
					{
						uiIndex = (i + 1) % fileNames.size();
						break;
					}
				}

				CComPtr<VxDTE::ItemOperations> spItemOperations;
				if (SUCCEEDED(m_spDTE->get_ItemOperations(&spItemOperations)) && spItemOperations)
				{
					WCHAR lpViewKindTextView[MAX_PATH + 1];
					MultiByteToWideChar(CP_ACP, 0, VxDTE::vsViewKindTextView, -1, lpViewKindTextView, MAX_PATH + 1);
					lpViewKindTextView[MAX_PATH] = '\0';

					CComPtr<VxDTE::Window> spWindow;
					return SUCCEEDED(spItemOperations->OpenFile(const_cast<LPWSTR>(fileNames[uiIndex].c_str()), lpViewKindTextView, &spWindow));
				}
			}
		}
	}
	return false;
}

bool CGoToComplementary::AddFileName(VxDTE::Document* pDocument, VxDTE::ProjectItem* pProjectItem, LPCWSTR lpFileName, std::vector<std::wstring>& fileNames, FileRestrictionFlags eFlags)
{
	CComBSTR spFullName;
	if (SUCCEEDED(pDocument->get_FullName(&spFullName)))
	{
		LPCWSTR lpName = wcsrchr(spFullName, L'\\');
		if (lpName == NULL)
		{
			lpName = spFullName;
		}
		else
		{
			lpName++;
		}

		LPCWSTR lpExtension = wcschr(lpName, L'.');
		if (lpExtension != NULL && _wcsnicmp(spFullName, lpFileName, lpExtension - spFullName + 1) == 0)
		{
			if (pProjectItem)
			{
				if ((eFlags & FRF_KindPhysicalFile) != FRF_None)
				{
					CComBSTR spKind;
					if (SUCCEEDED(pProjectItem->get_Kind(&spKind)) && spKind)
					{
						if (_wcsicmp(spKind, m_wzProjectItemKindPhysicalFile) != 0)
						{
							return false;
						}
					}
				}

				if ((eFlags & FRF_CodeFile) != FRF_None)
				{
					CComPtr<VxDTE::FileCodeModel> spFileCodeModel;
					if (!SUCCEEDED(pProjectItem->get_FileCodeModel(&spFileCodeModel)) || !spFileCodeModel)
					{
						return false;
					}
				}
			}
			fileNames.emplace_back(lpFileName);
			return true;
		}
	}
	return false;
}

void CGoToComplementary::GetProjectItemFiles(VxDTE::Document* pDocument, std::vector<std::wstring>& fileNames)
{
	CComPtr<VxDTE::ProjectItem> spProjectItem;
	if (SUCCEEDED(pDocument->get_ProjectItem(&spProjectItem)) && spProjectItem)
	{
		short sCount = 0;
		if (SUCCEEDED(spProjectItem->get_FileCount(&sCount)) && sCount > 1)
		{
			for (short i = 1; i <= sCount; i++)
			{
				CComBSTR spFilePath;
				if (SUCCEEDED(spProjectItem->get_FileNames(i, &spFilePath)))
				{
					AddFileName(pDocument, spProjectItem, spFilePath, fileNames);
				}
			}
		}
	}
}

void CGoToComplementary::GetProjectItemChildren(VxDTE::Document* pDocument, std::vector<std::wstring>& fileNames)
{
	CComPtr<VxDTE::ProjectItem> spProjectItem;
	if (SUCCEEDED(pDocument->get_ProjectItem(&spProjectItem)) && spProjectItem)
	{
		CComPtr<VxDTE::ProjectItems> spParentProjectItems;
		if (fileNames.empty())
		{
			if (SUCCEEDED(spProjectItem->get_ProjectItems(&spParentProjectItems)) && spParentProjectItems)
			{
				LONG lCount = 0;
				if (SUCCEEDED(spParentProjectItems->get_Count(&lCount)) && lCount > 0)
				{
					short sCount = 0;
					if (SUCCEEDED(spProjectItem->get_FileCount(&sCount)) && sCount == 1)
					{
						CComBSTR spFilePath;
						if (SUCCEEDED(spProjectItem->get_FileNames(1, &spFilePath)))
						{
							AddFileName(pDocument, spProjectItem, spFilePath, fileNames, static_cast<FileRestrictionFlags>(FRF_Default & ~FRF_CodeFile));
						}
					}
				}
			}
		}

		if (fileNames.empty())
		{
			spParentProjectItems = NULL;
			if (SUCCEEDED(spProjectItem->get_Collection(&spParentProjectItems)) && spParentProjectItems)
			{
				CComPtr<IDispatch> spParent;
				if (SUCCEEDED(spParentProjectItems->get_Parent(&spParent)))
				{
					CComPtr<VxDTE::ProjectItem> spParentProjectItem;
					spParentProjectItem = spParent;
					if (spParentProjectItem)
					{
						short sCount = 0;
						if (SUCCEEDED(spParentProjectItem->get_FileCount(&sCount)) && sCount == 1)
						{
							CComBSTR spFilePath;
							if (SUCCEEDED(spParentProjectItem->get_FileNames(1, &spFilePath)))
							{
								AddFileName(pDocument, spParentProjectItem, spFilePath, fileNames, static_cast<FileRestrictionFlags>(FRF_Default & ~FRF_CodeFile));
							}
						}
					}
				}
			}
		}

		if (fileNames.size() == 1)
		{
			LONG lCount = 0;
			if (SUCCEEDED(spParentProjectItems->get_Count(&lCount)))
			{
				for (LONG i = 1; i <= lCount; i++)
				{
					CComPtr<VxDTE::ProjectItem> spSiblingProjectItem;
					if (SUCCEEDED(spParentProjectItems->Item(CComVariant(i), &spSiblingProjectItem)) && spSiblingProjectItem)
					{
						short sCount = 0;
						if (SUCCEEDED(spSiblingProjectItem->get_FileCount(&sCount)) && sCount == 1)
						{
							CComBSTR spFilePath;
							if (SUCCEEDED(spSiblingProjectItem->get_FileNames(1, &spFilePath)))
							{
								AddFileName(pDocument, spSiblingProjectItem, spFilePath, fileNames, static_cast<FileRestrictionFlags>(FRF_Default & ~FRF_CodeFile));
							}
						}
					}
				}

				if (fileNames.size() == 1)
				{
					fileNames.clear();
				}
			}
		}
	}
}

void CGoToComplementary::GetProjectItemSiblings(VxDTE::Document* pDocument, std::vector<std::wstring>& fileNames)
{
	CComPtr<VxDTE::ProjectItem> spProjectItem;
	if (SUCCEEDED(pDocument->get_ProjectItem(&spProjectItem)) && spProjectItem)
	{
		CComPtr<VxDTE::ProjectItems> spParentProjectItems;
		if (SUCCEEDED(spProjectItem->get_Collection(&spParentProjectItems)) && spParentProjectItems)
		{
			LONG lCount = 0;
			if (SUCCEEDED(spParentProjectItems->get_Count(&lCount)))
			{
				bool bAddedProjectItem = false;
				for (LONG i = 1; i <= lCount; i++)
				{
					CComPtr<VxDTE::ProjectItem> spSiblingProjectItem;
					if (SUCCEEDED(spParentProjectItems->Item(CComVariant(i), &spSiblingProjectItem)) && spSiblingProjectItem)
					{
						short sCount = 0;
						if (SUCCEEDED(spSiblingProjectItem->get_FileCount(&sCount)) && sCount == 1)
						{
							CComBSTR spFilePath;
							if (SUCCEEDED(spSiblingProjectItem->get_FileNames(1, &spFilePath)))
							{
								if (AddFileName(pDocument, spSiblingProjectItem, spFilePath, fileNames))
								{
									bAddedProjectItem |= spProjectItem == spSiblingProjectItem;
								}
							}
						}
					}
				}

				if (bAddedProjectItem && fileNames.size() == 1)
				{
					fileNames.clear();
				}
			}
		}
	}
}

void CGoToComplementary::GetProjectFiles(VxDTE::Document* pDocument, std::vector<std::wstring>& fileNames)
{
	CComBSTR spFullName;
	if (SUCCEEDED(pDocument->get_FullName(&spFullName)))
	{
		LPCWSTR lpName = wcsrchr(spFullName, L'\\');
		if (lpName == NULL)
		{
			lpName = spFullName;
		}
		else
		{
			lpName++;
		}

		LPCWSTR lpExtension = wcschr(lpName, L'.');
		if (lpExtension != NULL)
		{
			std::wstring Search(spFullName, lpExtension - spFullName + 1);
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
							std::wstring findFileName(spFullName, lpExtension - spFullName);
							findFileName += lpFindFileExtension;

							bool bHasProject = false;
							CComPtr<VxDTE::ProjectItem> spProjectItem;
							if (SUCCEEDED(pDocument->get_ProjectItem(&spProjectItem)) && spProjectItem)
							{
								CComPtr<VxDTE::Project> spProject;
								if (SUCCEEDED(spProjectItem->get_ContainingProject(&spProject)) && spProject)
								{
									CComBSTR spFullProjectName;
									if (SUCCEEDED(spProject->get_FullName(&spFullProjectName)) && spFullProjectName != NULL && *spFullProjectName != L'\0')
									{
										CComPtr<VxDTE::ProjectItems> spParentProjectItems;
										if (SUCCEEDED(spProject->get_ProjectItems(&spParentProjectItems)) && spParentProjectItems)
										{
											CComPtr<VxDTE::ProjectItem> spSubProjectItem = FindProjectItem(spParentProjectItems, findFileName.c_str());
											if (spSubProjectItem)
											{
												AddFileName(pDocument, spSubProjectItem, findFileName.c_str(), fileNames);
											}
											bHasProject = true;
										}
									}
								}
							}

							if (!bHasProject)
							{
								fileNames.push_back(std::move(findFileName));
							}
						}
					}
				} while (FindNextFile(hFindFile, &FindData));

				FindClose(hFindFile);
			}
		}
	}
}