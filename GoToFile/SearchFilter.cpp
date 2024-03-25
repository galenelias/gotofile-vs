#include "StdAfx.h"

#include "SearchFilter.h"

WCHAR SFilter::Normalize(WCHAR cChar, ESearchField m_eSearchField)
{
	cChar = static_cast<WCHAR>(tolower(cChar));
	if ((m_eSearchField == SEARCH_FIELD_FILE_PATH || m_eSearchField == SEARCH_FIELD_PROJECT_PATH) && cChar == L'/')
	{
		cChar = L'\\';
	}
	return cChar;
}

int SFilter::Like(LPCWSTR lpSearch, LPCWSTR m_lpFilter, ESearchField m_eSearchField)
{
	LPCWSTR lpSearchStart = lpSearch;
	LPCWSTR lpSearchMatch = nullptr;

	while (*m_lpFilter)
	{
		if (*m_lpFilter == L'*')
		{
			if (m_lpFilter[1] == L'*')
			{
				m_lpFilter++;
				continue;
			}
			else if (m_lpFilter[1] == L'\0')
			{
				return 0;
			}
			else
			{
				m_lpFilter++;
				while (*lpSearch)
				{
					int iResult = Like(lpSearch, m_lpFilter, m_eSearchField);
					if (iResult >= 0)
					{
						return lpSearchMatch 
							? static_cast<int>(lpSearchMatch - lpSearchStart) 
							: static_cast<int>(lpSearch - lpSearchStart) + iResult;
					}
					lpSearch++;
				}

				return -1;
			}
		}
		else if (*m_lpFilter == L'?')
		{
			if (*lpSearch == L'\0')
			{
				return -1;
			}
			m_lpFilter++;
			lpSearch++;
		}
		else
		{
			if (*lpSearch == L'\0')
			{
				return -1;
			}
			else if (*m_lpFilter != Normalize(*lpSearch, m_eSearchField))
			{
				return -1;
			}
			if (lpSearchMatch == nullptr)
			{
				lpSearchMatch = lpSearch;
			}
			m_lpFilter++;
			lpSearch++;
		}
	}

	if (*lpSearch == L'\0')
	{
		return lpSearchMatch ? static_cast<int>(lpSearchMatch - lpSearchStart) : 0;
	}
	return -1;
}

int SFilter::Match(const SFile& file) const
{
	LPCWSTR lpSearch;
	switch (m_eSearchField)
	{
		case SEARCH_FIELD_FILE_NAME:
			lpSearch = file.spFilePath.get() + file.uiFileName;
			break;
		case SEARCH_FIELD_FILE_PATH:
			lpSearch = file.spFilePath.get();
			break;
		case SEARCH_FIELD_PROJECT_NAME:
			lpSearch = file.lpProjectName;
			break;
		case SEARCH_FIELD_PROJECT_PATH:
			lpSearch = file.lpProjectPath;
			break;
		default:
			return -1;
	}

	int iMatch = -1;

	size_t iFileLength = wcslen(lpSearch);
	size_t iFilterLength = wcslen(m_lpFilter);
	if (m_bWildcard)
	{
		iMatch = Like(lpSearch, m_lpFilter, m_eSearchField);
	}
	else if (iFilterLength <= iFileLength)
	{
		for (size_t i = 0; i <= iFileLength - iFilterLength; i++)
		{
			bool bSubMatch = true;
			for (size_t j = 0; j < iFilterLength; j++)
			{
				if (Normalize(lpSearch[i + j], m_eSearchField) != m_lpFilter[j])
				{
					bSubMatch = false;
					break;
				}
			}
			if (bSubMatch)
			{
				iMatch = static_cast<int>(i);
				break;
			}
		}
	}

	if (m_eLogicOperator & LOGIC_OPERATOR_NOT_MASK)
	{
		if (iMatch < 0)
		{
			iMatch = static_cast<int>(iFileLength - iFilterLength);
		}
		else
		{
			iMatch = -1;
		}
	}

	return iMatch;
}