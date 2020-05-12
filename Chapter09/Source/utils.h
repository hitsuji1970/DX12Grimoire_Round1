#pragma once;

// std
#include <string>
#include <vector>

// Windows
#include <tchar.h>
#include <wrl.h>

// DirectX
#include <d3d12.h>

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
 * std::stringをstd::wstringに変換
 */
std::wstring GetWString(const std::string& srcString);

/**
 * null終端文字列をstd::wstringに変換
 */
std::wstring GetWString(const char* const rawString, size_t length);

/**
 * 入力ファイル名から拡張子を取得(std::string)
 */
std::string GetExtension(const std::string& path);

/**
 * 入力ファイル名から拡張子を取得(std::wstring)
 */
std::wstring GetExtension(const std::wstring& path);

/**
 * 中身が空のテクスチャーリソースを生成
 */
ID3D12Resource* CreateEmptyTexture(ID3D12Device* const pD3D12Device, UINT64 width, UINT height);

/**
 * 中身が空のテクスチャーリソースを生成
 */
ID3D12Resource* CreateEmptyTexture(ID3D12Device* const pD3D12Device, DXGI_FORMAT format, UINT64 width, UINT height);

/**
 * 単一色のテクスチャーを生成
 */
ID3D12Resource* CreateSingleColorTexture(ID3D12Device* const pD3D12Device, UINT8 r, UINT8 g, UINT8 b, UINT8 a);

/**
 * 白黒のグラデーションテクスチャーを生成
 */
ID3D12Resource* CreateGrayGradationTexture(ID3D12Device* const pD3D12Device);

/**
 * デバッグ出力
 */
void DebugOutputFromString(const char* format, ...);
