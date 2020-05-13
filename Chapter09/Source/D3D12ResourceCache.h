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

private:
	// DirextXグラフィックスデバイスインターフェイス
	ID3D12Device* const _pDevice;

	// 拡張子別のテクスチャーローダー
	using loader_t = std::function<HRESULT(const std::wstring&, DirectX::TexMetadata* const, DirectX::ScratchImage&)>;
	std::map<std::wstring, loader_t> _textureLoaderTable;

	// ロード済みテクスチャーリソース格納テーブル
	std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D12Resource>> _loadedTextures;
};
