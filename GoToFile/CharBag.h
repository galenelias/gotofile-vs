/********************************************************************************
	CharBag.h

	Utility class storing a 64-bit integer representation of the characters in a
	string.

	Bits 0-52: 2x26. Count (up to 2) of frequency of each (lowercase'd) letter
	Bits 53-62: Count (up to 2) of frequency of each digit
********************************************************************************/
#pragma once

#include <string_view>

class CharBag
{
public:
	CharBag() = default;

	CharBag(LPCWSTR pwz)
	{
		Initialize(pwz);
	}

	void Initialize(LPCWSTR pwz)
	{
		m_bits = 0;
		while (*pwz != L'\0')
		{
			const wchar_t c = towlower(*pwz++);
			if (c >= L'a' && c <= L'z')
			{
				size_t index = (c - L'a') * 2;
				uint64_t mask = 2ULL << index;
				// Set the bit corresponding to the letter
				m_bits = m_bits
					| (m_bits & mask) << 1 // Shift the existing low part of the count left (1 -> 2, 2 -> 2)
					| (1ULL << index);
			}
			else if (c >= L'0' && c <= L'9')
			{
				// Set the bit corresponding to the digit
				m_bits |= 1ULL << (c - L'0' + 52);
			}
			else if (c == '-')
			{
				m_bits |= 1ULL << 62;
			}
		}
	}

	bool IsNil() const
	{
		return m_bits == 0;
	}

	// Determines if the CharBag contains all the characters in the other CharBag
	// (other would be needle, this would be haystack)
	bool IsSuperset(const CharBag& other) const
	{
		return (m_bits & other.m_bits) == other.m_bits;
	}

private:
	uint64_t m_bits = 0;
};

