#pragma once

#include "stdafx.h"

enum ESearchField
{
	SEARCH_FIELD_FILE_NAME		= 0,
	SEARCH_FIELD_FILE_PATH		= 4,
	SEARCH_FIELD_PROJECT_NAME	= 8,
	SEARCH_FIELD_PROJECT_PATH	= 12
};

enum ELogicOperator
{
	LOGIC_OPERATOR_NONE		= 0x00,
	LOGIC_OPERATOR_AND		= 0x01,
	LOGIC_OPERATOR_AND_NOT	= 0x02,
	LOGIC_OPERATOR_OR		= 0x04,
	LOGIC_OPERATOR_OR_NOT	= 0x08,
	LOGIC_OPERATOR_NOT_MASK	= LOGIC_OPERATOR_AND_NOT | LOGIC_OPERATOR_OR_NOT
};

enum EFilterTerm
{
	FILTER_TERM_NONE					= 0x00,
	FILTER_TERM_FILE_NAME_AND			= LOGIC_OPERATOR_AND << SEARCH_FIELD_FILE_NAME,
	FILTER_TERM_FILE_NAME_AND_NOT		= LOGIC_OPERATOR_AND_NOT << SEARCH_FIELD_FILE_NAME,
	FILTER_TERM_FILE_NAME_OR			= LOGIC_OPERATOR_OR << SEARCH_FIELD_FILE_NAME,
	FILTER_TERM_FILE_NAME_OR_NOT		= LOGIC_OPERATOR_OR_NOT << SEARCH_FIELD_FILE_NAME,
	FILTER_TERM_FILE_PATH_AND			= LOGIC_OPERATOR_AND << SEARCH_FIELD_FILE_PATH,
	FILTER_TERM_FILE_PATH_AND_NOT		= LOGIC_OPERATOR_AND_NOT << SEARCH_FIELD_FILE_PATH,
	FILTER_TERM_FILE_PATH_OR			= LOGIC_OPERATOR_OR << SEARCH_FIELD_FILE_PATH,
	FILTER_TERM_FILE_PATH_OR_NOT		= LOGIC_OPERATOR_OR_NOT << SEARCH_FIELD_FILE_PATH,
	FILTER_TERM_PROJECT_NAME_AND		= LOGIC_OPERATOR_AND << SEARCH_FIELD_PROJECT_NAME,
	FILTER_TERM_PROJECT_NAME_AND_NOT	= LOGIC_OPERATOR_AND_NOT << SEARCH_FIELD_PROJECT_NAME,
	FILTER_TERM_PROJECT_NAME_OR			= LOGIC_OPERATOR_OR << SEARCH_FIELD_PROJECT_NAME,
	FILTER_TERM_PROJECT_NAME_OR_NOT		= LOGIC_OPERATOR_OR_NOT << SEARCH_FIELD_PROJECT_NAME,
	FILTER_TERM_PROJECT_PATH_AND		= LOGIC_OPERATOR_AND << SEARCH_FIELD_PROJECT_PATH,
	FILTER_TERM_PROJECT_PATH_AND_NOT	= LOGIC_OPERATOR_AND_NOT << SEARCH_FIELD_PROJECT_PATH,
	FILTER_TERM_PROJECT_PATH_OR			= LOGIC_OPERATOR_OR << SEARCH_FIELD_PROJECT_PATH,
	FILTER_TERM_PROJECT_PATH_OR_NOT		= LOGIC_OPERATOR_OR_NOT << SEARCH_FIELD_PROJECT_PATH,
	FILTER_TERM_AND_MASK				= FILTER_TERM_FILE_NAME_AND | FILTER_TERM_FILE_NAME_AND_NOT | FILTER_TERM_FILE_PATH_AND | FILTER_TERM_FILE_PATH_AND_NOT | FILTER_TERM_PROJECT_NAME_AND | FILTER_TERM_PROJECT_NAME_AND_NOT | FILTER_TERM_PROJECT_PATH_AND | FILTER_TERM_PROJECT_PATH_AND_NOT,
	FILTER_TERM_OR_MASK					= FILTER_TERM_FILE_NAME_OR | FILTER_TERM_FILE_NAME_OR_NOT | FILTER_TERM_FILE_PATH_OR | FILTER_TERM_FILE_PATH_OR_NOT | FILTER_TERM_PROJECT_NAME_OR | FILTER_TERM_PROJECT_NAME_OR_NOT | FILTER_TERM_PROJECT_PATH_OR | FILTER_TERM_PROJECT_PATH_OR_NOT
};

struct SFile
{
	SFile(std::unique_ptr<WCHAR[]>&& spFilePathArg, LPCWSTR lpProjectName, LPCWSTR lpProjectPath)
		: spFilePath(std::move(spFilePathArg)), uiFileName(0), lpProjectName(lpProjectName), lpProjectPath(lpProjectPath)
	{
		LPWSTR lpFileName = std::max<LPWSTR>(wcsrchr(spFilePath.get(), L'\\'), wcsrchr(spFilePath.get(), L'/'));
		if (lpFileName)
		{
			uiFileName = static_cast<unsigned short>(lpFileName - spFilePath.get()) + 1;
		}
	}

	SFile(std::unique_ptr<WCHAR[]>&& spFilePath, unsigned short uiFileName, LPCWSTR lpProjectName, LPCWSTR lpProjectPath)
		: spFilePath(std::move(spFilePath)), uiFileName(uiFileName), lpProjectName(lpProjectName), lpProjectPath(lpProjectPath)
	{
	}

	std::unique_ptr<WCHAR[]> spFilePath;
	unsigned short uiFileName;
	LPCWSTR lpProjectName;
	LPCWSTR lpProjectPath;
};

struct SFilteredFile
{
	SFilteredFile(const SFile* pFile, int iMatch)
		: pFile(pFile), iMatch(iMatch)
	{
	}

	const SFile* pFile;
	int iMatch;
};

struct SFilter
{
public:
	SFilter(LPWSTR lpFilter, ESearchField eSearchField, ELogicOperator eLogicOperator)
		: m_lpFilter(lpFilter)
		, m_eSearchField(eSearchField)
		, m_eLogicOperator(eLogicOperator)
		, m_bWildcard(false)
	{
		while (*lpFilter)
		{
			*lpFilter = Normalize(*lpFilter, eSearchField);
			if (*lpFilter == L'*' || *lpFilter == L'?')
			{
				m_bWildcard = true;
			}
			lpFilter++;
		}
	}

	EFilterTerm GetFilterTerm() const
	{
		return static_cast<EFilterTerm>(m_eLogicOperator << m_eSearchField);
	}

	static WCHAR Normalize(WCHAR cChar, ESearchField eSearchField);
	static int Like(LPCWSTR lpSearch, LPCWSTR lpFilter, ESearchField eSearchField);
	int Match(const SFile& File) const;

	LPWSTR m_lpFilter;
	ESearchField m_eSearchField;
	ELogicOperator m_eLogicOperator;

private:
	bool m_bWildcard;
};


void CreateFilterList(const CComBSTR& spFilter, std::unique_ptr<WCHAR[]>& spFilterStringTable, std::vector<SFilter>& Filters, _Inout_ int* destinationLine, _Inout_ int* destinationColumn);

