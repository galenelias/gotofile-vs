// Unit tests for the search-filter language (see "Special Characters" in README.md) and wildcard matching.

#include "CppUnitTest.h"

#include "SearchFilter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace
{
	// Owns the string table that the parsed SFilters point into
	struct ParsedFilter
	{
		std::unique_ptr<WCHAR[]> spStringTable;
		std::vector<SFilter> filters;
		int line = -1;
		int column = -1;
	};

	ParsedFilter Parse(LPCWSTR pwzFilter)
	{
		ParsedFilter parsed;
		CreateFilterList(pwzFilter, parsed.spStringTable, parsed.filters, &parsed.line, &parsed.column);
		return parsed;
	}

	void AssertFilter(const SFilter& filter, LPCWSTR pwzExpectedText, ESearchField eExpectedField, ELogicOperator eExpectedOperator)
	{
		Assert::AreEqual(pwzExpectedText, static_cast<LPCWSTR>(filter.m_lpFilter));
		Assert::AreEqual(static_cast<int>(eExpectedField), static_cast<int>(filter.m_eSearchField), L"search field");
		Assert::AreEqual(static_cast<int>(eExpectedOperator), static_cast<int>(filter.m_eLogicOperator), L"logic operator");
	}

	void AssertFilterCount(const ParsedFilter& parsed, int iExpected)
	{
		Assert::AreEqual(iExpected, static_cast<int>(parsed.filters.size()), L"filter count");
	}

	SFile MakeFile(LPCWSTR pwzFilePath, LPCWSTR pwzProjectName, LPCWSTR pwzProjectPath)
	{
		const size_t cch = wcslen(pwzFilePath) + 1;
		std::unique_ptr<WCHAR[]> spFilePath = std::make_unique<WCHAR[]>(cch);
		wcscpy_s(spFilePath.get(), cch, pwzFilePath);
		return SFile(std::move(spFilePath), pwzProjectName, pwzProjectPath);
	}

	// Parses a single-term filter and matches it against a sample file
	int MatchSampleFile(LPCWSTR pwzFilter)
	{
		const SFile file = MakeFile(L"C:\\src\\Foo.cpp", L"MyProject", L"C:\\src\\MyProject.vcxproj");
		ParsedFilter parsed = Parse(pwzFilter);
		AssertFilterCount(parsed, 1);
		return parsed.filters[0].Match(file);
	}
}

