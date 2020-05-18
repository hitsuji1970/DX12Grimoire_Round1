#pragma once;

// std
#include <string>
#include <vector>

// 文字列を指定の文字で分割
template<typename T>
std::vector<std::basic_string<T>> Split(std::basic_string<T> str, T separator)
{
	std::vector<std::basic_string<T>> result;
	size_t pos;

	do {
		pos = str.find_first_of(separator);
		auto element = str.substr(0, pos);
		result.push_back(element);
		str = str.substr(pos + 1);
	} while (pos != std::basic_string<T>::npos);

	return result;
}

// std::stringをstd::wstringに変換
std::wstring GetWString(const std::string& srcString);

// null終端文字列をstd::wstringに変換
std::wstring GetWString(const char* const rawString, size_t length);

// 入力ファイル名から拡張子を取得(std::string)
std::string GetExtension(const std::string& path);

// 入力ファイル名から拡張子を取得(std::wstring)
std::wstring GetExtension(const std::wstring& path);

