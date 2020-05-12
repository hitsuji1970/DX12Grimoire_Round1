#include "PMDMaterial.h"

#include <d3dx12.h>
#include <DirectXTex.h>
#include <functional>
#include "utils.h"

namespace pmd
{
	template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

	ComPtr<ID3D12Resource> PMDMaterial::TheWhiteTexture;
	ComPtr<ID3D12Resource> PMDMaterial::TheBlackTexture;
	ComPtr<ID3D12Resource> PMDMaterial::TheGradTexture;
	std::map<std::wstring, ComPtr<ID3D12Resource>> PMDMaterial::SharedResources;

	/**
	 * コンストラクター
	 */
	PMDMaterial::PMDMaterial() :
		indicesNum(0), basicMaterial(), additionalMaterial(),
		pTextureResource(nullptr), pSPHResource(nullptr), pSPAResource(nullptr),
		pToonResource(nullptr)
	{

	}

	/**
	 * デストラクター
	 */
	PMDMaterial::~PMDMaterial()
	{
	}

	/**
	 * 指定が無い場合に適用する加算・乗算テクスチャーのロード
	 */
	HRESULT PMDMaterial::LoadDefaultTextures(ID3D12Device* pD3D12Device)
	{
		TheWhiteTexture = CreateSingleColorTexture(pD3D12Device, 0xff, 0xff, 0xff, 0xff);
		if (TheWhiteTexture == nullptr) {
			return E_FAIL;
		}
		TheWhiteTexture->SetName(L"White Texture");

		TheBlackTexture = CreateSingleColorTexture(pD3D12Device, 0x00, 0x00, 0x00, 0xff);
		if (TheBlackTexture == nullptr) {
			return E_FAIL;
		}
		TheBlackTexture->SetName(L"Black Texture");

		TheGradTexture = CreateGrayGradationTexture(pD3D12Device);
		if (TheGradTexture == nullptr) {
			return E_FAIL;
		}
		TheGradTexture->SetName(L"Grad Texture");

		return S_OK;
	}

	void PMDMaterial::ReleaseDefaultTextures()
	{
		TheWhiteTexture->Release();
		TheBlackTexture->Release();
		TheGradTexture->Release();
		for (auto res : SharedResources) {
			res.second->Release();
		}
	}

	/**
	 * ファイルから読み込んだシリアライズ済みデータを展開
	 */
	HRESULT PMDMaterial::LoadFromSerializedData(
		ID3D12Device* const pD3D12Device,
		const SerializedMaterialData& serializedData,
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
					pSPHResource = LoadTextureFromFile(pD3D12Device, path);
				}
				else if (ext == L"spa") {
					pSPAResource = LoadTextureFromFile(pD3D12Device, path);
				}
				else {
					pTextureResource = LoadTextureFromFile(pD3D12Device, path);
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
		pToonResource = LoadTextureFromFile(pD3D12Device, toonTexturePath + toonFileName);

		return result;
	}

	void PMDMaterial::CreateTextureBuffers(
		ID3D12Device* pD3D12Device,
		D3D12_SHADER_RESOURCE_VIEW_DESC* const pSRVDesc,
		D3D12_CPU_DESCRIPTOR_HANDLE* const pDescriptorHandle,
		UINT incSize
	) {
		// ディフューズ色テクスチャー
		if (pTextureResource)
		{
			pSRVDesc->Format = pTextureResource->GetDesc().Format;
			pD3D12Device->CreateShaderResourceView(pTextureResource.Get(), pSRVDesc, *pDescriptorHandle);
		}
		else
		{
			pSRVDesc->Format = TheWhiteTexture->GetDesc().Format;
			pD3D12Device->CreateShaderResourceView(TheWhiteTexture.Get(), pSRVDesc, *pDescriptorHandle);
		}
		pDescriptorHandle->ptr += incSize;

		// 乗算スフィアマップテクスチャー
		if (pSPHResource)
		{
			pSRVDesc->Format = pSPHResource->GetDesc().Format;
			pD3D12Device->CreateShaderResourceView(pSPHResource.Get(), pSRVDesc, *pDescriptorHandle);
		}
		else
		{
			pSRVDesc->Format = TheWhiteTexture->GetDesc().Format;
			pD3D12Device->CreateShaderResourceView(TheWhiteTexture.Get(), pSRVDesc, *pDescriptorHandle);
		}
		pDescriptorHandle->ptr += incSize;

		// 加算スフィアマップテクスチャー
		if (pSPAResource)
		{
			pSRVDesc->Format = pSPAResource->GetDesc().Format;
			pD3D12Device->CreateShaderResourceView(pSPAResource.Get(), pSRVDesc, *pDescriptorHandle);
		}
		else
		{
			pSRVDesc->Format = TheBlackTexture->GetDesc().Format;
			pD3D12Device->CreateShaderResourceView(TheBlackTexture.Get(), pSRVDesc, *pDescriptorHandle);
		}
		pDescriptorHandle->ptr += incSize;

		// トゥーンテクスチャー
		if (pToonResource)
		{
			pSRVDesc->Format = pToonResource->GetDesc().Format;
			pD3D12Device->CreateShaderResourceView(pToonResource.Get(), pSRVDesc, *pDescriptorHandle);
		}
		else
		{
			pSRVDesc->Format = TheGradTexture->GetDesc().Format;
			pD3D12Device->CreateShaderResourceView(TheGradTexture.Get(), pSRVDesc, *pDescriptorHandle);
		}
		pDescriptorHandle->ptr += incSize;
	}

