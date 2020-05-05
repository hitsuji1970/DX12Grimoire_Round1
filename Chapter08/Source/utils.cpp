#include "utils.h"

std::wstring GetExtension(const std::wstring& path)
{
	auto pos = path.find_last_of(L'.');
	if (pos == path.npos)
	{
		return std::wstring();
	}
	return path.substr(pos + 1);
}
