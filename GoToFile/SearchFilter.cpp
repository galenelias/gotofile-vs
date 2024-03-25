#include "StdAfx.h"

#include "SearchFilter.h"

#include <optional>


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


// Attempt to parse line number offsets, e.g.  "Foo.cpp(32,10)"
static std::tuple<std::wstring_view::size_type, std::optional<int>, std::optional<int>> ParseParenLineAndColumn(std::wstring_view svFilePath)
{
	std::optional<int> destinationLine;
	std::optional<int> destinationColumn;
	auto posParen = svFilePath.find(L'(');

	if (posParen != svFilePath.npos)
	{
		auto posInvalidChar = svFilePath.find_first_not_of(L"0123456789,)", posParen + 1); // Only parse the remainder as a line/column indicator if there are no unexpected characters

		if (posInvalidChar == svFilePath.npos)
		{
			int lineResult = _wtoi(svFilePath.data() + posParen + 1);
			if (lineResult != 0)
				destinationLine = lineResult;

			auto posComma = svFilePath.find(L',', posParen + 1);
			if (posComma != svFilePath.npos)
			{
				int colResult = _wtoi(svFilePath.data() + posComma + 1);
				if (colResult != 0)
					destinationColumn = colResult;
			}
		}
	}

	return { posParen, destinationLine, destinationColumn };
}


static std::tuple<std::wstring_view::size_type, std::optional<int>, std::optional<int>> ParseColonLineAndColumn(std::wstring_view svFilePath)
{
	std::optional<int> destinationLine;
	std::optional<int> destinationColumn;

	auto posColon = svFilePath.find(L':');
	if (posColon != svFilePath.npos)
	{
		auto posInvalidChar = svFilePath.find_first_not_of(L"0123456789:", posColon + 1);  // Only parse the remainder as a line/column indicator if there are no unexpected characters

		if (posInvalidChar == svFilePath.npos)
		{
			int lineResult = _wtoi(svFilePath.data() + posColon + 1);
			if (lineResult != 0)
				destinationLine = lineResult;

			auto posSecondColon = svFilePath.find(L':', posColon + 1);
			if (posSecondColon != svFilePath.npos)
			{
				int colResult = _wtoi(svFilePath.data() + posSecondColon + 1);
				if (colResult != 0)
					destinationColumn = colResult;
			}
		}
	}

	return { posColon, destinationLine, destinationColumn };
}


static void TryParseLineAndColumns(LPWSTR lpStart, LPCWSTR lpEnd, _Inout_ int* pDestinationLine, _Inout_ int* pDestinationColumn)
{
	// Check for line/column indicators in the file path
	std::wstring_view svFilePath(lpStart, lpEnd - lpStart);

	size_t posParen;
	std::optional<int> destinationLine, destinationColumn;

	std::tie(posParen, destinationLine, destinationColumn) = ParseParenLineAndColumn(svFilePath);
	if (posParen != svFilePath.npos)
	{
		lpStart[posParen] = '\0';
	}
	else
	{
		size_t posColon;
		std::tie(posColon, destinationLine, destinationColumn) = ParseColonLineAndColumn(svFilePath);
		if (posColon != svFilePath.npos)
			lpStart[posColon] = '\0';
	}

	if (destinationLine.has_value())
		*pDestinationLine = *destinationLine;

	if (destinationColumn.has_value())
		*pDestinationColumn = *destinationColumn;
}


void CreateFilterList(const CComBSTR& spFilter, std::unique_ptr<WCHAR[]>& spFilterStringTable, std::vector<SFilter>& filters, _Inout_ int* pDestinationLine, _Inout_ int* pDestinationColumn)
{
	const size_t cchFilter = spFilter.Length() + 1;
	spFilterStringTable = std::make_unique<WCHAR[]>(cchFilter);
	wcscpy_s(spFilterStringTable.get(), cchFilter, spFilter);

	LPWSTR lpFilter = nullptr;
	ESearchField eSearchField = SEARCH_FIELD_FILE_NAME;
	ELogicOperator eLogicOperator = LOGIC_OPERATOR_NONE;
	bool bNot = false;
	bool bQuoted = false;

	bool bParse = true;
	for (LPWSTR pChar = spFilterStringTable.get(); bParse; pChar++)
	{
		bool bDone = false;
		switch(*pChar)
		{
		case L'\0':
			bParse = false;
			bDone = true;
			break;
		case L'&':
			if (lpFilter == nullptr && eLogicOperator == LOGIC_OPERATOR_NONE && !bQuoted)
			{
				eLogicOperator = LOGIC_OPERATOR_AND;
				continue;
			}
			break;
		case L'|':
			if (lpFilter == nullptr && eLogicOperator == LOGIC_OPERATOR_NONE && !bQuoted)
			{
				eLogicOperator = LOGIC_OPERATOR_OR;
				continue;
			}
			break;
		case L'-':
		case L'!':
			if (lpFilter == nullptr && !bNot && !bQuoted)
			{
				bNot = true;
				continue;
			}
			break;
		case L'"':
			if (lpFilter == nullptr && !bQuoted)
			{
				bQuoted = true;
				continue;
			}
			else if (bQuoted)
			{
				bDone = true;
			}
			break;
		case L'\\':
		case L'/':
			if (lpFilter == nullptr && !bQuoted)
			{
				if (eSearchField == SEARCH_FIELD_FILE_NAME)
				{
					eSearchField = SEARCH_FIELD_FILE_PATH;
				}
				else if (eSearchField == SEARCH_FIELD_PROJECT_NAME)
				{
					eSearchField = SEARCH_FIELD_PROJECT_PATH;
				}
				continue;
			}
			break;
		case L':':
			if (lpFilter == nullptr && !bQuoted)
			{
				if (eSearchField == SEARCH_FIELD_FILE_NAME)
				{
					eSearchField = SEARCH_FIELD_PROJECT_NAME;
				}
				else if (eSearchField == SEARCH_FIELD_FILE_PATH)
				{
					eSearchField = SEARCH_FIELD_PROJECT_PATH;
				}
				continue;
			}
			break;
		case L' ':
		case L'\t':
		case L'\r':
		case L'\n':
			if (!bQuoted)
			{
				bDone = true;
			}
			break;
		}

		if (bDone)
		{
			if (lpFilter != nullptr && *lpFilter != L'\0')
			{
				if (eLogicOperator == LOGIC_OPERATOR_NONE)
				{
					eLogicOperator = LOGIC_OPERATOR_AND;
				}
				if (bNot)
				{
					eLogicOperator = static_cast<ELogicOperator>(eLogicOperator << 1);
				}

				*pChar = L'\0';

				TryParseLineAndColumns(lpFilter, pChar, pDestinationLine, pDestinationColumn);

				filters.emplace_back(lpFilter, eSearchField, eLogicOperator);
			}

			lpFilter = nullptr;
			eSearchField = SEARCH_FIELD_FILE_NAME;
			eLogicOperator = LOGIC_OPERATOR_NONE;
			bNot = false;
			bQuoted = false;
		}
		else if (lpFilter == nullptr)
		{
			lpFilter = pChar;
		}
	}
}