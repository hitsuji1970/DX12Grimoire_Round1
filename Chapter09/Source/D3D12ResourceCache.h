#pragma once

// std
#include <map>
#include <string>
#include <unordered_map>

// DirectX
#include <d3d12.h>
#include <DirectXTex.h>

// Windows
#include <wrl.h>

class D3D12ResourceCache
{
public:
	D3D12ResourceCache(ID3D12Device* const);
	virtual ~D3D12ResourceCache();

	// 画像ファイルからテクスチャーリソースを生成
	ID3D12Resource* LoadTextureFromFile(const std::wstring& filename);

	// 中身が空のテクスチャーリソースを生成
	ID3D12Resource* CreateEmptyTexture(ID3D12Device* const pD3D12Device, UINT64 width, UINT height);

	// 中身が空のテクスチャーリソースを生成
	ID3D12Resource* CreateEmptyTexture(ID3D12Device* const pD3D12Device, DXGI_FORMAT format, UINT64 width, UINT height);

	// 単一色のテクスチャーを生成
	ID3D12Resource* CreateSingleColorTexture(ID3D12Device* const pD3D12Device, UINT8 r, UINT8 g, UINT8 b, UINT8 a);

	// 白黒のグラデーションテクスチャーを生成
	ID3D12Resource* CreateGrayGradationTexture(ID3D12Device* const pD3D12Device);

	// 白一色のグラデーションテクスチャーを取得
	ID3D12Resource* GetWhiteTexture()
	{
		return _whiteTexture4x4.Get();
	}

	// 黒一色のグラデーションテクスチャーを取得
	ID3D12Resource* GetBlackTexture()
	{
		return _blackTexture4x4.Get();
	}

	// 線型グラデーションテクスチャーを取得
	ID3D12Resource* GetGrayGradationTexture()
	{
		return _grayGradationTexture.Get();
	}

private:
	// DirextXグラフィックスデバイスインターフェイス
	ID3D12Device* const _pDevice;

	// 拡張子別のテクスチャーローダー
	using loader_t = std::function<HRESULT(const std::wstring&, DirectX::TexMetadata* const, DirectX::ScratchImage&)>;
	std::map<std::wstring, loader_t> _textureLoaderTable;

	// ロード済みテクスチャーリソース格納テーブル
	std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D12Resource>> _loadedTextures;

	// 白一色のテクスチャーリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> _whiteTexture4x4;

	// 黒一色のテクスチャーリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> _blackTexture4x4;

	// 線型グラデーションのテクスチャーリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> _grayGradationTexture;
};