TEST_CLASS(CreateFilterListTests)
{
public:
	TEST_METHOD(EmptyInputProducesNoFilters)
	{
		AssertFilterCount(Parse(L""), 0);
		AssertFilterCount(Parse(L" \t\r\n "), 0);
		AssertFilterCount(Parse(nullptr), 0);
	}

	TEST_METHOD(LoneOperatorProducesNoFilters)
	{
		AssertFilterCount(Parse(L"-"), 0);
		AssertFilterCount(Parse(L"| & :"), 0);
	}

	TEST_METHOD(PlainTermIsFileNameAnd)
	{
		ParsedFilter parsed = Parse(L"substring");
		AssertFilterCount(parsed, 1);
		AssertFilter(parsed.filters[0], L"substring", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND);
	}

	TEST_METHOD(TermIsLowercased)
	{
		ParsedFilter parsed = Parse(L"SubString");
		AssertFilterCount(parsed, 1);
		AssertFilter(parsed.filters[0], L"substring", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND);
	}

	TEST_METHOD(WhitespaceSeparatesTerms)
	{
		ParsedFilter parsed = Parse(L"substring  .h\tfoo");
		AssertFilterCount(parsed, 3);
		AssertFilter(parsed.filters[0], L"substring", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND);
		AssertFilter(parsed.filters[1], L".h", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND);
		AssertFilter(parsed.filters[2], L"foo", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND);
	}

	TEST_METHOD(ExplicitAnd)
	{
		ParsedFilter parsed = Parse(L"&foo");
		AssertFilterCount(parsed, 1);
		AssertFilter(parsed.filters[0], L"foo", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND);
	}

	// README: "substring |.cpp |.h"
	TEST_METHOD(BarMeansOr)
	{
		ParsedFilter parsed = Parse(L"substring |.cpp |.h");
		AssertFilterCount(parsed, 3);
		AssertFilter(parsed.filters[0], L"substring", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND);
		AssertFilter(parsed.filters[1], L".cpp", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_OR);
		AssertFilter(parsed.filters[2], L".h", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_OR);
	}

	// README: "substring -.h" and "substring -.cpp -.h"
	TEST_METHOD(HyphenAndExclamationMeanNot)
	{
		ParsedFilter parsed = Parse(L"substring -.cpp !.h");
		AssertFilterCount(parsed, 3);
		AssertFilter(parsed.filters[0], L"substring", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND);
		AssertFilter(parsed.filters[1], L".cpp", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND_NOT);
		AssertFilter(parsed.filters[2], L".h", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND_NOT);
	}

	TEST_METHOD(OrAndNotCombineInEitherOrder)
	{
		ParsedFilter parsed = Parse(L"|-.cpp -|.h");
		AssertFilterCount(parsed, 2);
		AssertFilter(parsed.filters[0], L".cpp", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_OR_NOT);
		AssertFilter(parsed.filters[1], L".h", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_OR_NOT);
	}

	// README: "substring \\include\"
	TEST_METHOD(BackslashSearchesFilePath)
	{
		ParsedFilter parsed = Parse(L"substring \\\\include\\");
		AssertFilterCount(parsed, 2);
		AssertFilter(parsed.filters[1], L"include\\", SEARCH_FIELD_FILE_PATH, LOGIC_OPERATOR_AND);
	}

	TEST_METHOD(ForwardSlashSearchesFilePathAndIsNormalized)
	{
		ParsedFilter parsed = Parse(L"/include/");
		AssertFilterCount(parsed, 1);
		AssertFilter(parsed.filters[0], L"include\\", SEARCH_FIELD_FILE_PATH, LOGIC_OPERATOR_AND);
	}

	// README: ":substring"
	TEST_METHOD(ColonSearchesProjectName)
	{
		ParsedFilter parsed = Parse(L":substring");
		AssertFilterCount(parsed, 1);
		AssertFilter(parsed.filters[0], L"substring", SEARCH_FIELD_PROJECT_NAME, LOGIC_OPERATOR_AND);
	}

	TEST_METHOD(ColonAndSlashSearchProjectPathInEitherOrder)
	{
		ParsedFilter parsed = Parse(L":\\proj \\:proj");
		AssertFilterCount(parsed, 2);
		AssertFilter(parsed.filters[0], L"proj", SEARCH_FIELD_PROJECT_PATH, LOGIC_OPERATOR_AND);
		AssertFilter(parsed.filters[1], L"proj", SEARCH_FIELD_PROJECT_PATH, LOGIC_OPERATOR_AND);
	}

	// README: substring :"header files"
	TEST_METHOD(QuotesKeepSpaces)
	{
		ParsedFilter parsed = Parse(L"substring :\"header files\"");
		AssertFilterCount(parsed, 2);
		AssertFilter(parsed.filters[1], L"header files", SEARCH_FIELD_PROJECT_NAME, LOGIC_OPERATOR_AND);
	}

	TEST_METHOD(QuotesEscapeSpecialCharacters)
	{
		ParsedFilter parsed = Parse(L"\"-foo\"");
		AssertFilterCount(parsed, 1);
		AssertFilter(parsed.filters[0], L"-foo", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND);
	}

	TEST_METHOD(EmptyQuotesProduceNoFilter)
	{
		AssertFilterCount(Parse(L"\"\""), 0);
		AssertFilterCount(Parse(L"- \"\" :"), 0);
	}

	TEST_METHOD(UnterminatedQuoteRunsToEnd)
	{
		ParsedFilter parsed = Parse(L"\"a b");
		AssertFilterCount(parsed, 1);
		AssertFilter(parsed.filters[0], L"a b", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND);
	}

	TEST_METHOD(ClosingQuoteEndsTerm)
	{
		ParsedFilter parsed = Parse(L"\"foo\"bar");
		AssertFilterCount(parsed, 2);
		AssertFilter(parsed.filters[0], L"foo", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND);
		AssertFilter(parsed.filters[1], L"bar", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND);
	}

	TEST_METHOD(QuoteInsideTermIsLiteral)
	{
		ParsedFilter parsed = Parse(L"a\"b");
		AssertFilterCount(parsed, 1);
		AssertFilter(parsed.filters[0], L"a\"b", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND);
	}

	TEST_METHOD(ModifiersApplyBeforeQuote)
	{
		ParsedFilter parsed = Parse(L"|-:\"a b\"");
		AssertFilterCount(parsed, 1);
		AssertFilter(parsed.filters[0], L"a b", SEARCH_FIELD_PROJECT_NAME, LOGIC_OPERATOR_OR_NOT);
	}

	TEST_METHOD(SecondNegationStartsTerm)
	{
		ParsedFilter parsed = Parse(L"--foo");
		AssertFilterCount(parsed, 1);
		AssertFilter(parsed.filters[0], L"-foo", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND_NOT);
	}

	TEST_METHOD(SecondLogicOperatorStartsTerm)
	{
		ParsedFilter parsed = Parse(L"&|foo");
		AssertFilterCount(parsed, 1);
		AssertFilter(parsed.filters[0], L"|foo", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND);
	}

	TEST_METHOD(RepeatedFieldPrefixesAreAbsorbed)
	{
		ParsedFilter parsed = Parse(L"::foo //bar :\\:\\baz");
		AssertFilterCount(parsed, 3);
		AssertFilter(parsed.filters[0], L"foo", SEARCH_FIELD_PROJECT_NAME, LOGIC_OPERATOR_AND);
		AssertFilter(parsed.filters[1], L"bar", SEARCH_FIELD_FILE_PATH, LOGIC_OPERATOR_AND);
		AssertFilter(parsed.filters[2], L"baz", SEARCH_FIELD_PROJECT_PATH, LOGIC_OPERATOR_AND);
	}

	TEST_METHOD(SpecialCharactersInsideTermAreLiteral)
	{
		ParsedFilter parsed = Parse(L"a&b|c-d!e\\f/g");
		AssertFilterCount(parsed, 1);
		AssertFilter(parsed.filters[0], L"a&b|c-d!e\\f/g", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND);
	}

	TEST_METHOD(QuotedLineNumber)
	{
		ParsedFilter parsed = Parse(L"\"foo.cpp:3\"");
		AssertFilterCount(parsed, 1);
		AssertFilter(parsed.filters[0], L"foo.cpp", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND);
		Assert::AreEqual(3, parsed.line);
	}

	// README: "substring *.h"
	TEST_METHOD(WildcardsAreKept)
	{
		ParsedFilter parsed = Parse(L"substring *.h f?o");
		AssertFilterCount(parsed, 3);
		AssertFilter(parsed.filters[1], L"*.h", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND);
		AssertFilter(parsed.filters[2], L"f?o", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND);
	}
};

