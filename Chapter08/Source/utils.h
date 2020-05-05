#pragma once;

#include <string>
#include <vector>

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

std::wstring GetExtension(const std::wstring& path);
