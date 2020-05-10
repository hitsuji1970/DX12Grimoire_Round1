#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl.h>
#include <string>
#include <map>

namespace pmd
{
	// PMDマテリアル構造体
#pragma pack(1)
	struct SerializedMaterialData
	{
		DirectX::XMFLOAT3 diffuse;
		float alpha;
		float specularity;
		DirectX::XMFLOAT3 specular;
		DirectX::XMFLOAT3 ambient;
		unsigned char toonIdx;
		unsigned char edgeFlg;
		// パディング2bytesが入る
		unsigned int indicesNum;
		char texFilePath[20];
	};
#pragma pack()

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

	struct AdditionalMaterial
	{
		std::wstring texPath;
		int toonIdx;
		bool edgeFlg;
	};

	class PMDMaterial
	{
	public:
		PMDMaterial();
		virtual ~PMDMaterial();

		/** 共通のデフォルトテクスチャーをロード */
		static HRESULT LoadDefaultTextures(const Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice);

		/** ファイルから読み込んだシリアライズ済みデータの展開 */
		HRESULT LoadFromSerializedData(
			const Microsoft::WRL::ComPtr<ID3D12Device>& d3dDevice,
			const SerializedMaterialData& serealizedData,
			const std::wstring& folderPath,
			const std::wstring& toonTexturePath);

		/** テクスチャー用バッファーリソースの生成 */
		void CreateTextureBuffers(
			const Microsoft::WRL::ComPtr<ID3D12Device>& d3dDevice,
			D3D12_SHADER_RESOURCE_VIEW_DESC* const pSRVDesc,
			D3D12_CPU_DESCRIPTOR_HANDLE* const pDescriptorHeapHandle,
			UINT incSize);

		unsigned int GetIndicesNum() const
		{
			return indicesNum;
		}

		const BasicMaterial& GetBasicMaterial() const
		{
			return basicMaterial;
		}

	private:
		// 白テクスチャー
		static Microsoft::WRL::ComPtr<ID3D12Resource> TheWhiteTexture;

		// 黒テクスチャー
		static Microsoft::WRL::ComPtr<ID3D12Resource> TheBlackTexture;

		// 白黒のグラデーションテクスチャー
		static Microsoft::WRL::ComPtr<ID3D12Resource> TheGradTexture;

		// 共有リソース
		static std::map<std::wstring, Microsoft::WRL::ComPtr<ID3D12Resource>> SharedResources;

	private:
		UINT indicesNum;
		BasicMaterial basicMaterial;
		AdditionalMaterial additionalMaterial;
		Microsoft::WRL::ComPtr<ID3D12Resource> pTextureResource;
		Microsoft::WRL::ComPtr<ID3D12Resource> pSPHResource;
		Microsoft::WRL::ComPtr<ID3D12Resource> pSPAResource;
		Microsoft::WRL::ComPtr<ID3D12Resource> pToonResource;

	private:
		// テクスチャーをファイルからロード
		ID3D12Resource* LoadTextureFromFile(Microsoft::WRL::ComPtr<ID3D12Device> pD3D12Device, const std::wstring& filename);
	};
}