TEST_CLASS(LineAndColumnTests)
{
public:
	// README: "substring.cpp(row[,col])"
	TEST_METHOD(ParenLineAndColumn)
	{
		ParsedFilter parsed = Parse(L"substring.cpp(32,10)");
		AssertFilterCount(parsed, 1);
		AssertFilter(parsed.filters[0], L"substring.cpp", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND);
		Assert::AreEqual(32, parsed.line);
		Assert::AreEqual(10, parsed.column);
	}

	TEST_METHOD(ParenLineOnly)
	{
		ParsedFilter parsed = Parse(L"substring.cpp(32)");
		AssertFilter(parsed.filters[0], L"substring.cpp", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND);
		Assert::AreEqual(32, parsed.line);
		Assert::AreEqual(-1, parsed.column);
	}

	// README: "substring.cpp:row[:col]"
	TEST_METHOD(ColonLineAndColumn)
	{
		ParsedFilter parsed = Parse(L"substring.cpp:32:10");
		AssertFilterCount(parsed, 1);
		AssertFilter(parsed.filters[0], L"substring.cpp", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND);
		Assert::AreEqual(32, parsed.line);
		Assert::AreEqual(10, parsed.column);
	}

	TEST_METHOD(ColonLineOnly)
	{
		ParsedFilter parsed = Parse(L"foo.cpp:32");
		AssertFilter(parsed.filters[0], L"foo.cpp", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND);
		Assert::AreEqual(32, parsed.line);
		Assert::AreEqual(-1, parsed.column);
	}

	TEST_METHOD(NoLineLeavesDestinationUnchanged)
	{
		ParsedFilter parsed = Parse(L"foo.cpp");
		AssertFilter(parsed.filters[0], L"foo.cpp", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND);
		Assert::AreEqual(-1, parsed.line);
		Assert::AreEqual(-1, parsed.column);
	}

	TEST_METHOD(TrailingColonIsStripped)
	{
		ParsedFilter parsed = Parse(L"foo.cpp:");
		AssertFilter(parsed.filters[0], L"foo.cpp", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND);
		Assert::AreEqual(-1, parsed.line);
	}

	TEST_METHOD(LineZeroIsIgnored)
	{
		ParsedFilter parsed = Parse(L"foo.cpp(0)");
		Assert::AreEqual(-1, parsed.line);
	}

	// The parsers call _wtoi on a pointer into a wstring_view, which relies on the term being
	// null-terminated. CreateFilterList writes that terminator before parsing; pin it down.
	TEST_METHOD(LineNumberStopsAtEndOfTerm)
	{
		ParsedFilter colon = Parse(L"foo.cpp:12 34");
		AssertFilterCount(colon, 2);
		AssertFilter(colon.filters[0], L"foo.cpp", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND);
		AssertFilter(colon.filters[1], L"34", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND);
		Assert::AreEqual(12, colon.line);
		Assert::AreEqual(-1, colon.column);

		ParsedFilter paren = Parse(L"foo.cpp(12,3) 45");
		AssertFilterCount(paren, 2);
		Assert::AreEqual(12, paren.line);
		Assert::AreEqual(3, paren.column);
	}

	TEST_METHOD(LineNumberAfterProjectTerm)
	{
		ParsedFilter parsed = Parse(L":proj foo.cpp:7");
		AssertFilterCount(parsed, 2);
		AssertFilter(parsed.filters[0], L"proj", SEARCH_FIELD_PROJECT_NAME, LOGIC_OPERATOR_AND);
		AssertFilter(parsed.filters[1], L"foo.cpp", SEARCH_FIELD_FILE_NAME, LOGIC_OPERATOR_AND);
		Assert::AreEqual(7, parsed.line);
	}
};

