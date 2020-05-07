#include "utils.h"

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
 * 単一色のテクスチャーを生成
 */
ID3D12Resource* CreateSingleColorTexture(ID3D12Device* pD3D12Device, UINT8 r, UINT8 g, UINT8 b, UINT8 a)
{
	D3D12_HEAP_PROPERTIES texHeapProp = {};

	texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
	texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	texHeapProp.CreationNodeMask = 0;
	texHeapProp.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	resDesc.Width = 4;
	resDesc.Height = 4;
	resDesc.DepthOrArraySize = 1;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.MipLevels = 1;
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Resource* whiteTextureResource = nullptr;
	auto result = pD3D12Device->CreateCommittedResource(
		&texHeapProp, D3D12_HEAP_FLAG_NONE,
		&resDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr, IID_PPV_ARGS(&whiteTextureResource)
	);

	if (FAILED(result))
	{
		return nullptr;
	}

	std::vector<unsigned char> data(4 * 4 * 4);
	for (size_t i = 0; i < 4 * 4; i++) {
		data[i * 4 + 0] = r;
		data[i * 4 + 1] = g;
		data[i * 4 + 2] = b;
		data[i * 4 + 3] = a;
	}
	result = whiteTextureResource->WriteToSubresource(0, nullptr, data.data(), 4 * 4, static_cast<UINT>(data.size()));

	return whiteTextureResource;
}


