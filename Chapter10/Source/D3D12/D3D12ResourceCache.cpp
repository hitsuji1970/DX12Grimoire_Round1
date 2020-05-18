#include "D3D12ResourceCache.h"

// DirectX
#include <d3dx12.h>

// user
#include "utils.h"

D3D12ResourceCache::D3D12ResourceCache(ID3D12Device* const pDevice) : _pDevice(pDevice)
{
	using DirectX::TexMetadata;
	using DirectX::ScratchImage;

	// テクスチャーローダーテーブルににラムダを配置
	_textureLoaderTable[L"sph"]
		= _textureLoaderTable[L"spa"]
		= _textureLoaderTable[L"bmp"]
		= _textureLoaderTable[L"png"]
		= _textureLoaderTable[L"jpg"]
		= [](const std::wstring& path, TexMetadata* meta, ScratchImage& img)
		-> HRESULT
	{
		return LoadFromWICFile(path.c_str(), DirectX::WIC_FLAGS_NONE, meta, img);
	};

	_textureLoaderTable[L"tga"]
		= [](const std::wstring& path, TexMetadata* meta, ScratchImage& img)
		-> HRESULT
	{
		return LoadFromTGAFile(path.c_str(), meta, img);
	};

	_textureLoaderTable[L"dds"]
		= [](const std::wstring& path, TexMetadata* meta, ScratchImage& img)
		-> HRESULT
	{
		return LoadFromDDSFile(path.c_str(), 0, meta, img);
	};

	// 白一色
	_whiteTexture4x4 = CreateSingleColorTexture(pDevice, 0xff, 0xff, 0xff, 0xff);
	_whiteTexture4x4->SetName(L"White Texture");

	// 黒一色
	_blackTexture4x4 = CreateSingleColorTexture(pDevice, 0x00, 0x00, 0x00, 0xff);
	_blackTexture4x4->SetName(L"Black Texture");

	// グラデーション
	_grayGradationTexture = CreateGrayGradationTexture(pDevice);
	_grayGradationTexture->SetName(L"Grad Texture");

}

D3D12ResourceCache::~D3D12ResourceCache()
{
}

// 画像ファイルからテクスチャーをロード
ID3D12Resource* D3D12ResourceCache::LoadTextureFromFile(const std::wstring& filename)
{
	auto it = _loadedTextures.find(filename);
	if (it != _loadedTextures.end()) {
		return it->second.Get();
	}

	DirectX::TexMetadata metadata = {};
	DirectX::ScratchImage scratchImg = {};
	HRESULT result;

	// ファイルから読み込み
	auto ext = GetExtension(filename);
	result = _textureLoaderTable[ext](filename, &metadata, scratchImg);
	if (FAILED(result))
	{
		wprintf(L"load failed. : %s\n", filename.c_str());
		return nullptr;
	}

	// リソースのプロパティ定義
	auto img = scratchImg.GetImage(0, 0, 0);
	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0);
	auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		metadata.format, metadata.width, static_cast<UINT>(metadata.height),
		static_cast<UINT16>(metadata.arraySize), static_cast<UINT16>(metadata.mipLevels));

	// リソース作成
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource;
	result = _pDevice->CreateCommittedResource(
		&heapProp, D3D12_HEAP_FLAG_NONE,
		&resDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr, IID_PPV_ARGS(textureResource.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		wprintf(L"failed to create resource : %s\n", filename.c_str());
		return nullptr;
	}

	// ファイル内容をリソースに書き込み
	auto rowPitch = static_cast<UINT>(img->rowPitch);
	auto slicePitch = static_cast<UINT>(img->slicePitch);
	result = textureResource->WriteToSubresource(0, nullptr, img->pixels, rowPitch, slicePitch);
	if (FAILED(result))
	{
		wprintf(L"failed to create resource : %s\n", filename.c_str());
		return nullptr;
	}

	// キャッシュテーブルに追加
	_loadedTextures.emplace(filename, textureResource);
	textureResource->SetName(filename.c_str());

	return textureResource.Get();
}

// 中身が空のテクスチャーリソースを生成
ID3D12Resource* D3D12ResourceCache::CreateEmptyTexture(ID3D12Device* const pD3D12Device, DXGI_FORMAT format, UINT64 width, UINT height)
{
	auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height);
	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0);

	ID3D12Resource* pTextureResource = nullptr;
	auto result = pD3D12Device->CreateCommittedResource(
		&heapProp, D3D12_HEAP_FLAG_NONE,
		&resDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr, IID_PPV_ARGS(&pTextureResource)
	);

	if (FAILED(result))
	{
		return nullptr;
	}

	return pTextureResource;
}

// 中身が空のテクスチャーリソースを生成
ID3D12Resource* D3D12ResourceCache::CreateEmptyTexture(ID3D12Device* const pD3D12Device, UINT64 width, UINT height)
{
	return CreateEmptyTexture(pD3D12Device, DXGI_FORMAT_R8G8B8A8_UNORM, width, height);
}

// 単一色のテクスチャーを生成
ID3D12Resource* D3D12ResourceCache::CreateSingleColorTexture(ID3D12Device* pD3D12Device, UINT8 r, UINT8 g, UINT8 b, UINT8 a)
{
	auto pTextureResource = CreateEmptyTexture(pD3D12Device, 4, 4);
	if (pTextureResource == nullptr)
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
	auto result = pTextureResource->WriteToSubresource(0, nullptr, data.data(), 4 * 4, static_cast<UINT>(data.size()));
	if (FAILED(result))
	{
		pTextureResource->Release();
		return nullptr;
	}

	return pTextureResource;
}

// 白黒のグラデーションテクスチャーを生成
ID3D12Resource* D3D12ResourceCache::CreateGrayGradationTexture(ID3D12Device* const pD3D12Device)
{
	auto pTextureResource = CreateEmptyTexture(pD3D12Device, 4, 4);
	if (pTextureResource == nullptr)
	{
		return nullptr;
	}

	std::vector<unsigned int> data(4 * 256);
	unsigned int c = 0xff;
	for (auto it = data.begin(); it != data.end(); it += 4)
	{
		auto col = (c << 24) | (c << 16) | (c << 8) | c;
		std::fill(it, it + 4, col);
		--c;
	}
	auto result = pTextureResource->WriteToSubresource(
		0, nullptr, data.data(), 4 * sizeof(unsigned int),
		sizeof(unsigned int) * static_cast<UINT>(data.size()));
	if (FAILED(result))
	{
		pTextureResource->Release();
		return nullptr;
	}

	return pTextureResource;
}