TEST_CLASS(LikeTests)
{
public:
	TEST_METHOD(NormalizeLowercasesAndUnifiesPathSeparators)
	{
		Assert::AreEqual(L'a', SFilter::Normalize(L'A', SEARCH_FIELD_FILE_NAME));
		Assert::AreEqual(L'\\', SFilter::Normalize(L'/', SEARCH_FIELD_FILE_PATH));
		Assert::AreEqual(L'\\', SFilter::Normalize(L'/', SEARCH_FIELD_PROJECT_PATH));
		Assert::AreEqual(L'/', SFilter::Normalize(L'/', SEARCH_FIELD_FILE_NAME));
	}

	TEST_METHOD(LiteralPatternMustMatchWholeString)
	{
		Assert::AreEqual(0, SFilter::Like(L"FOO.H", L"foo.h", SEARCH_FIELD_FILE_NAME));
		Assert::AreEqual(-1, SFilter::Like(L"foo.h", L"foo", SEARCH_FIELD_FILE_NAME));
	}

	TEST_METHOD(StarMatchesAnySuffix)
	{
		Assert::AreEqual(3, SFilter::Like(L"Foo.h", L"*.h", SEARCH_FIELD_FILE_NAME));
		Assert::AreEqual(-1, SFilter::Like(L"Foo.cpp", L"*.h", SEARCH_FIELD_FILE_NAME));
		Assert::AreEqual(0, SFilter::Like(L"foo.h", L"foo*", SEARCH_FIELD_FILE_NAME));
		Assert::AreEqual(0, SFilter::Like(L"", L"*", SEARCH_FIELD_FILE_NAME));
	}

	TEST_METHOD(ConsecutiveStarsCollapse)
	{
		Assert::AreEqual(0, SFilter::Like(L"abc", L"a**c", SEARCH_FIELD_FILE_NAME));
	}

	TEST_METHOD(QuestionMarkMatchesExactlyOneCharacter)
	{
		Assert::AreEqual(0, SFilter::Like(L"foo.h", L"f?o.h", SEARCH_FIELD_FILE_NAME));
		Assert::AreEqual(-1, SFilter::Like(L"fo.h", L"f?o.h", SEARCH_FIELD_FILE_NAME));
		Assert::AreEqual(-1, SFilter::Like(L"", L"?", SEARCH_FIELD_FILE_NAME));
	}

	TEST_METHOD(PathPatternMatchesEitherSeparator)
	{
		Assert::AreEqual(3, SFilter::Like(L"src/include/a.h", L"*\\include\\*", SEARCH_FIELD_FILE_PATH));
		Assert::AreEqual(3, SFilter::Like(L"src\\include\\a.h", L"*\\include\\*", SEARCH_FIELD_FILE_PATH));
	}
};

