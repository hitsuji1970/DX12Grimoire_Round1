#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl.h>
#include <string>
#include <map>

#include "D3D12ResourceCache.h"

namespace pmd
{
	// 描画単位となるメッシュのロード用構造体
#pragma pack(1)
	struct SerializedMeshData
	{
		DirectX::XMFLOAT3 diffuse;
		float alpha;
		float specularity;
		DirectX::XMFLOAT3 specular;
		DirectX::XMFLOAT3 ambient;
		unsigned char toonIdx;
		unsigned char edgeFlg;
		// 本来はここにパディング2bytesが入る
		unsigned int indicesNum;
		char texFilePath[20];
	};
#pragma pack()

	// マテリアル情報構造体
	struct BasicMaterial
	{
		// ディフューズ成分
		DirectX::XMFLOAT3 diffuse;
		float alpha;

		// スペキュラー成分
		DirectX::XMFLOAT3 specular;
		float specularity;

		// アンビエントカラー
		DirectX::XMFLOAT3 ambient;
	};

	// 追加のマテリアル情報構造体
	struct AdditionalMaterial
	{
		std::wstring texPath;
		int toonIdx;
		bool edgeFlg;
	};

	// 描画メッシュ単位のデータクラス
	// インデックス範囲とマテリアルの値を持つ
	class PMDMesh
	{
	public:
		PMDMesh();
		virtual ~PMDMesh();

		// ファイルから読み込んだシリアライズ済みデータの展開
		HRESULT LoadFromSerializedData(
			D3D12ResourceCache* const pResourceCache,
			const SerializedMeshData& serealizedData,
			const std::wstring& folderPath,
			const std::wstring& toonTexturePath);

		// マテリアルに適用するテクスチャーリソースの生成
		void CreateMaterialTextureViews(
			ID3D12Device* const pD3D12Device,
			D3D12ResourceCache* const pResourceCache,
			D3D12_CPU_DESCRIPTOR_HANDLE* const pDescriptorHeapHandle);

		// 描画命令の発効時に参照するインデックス数
		unsigned int GetIndicesNum() const
		{
			return indicesNum;
		}

		// マテリアル情報の取得
		const BasicMaterial& GetBasicMaterial() const
		{
			return basicMaterial;
		}

	private:
		UINT indicesNum;
		BasicMaterial basicMaterial;
		AdditionalMaterial additionalMaterial;
		Microsoft::WRL::ComPtr<ID3D12Resource> pTextureResource;
		Microsoft::WRL::ComPtr<ID3D12Resource> pSPHResource;
		Microsoft::WRL::ComPtr<ID3D12Resource> pSPAResource;
		Microsoft::WRL::ComPtr<ID3D12Resource> pToonResource;
	};
}

