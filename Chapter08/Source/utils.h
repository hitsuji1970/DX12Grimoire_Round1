#pragma once;

#include <d3d12.h>
#include <string>
#include <vector>

/**
 * 文字列を指定の文字で分割
 */
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

/**
 * 入力ファイル名から拡張子を取得
 */
std::wstring GetExtension(const std::wstring& path);

/**
 * 単一色のテクスチャーを生成
 */
ID3D12Resource* CreateSingleColorTexture(ID3D12Device* pD3D12Device, UINT8 r, UINT8 g, UINT8 b, UINT8 a);

