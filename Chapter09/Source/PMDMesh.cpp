#include "PMDMesh.h"

#include <d3dx12.h>
#include <DirectXTex.h>
#include <functional>
#include "utils.h"

namespace pmd
{
	// コンストラクター
	PMDMesh::PMDMesh() :
		indicesNum(0), basicMaterial(), additionalMaterial(),
		pTextureResource(nullptr), pSPHResource(nullptr), pSPAResource(nullptr),
		pToonResource(nullptr)
	{
	}

	// デストラクター
	PMDMesh::~PMDMesh()
	{
	}

	// ファイルから読み込んだシリアライズ済みデータを展開
	HRESULT PMDMesh::LoadFromSerializedData(
		D3D12ResourceCache* const pResourceCache,
		const SerializedMeshData& serializedData,
		const std::wstring& folderPath,
		const std::wstring& toonTexturePath
	) {

		HRESULT result = S_OK;
		indicesNum = serializedData.indicesNum;
		basicMaterial.diffuse = serializedData.diffuse;
		basicMaterial.alpha = serializedData.alpha;
		basicMaterial.specular = serializedData.specular;
		basicMaterial.specularity = serializedData.specularity;
		basicMaterial.ambient = serializedData.ambient;

		additionalMaterial.toonIdx = serializedData.toonIdx;
		additionalMaterial.edgeFlg = serializedData.edgeFlg;
		auto len = std::strlen(serializedData.texFilePath);
		if (len > 0) {
			std::wstring texPath = GetWString(serializedData.texFilePath, len);
			auto filenames = Split(texPath, L'*');
			for (auto filename : filenames) {
				auto path = folderPath + L'/' + filename;
				auto ext = ::GetExtension(filename);
				if (ext == L"sph") {
					pSPHResource = pResourceCache->LoadTextureFromFile(path);
				}
				else if (ext == L"spa") {
					pSPAResource = pResourceCache->LoadTextureFromFile(path);
				}
				else {
					pTextureResource = pResourceCache->LoadTextureFromFile(path);
				}
			}
#ifdef _DEBUG
			wprintf(L" texture = \"%s\"\n", texPath.c_str());
#endif // _DEBUG
		}
		else {
#ifdef _DEBUG
			wprintf(L"\n");
#endif // _DEBUG
		}

		wchar_t toonFileName[16];
		swprintf_s(toonFileName, L"/toon%02d.bmp", serializedData.toonIdx + 1);
		pToonResource = pResourceCache->LoadTextureFromFile(toonTexturePath + toonFileName);

		return result;
	}

	// マテリアルに適用するテクスチャービューの生成
	void PMDMesh::CreateMaterialTextureViews(
		ID3D12Device* pD3D12Device,
		D3D12ResourceCache* pResourceCache,
		D3D12_CPU_DESCRIPTOR_HANDLE* const pDescriptorHandle
	) {
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;

		auto incSize = pD3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		// ディフューズ色テクスチャー
		if (pTextureResource) {
			srvDesc.Format = pTextureResource->GetDesc().Format;
			pD3D12Device->CreateShaderResourceView(pTextureResource.Get(), &srvDesc, *pDescriptorHandle);
		}
		else {
			auto theWhiteTexture = pResourceCache->GetWhiteTexture();
			srvDesc.Format = theWhiteTexture->GetDesc().Format;
			pD3D12Device->CreateShaderResourceView(theWhiteTexture, &srvDesc, *pDescriptorHandle);
		}
		pDescriptorHandle->ptr += incSize;

		// 乗算スフィアマップテクスチャー
		if (pSPHResource) {
			srvDesc.Format = pSPHResource->GetDesc().Format;
			pD3D12Device->CreateShaderResourceView(pSPHResource.Get(), &srvDesc, *pDescriptorHandle);
		}
		else {
			auto theWhiteTexture = pResourceCache->GetWhiteTexture();
			srvDesc.Format = theWhiteTexture->GetDesc().Format;
			pD3D12Device->CreateShaderResourceView(theWhiteTexture, &srvDesc, *pDescriptorHandle);
		}
		pDescriptorHandle->ptr += incSize;

		// 加算スフィアマップテクスチャー
		if (pSPAResource) {
			srvDesc.Format = pSPAResource->GetDesc().Format;
			pD3D12Device->CreateShaderResourceView(pSPAResource.Get(), &srvDesc, *pDescriptorHandle);
		}
		else {
			auto theBlackTexture = pResourceCache->GetBlackTexture();
			srvDesc.Format = theBlackTexture->GetDesc().Format;
			pD3D12Device->CreateShaderResourceView(theBlackTexture, &srvDesc, *pDescriptorHandle);
		}
		pDescriptorHandle->ptr += incSize;

		// トゥーンテクスチャー
		if (pToonResource) {
			srvDesc.Format = pToonResource->GetDesc().Format;
			pD3D12Device->CreateShaderResourceView(pToonResource.Get(), &srvDesc, *pDescriptorHandle);
		}
		else {
			auto theGradationTexture = pResourceCache->GetGrayGradationTexture();
			srvDesc.Format = theGradationTexture->GetDesc().Format;
			pD3D12Device->CreateShaderResourceView(theGradationTexture, &srvDesc, *pDescriptorHandle);
		}
		pDescriptorHandle->ptr += incSize;
	}
}