	/**
	 * テクスチャーをファイルからロード
	 */
	ID3D12Resource* PMDMaterial::LoadTextureFromFile(ID3D12Device* const pD3D12Device, const std::wstring& filename)
	{
		auto it = SharedResources.find(filename);
		if (it != SharedResources.end()) {
			return it->second.Get();
		}

		using TexMetadata = DirectX::TexMetadata;
		using ScratchImage = DirectX::ScratchImage;

		TexMetadata metadata = {};
		ScratchImage scratchImg = {};
		HRESULT result;

		// テクスチャー読み込み関数テーブル
		using loader_t = std::function<HRESULT(const std::wstring& path, TexMetadata*, ScratchImage&)>;
		std::map<std::wstring, loader_t> loadLambdaTable;
		loadLambdaTable[L"sph"]
			= loadLambdaTable[L"spa"]
			= loadLambdaTable[L"bmp"]
			= loadLambdaTable[L"png"]
			= loadLambdaTable[L"jpg"]
			= [](const std::wstring& path, TexMetadata* meta, ScratchImage& img)
			-> HRESULT
		{
			return LoadFromWICFile(path.c_str(), DirectX::WIC_FLAGS_NONE, meta, img);
		};

		loadLambdaTable[L"tga"]
			= [](const std::wstring& path, TexMetadata* meta, ScratchImage& img)
			-> HRESULT
		{
			return LoadFromTGAFile(path.c_str(), meta, img);
		};

		loadLambdaTable[L"dds"]
			= [](const std::wstring& path, TexMetadata* meta, ScratchImage& img)
			-> HRESULT
		{
			return LoadFromDDSFile(path.c_str(), 0, meta, img);
		};

		// 読み込み
		auto ext = GetExtension(filename);
		result = loadLambdaTable[ext](filename, &metadata, scratchImg);
		if (FAILED(result))
		{
			wprintf(L"load failed. : %s\n", filename.c_str());
			return nullptr;
		}

		auto img = scratchImg.GetImage(0, 0, 0);
		auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0);
		auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			metadata.format, metadata.width, static_cast<UINT>(metadata.height),
			static_cast<UINT16>(metadata.arraySize), static_cast<UINT16>(metadata.mipLevels));

		ID3D12Resource* pTextureResource = nullptr;
		result = pD3D12Device->CreateCommittedResource(
			&heapProp, D3D12_HEAP_FLAG_NONE,
			&resDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr, IID_PPV_ARGS(&pTextureResource)
		);

		if (FAILED(result))
		{
			wprintf(L"failed to create resource : %s\n", filename.c_str());
			return nullptr;
		}

		auto rowPitch = static_cast<UINT>(img->rowPitch);
		auto slicePitch = static_cast<UINT>(img->slicePitch);
		result = pTextureResource->WriteToSubresource(0, nullptr, img->pixels, rowPitch, slicePitch);
		if (FAILED(result))
		{
			wprintf(L"failed to create resource : %s\n", filename.c_str());
			return nullptr;
		}

		SharedResources.emplace(filename, pTextureResource);

		pTextureResource->SetName(filename.c_str());
		return pTextureResource;
	}
}
