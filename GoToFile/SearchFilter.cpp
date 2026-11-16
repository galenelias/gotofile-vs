#include "SearchFilter.h"

#include <cctype>
#include <cstdlib>
#include <cwchar>
#include <optional>
#include <string_view>
#include <tuple>


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
		// Do a first-pass filter based on whether the field we're searching for even contains all the characters in the filter
		if (m_eSearchField != SEARCH_FIELD_FILE_NAME || file.filePathCharBag.IsSuperset(m_charbag))
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


namespace
{
	// The special characters seen in front of a term, before its text starts
	struct STermModifiers
	{
		ESearchField eSearchField = SEARCH_FIELD_FILE_NAME;
		ELogicOperator eLogicOperator = LOGIC_OPERATOR_NONE;
		bool bNot = false;
		bool bQuoted = false;
	};
}


// '\\' or '/': search the path instead of the name
static ESearchField PromoteToPath(ESearchField eSearchField)
{
	switch (eSearchField)
	{
		case SEARCH_FIELD_FILE_NAME:
			return SEARCH_FIELD_FILE_PATH;
		case SEARCH_FIELD_PROJECT_NAME:
			return SEARCH_FIELD_PROJECT_PATH;
		default:
			return eSearchField;
	}
}


// ':': search the project instead of the file
static ESearchField PromoteToProject(ESearchField eSearchField)
{
	switch (eSearchField)
	{
		case SEARCH_FIELD_FILE_NAME:
			return SEARCH_FIELD_PROJECT_NAME;
		case SEARCH_FIELD_FILE_PATH:
			return SEARCH_FIELD_PROJECT_PATH;
		default:
			return eSearchField;
	}
}


// Applies a special character that appears before a term's text. Returns false if the character
// isn't a modifier here, in which case it is the first character of the term.
static bool TryApplyModifier(WCHAR ch, STermModifiers& modifiers)
{
	if (modifiers.bQuoted)
		return false;

	switch (ch)
	{
		case L'&':
		case L'|':
			if (modifiers.eLogicOperator != LOGIC_OPERATOR_NONE)
				return false;
			modifiers.eLogicOperator = (ch == L'&') ? LOGIC_OPERATOR_AND : LOGIC_OPERATOR_OR;
			return true;
		case L'-':
		case L'!':
			if (modifiers.bNot)
				return false;
			modifiers.bNot = true;
			return true;
		case L'"':
			modifiers.bQuoted = true;
			return true;
		case L'\\':
		case L'/':
			modifiers.eSearchField = PromoteToPath(modifiers.eSearchField);
			return true;
		case L':':
			modifiers.eSearchField = PromoteToProject(modifiers.eSearchField);
			return true;
		default:
			return false;
	}
}


// Whether ch ends the current term: a closing quote inside quotes, otherwise whitespace
static bool IsTermEnd(WCHAR ch, bool bQuoted)
{
	if (bQuoted)
		return ch == L'"';

	return ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n';
}


// Terminates the term at lpEnd, strips any line/column suffix, and adds the resulting filter
static void AddFilter(LPWSTR lpTerm, LPWSTR lpEnd, const STermModifiers& modifiers, std::vector<SFilter>& filters, _Inout_ int* pDestinationLine, _Inout_ int* pDestinationColumn)
{
	ELogicOperator eLogicOperator = (modifiers.eLogicOperator == LOGIC_OPERATOR_NONE) ? LOGIC_OPERATOR_AND : modifiers.eLogicOperator;
	if (modifiers.bNot)
		eLogicOperator = static_cast<ELogicOperator>(eLogicOperator << 1); // AND -> AND_NOT, OR -> OR_NOT

	*lpEnd = L'\0';

	TryParseLineAndColumns(lpTerm, lpEnd, pDestinationLine, pDestinationColumn);

	filters.emplace_back(lpTerm, modifiers.eSearchField, eLogicOperator);
}


// Splits the filter text into terms. Each term is an optional run of modifier characters followed by its
// text. The terms are written, null-terminated, into spFilterStringTable, which the SFilters point into.
void CreateFilterList(LPCWSTR pwzFilter, std::unique_ptr<WCHAR[]>& spFilterStringTable, std::vector<SFilter>& filters, _Inout_ int* pDestinationLine, _Inout_ int* pDestinationColumn)
{
	if (pwzFilter == nullptr)
		pwzFilter = L"";

	const size_t cchFilter = wcslen(pwzFilter) + 1;
	spFilterStringTable = std::make_unique<WCHAR[]>(cchFilter);
	wcscpy_s(spFilterStringTable.get(), cchFilter, pwzFilter);

	LPWSTR lpTerm = nullptr; // Start of the current term's text, once past its modifiers
	STermModifiers modifiers;

	for (LPWSTR pChar = spFilterStringTable.get(); ; pChar++)
	{
		const WCHAR ch = *pChar;

		if (ch == L'\0' || IsTermEnd(ch, modifiers.bQuoted))
		{
			if (lpTerm != nullptr)
				AddFilter(lpTerm, pChar, modifiers, filters, pDestinationLine, pDestinationColumn);

			if (ch == L'\0')
				break;

			lpTerm = nullptr;
			modifiers = STermModifiers();
		}
		else if (lpTerm == nullptr && !TryApplyModifier(ch, modifiers))
		{
			lpTerm = pChar;
		}
	}
}
