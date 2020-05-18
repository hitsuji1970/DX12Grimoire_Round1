#include "utils.h"

/**
 * std::stringをstd::wstringに変換
 */
std::wstring GetWString(const std::string& srcString)
{
	std::wstring dstString;
	for (size_t i = 0; i < srcString.length(); i++)
	{
		wchar_t w;
		mbtowc(&w, &srcString.at(i), 1);
		dstString.push_back(w);
	}
	return dstString;
}

/**
 * null終端文字列をstd::wstringに変換
 */
std::wstring GetWString(const char* const rawString, size_t length)
{
	std::wstring dstString;
	for (size_t i = 0; i < length; i++) {
		wchar_t w;
		mbtowc(&w, &rawString[i], 1);
		dstString.push_back(w);
	}
	return dstString;
}

/**
 * 入力ファイル名から拡張子を取得
 */
std::string GetExtension(const std::string& path)
{
	auto pos = path.find_last_of('.');
	if (pos == path.npos)
	{
		return std::string();
	}
	return path.substr(pos + 1);
}

/**
 * 入力ファイル名から拡張子を取得
 */
std::wstring GetExtension(const std::wstring& path)
{
	auto pos = path.find_last_of(L'.');
	if (pos == path.npos)
	{
		return std::wstring();
	}
	return path.substr(pos + 1);
}