TEST_CLASS(MatchTests)
{
public:
	TEST_METHOD(SubstringOfFileNameReturnsPosition)
	{
		Assert::AreEqual(0, MatchSampleFile(L"foo"));
		Assert::AreEqual(4, MatchSampleFile(L"cpp"));
		Assert::AreEqual(0, MatchSampleFile(L"FOO.CPP"));
	}

	TEST_METHOD(NonMatchReturnsMinusOne)
	{
		Assert::AreEqual(-1, MatchSampleFile(L"xyz"));
		// All characters are present (passes the CharBag pre-check) but not as a substring
		Assert::AreEqual(-1, MatchSampleFile(L"ofo"));
		// The file name is searched, not the directory
		Assert::AreEqual(-1, MatchSampleFile(L"src"));
	}

	TEST_METHOD(NegatedTermInvertsMatch)
	{
		Assert::AreEqual(-1, MatchSampleFile(L"-foo"));
		Assert::IsTrue(MatchSampleFile(L"-bar") >= 0);
	}

	TEST_METHOD(PathTermSearchesWholePath)
	{
		Assert::AreEqual(3, MatchSampleFile(L"\\src\\"));
		Assert::AreEqual(3, MatchSampleFile(L"/src/"));
	}

	TEST_METHOD(ProjectTermsSearchProject)
	{
		Assert::AreEqual(0, MatchSampleFile(L":myproj"));
		Assert::AreEqual(-1, MatchSampleFile(L":foo"));
		Assert::AreEqual(3, MatchSampleFile(L":\\src\\"));
	}

	TEST_METHOD(WildcardTermUsesLike)
	{
		Assert::AreEqual(3, MatchSampleFile(L"*.cpp"));
		Assert::AreEqual(-1, MatchSampleFile(L"*.h"));
		Assert::AreEqual(0, MatchSampleFile(L"f?o.cpp"));
	}
};
