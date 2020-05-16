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
#include "PMDMesh.h"

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

	// トランスフォーム
	struct Transform
	{
		// XMMATRIX型のメンバーが16バイトアラインメントであるため
		// Transformのnew()演算子をオーバーライドして16バイト境界に確保する
		void* operator new(size_t size);
		DirectX::XMMATRIX world;
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

		void Update();
		void Draw(ID3D12Device* const pD3D12Device, ID3D12GraphicsCommandList* const pCommandList);

	private:
		// シェーダーリソース用テクスチャーの数
		static constexpr size_t NUMBER_OF_TEXTURE = 4;

		// ロードしたファイル名
		std::wstring m_loadedModelPath;

		// シグネチャー情報
		char _pmdSignature[3];

		// ヘッダー情報
		PMDHeader _pmdHeader;

		// 頂点バッファー
		Microsoft::WRL::ComPtr<ID3D12Resource> _vertexBuffer;
		D3D12_VERTEX_BUFFER_VIEW _vertexBufferView;

		// インデックスバッファー
		Microsoft::WRL::ComPtr<ID3D12Resource> _indexBuffer;
		D3D12_INDEX_BUFFER_VIEW _indexBufferView;

		// マテリアルバッファー
		Microsoft::WRL::ComPtr<ID3D12Resource> _materialBuffer;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _materialDescHeap;

		// インデックスとマテリアルを参照して描画の単位となるメッシュ
		std::vector<PMDMesh> _meshes;

		// 変換行列
		Microsoft::WRL::ComPtr<ID3D12Resource> _transformBuff;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _transformDescHeap;
		Transform* _mappedTransform;

		// 動作確認用の回転
		float _angle;

	private:
		HRESULT CreateVertexBuffer(ID3D12Device* const pD3D12Device, const std::vector<unsigned char>& rawVertices);
		HRESULT CreateIndexBuffer(ID3D12Device* const pD3D12Device, const std::vector<unsigned short>& rawIndices);
		HRESULT CreateMaterialBuffers(ID3D12Device* const pD3D12Device, unsigned int numberOfMesh, const std::vector<SerializedMeshData>& serializedMaterials);
	};

} // namespace pmd
