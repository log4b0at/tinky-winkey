#pragma once
#include <string>
#include <codecvt>

namespace utf8
{
	std::string narrow(const wchar_t *ucs2)
	{
		std::wstring_convert<std::codecvt_utf8<wchar_t>> strCnv;
		return strCnv.to_bytes(ucs2);
	}

	std::wstring widen(const char *ascii)
	{
		std::wstring_convert<std::codecvt_utf8<wchar_t>> strCnv;
		return strCnv.from_bytes(ascii);
	}
}