#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
//#include <Windows.h>
#include <wrl.h>
#include <string>
//#include <map>

namespace pmd
{
	// PMDマテリアル構造体
#pragma pack(1)
	struct SerializedMaterial
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

	struct BasicMatrial
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

	struct PMDMaterial
	{
		unsigned int indicesNum;
		BasicMatrial basicMaterial;
		AdditionalMaterial additionalMaterial;
		Microsoft::WRL::ComPtr<ID3D12Resource> pTextureResource;
		Microsoft::WRL::ComPtr<ID3D12Resource> pSPHResource;
		Microsoft::WRL::ComPtr<ID3D12Resource> pSPAResource;
		Microsoft::WRL::ComPtr<ID3D12Resource> pToonResource;

		PMDMaterial() :
			indicesNum(0), basicMaterial(), additionalMaterial(),
			pTextureResource(nullptr), pSPHResource(nullptr), pSPAResource(nullptr),
			pToonResource(nullptr)
		{
		}
	};
}

