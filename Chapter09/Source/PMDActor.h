#pragma once;

// std
#include <map>
#include <string>
#include <vector>

// Windows
#include <Windows.h>
#include <wrl.h>

// DirectX
#include <d3d12.h>
#include <DirectXMath.h>

#include "D3D12ResourceCache.h"
#include "PMDMaterial.h"

namespace pmd
{
	// PMDヘッダー構造体
	struct PMDHeader {
		float version;			// 例 : 00 00 80 3F == 1.00
		char model_name[20];	// モデル名
		char comment[256];		// モデルコメント
	};

	// PMD頂点構造体
	struct Vertex
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT2 uv;
		unsigned short boneNo[2];
		unsigned char boneWeight;
		unsigned char endflg;
	};

	class PMDActor
	{
	public:
		// 頂点データのサイズ
		static constexpr size_t VERTEX_SIZE = 38;

		// 頂点レイアウト
		static const std::vector<D3D12_INPUT_ELEMENT_DESC> InputLayout;

		PMDActor();
		virtual ~PMDActor();

		HRESULT LoadFromFile(
			ID3D12Device* const pD3D12Device,
			D3D12ResourceCache* const resourceCache,
			const std::wstring& filename,
			const std::wstring& toonTexturePath);

		void Draw(ID3D12Device* const pD3D12Device, ID3D12GraphicsCommandList* const pCommandList);

	private:
		// シェーダーリソース用テクスチャーの数
		static constexpr size_t NUMBER_OF_TEXTURE = 4;

		// ロードしたファイル名
		std::wstring m_loadedModelPath;

		// シグネチャー情報
		char m_signature[3];

		// ヘッダー情報
		PMDHeader m_header;

		// 頂点バッファー
		Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
		D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

		// インデックスバッファー
		Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
		D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

		// マテリアルバッファー
		Microsoft::WRL::ComPtr<ID3D12Resource> m_materialBuffer;

		// マテリアル用ディスクリプターヒープ
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_materialDescHeap;

		// マテリアル実体
		std::vector<PMDMaterial> m_materials;

	private:
		HRESULT CreateVertexBuffer(ID3D12Device* const pD3D12Device, const std::vector<unsigned char>& rawVertices);
		HRESULT CreateIndexBuffer(ID3D12Device* const pD3D12Device, const std::vector<unsigned short>& rawIndices);
		HRESULT CreateMaterialBuffers(ID3D12Device* const pD3D12Device, unsigned int numberOfMaterial, const std::vector<SerializedMaterialData>& serializedMaterials);
	};

} // namespace pmd
