// std
#include <algorithm>
#include <iostream>
#include <functional>
#include <map>
#include <vector>

// Windows
#include <Windows.h>
#include <tchar.h>

// DirectX
#include <d3dx12.h>
#include <DirectXTex.h>

#include "pmd.h"
#include "utils.h"

namespace pmd
{
	// PMD頂点レイアウト
	const std::vector<D3D12_INPUT_ELEMENT_DESC> PMDMesh::INPUT_LAYOUT = {
		{ // 座標
			"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{ // 法線
			"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{ // UV
			"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"BONE_NO", 0, DXGI_FORMAT_R16G16_UINT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"WEIGHT", 0, DXGI_FORMAT_R8_UINT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"EDGE_FLAG", 0, DXGI_FORMAT_R8_UINT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
	};

	/**
	 * コンストラクター
	 */
	PMDMesh::PMDMesh() :
		m_signature{}, m_header(),
		m_numberOfVertex(0), m_pVertexBuffer(nullptr), m_vertexBufferView{},
		m_numberOfIndex(0), m_pIndexBuffer(nullptr), m_indexBufferView{},
		m_numberOfMaterial(0), m_pMaterialBuffer(nullptr),
		m_pMaterialDescHeap(nullptr), m_materials{},
		m_pWhiteTexture(nullptr), m_pBlackTexture(nullptr)
	{
	}

	/**
	 * デストラクター
	 */
	PMDMesh::~PMDMesh()
	{
		ClearResources();
	}

	/**
	 * PMDファイルからの読み込み
	 */
	HRESULT PMDMesh::LoadFromFile(ID3D12Device* const pD3D12Device, const std::wstring& filename, const std::wstring& toonTexturePath)
	{
		HRESULT result;
		FILE* fp = nullptr;

		auto state = _wfopen_s(&fp, filename.c_str(), TEXT("rb"));
		if (state != 0) {
			return E_FAIL;
		}

		auto pathIndex = filename.rfind('/');
		if (pathIndex == filename.npos) {
			pathIndex = filename.rfind('\\');
		}
		auto folderPath = filename.substr(0, pathIndex);

		// シグネチャーとヘッダー情報
		std::fread(m_signature, sizeof(m_signature), 1, fp);
		std::fread(&m_header, sizeof(m_header), 1, fp);

		// 頂点データの読み込みと頂点バッファーの生成
		std::vector<unsigned char> rawVertices;
		std::fread(&m_numberOfVertex, sizeof(m_numberOfVertex), 1, fp);
		rawVertices.resize(m_numberOfVertex * VERTEX_SIZE);
		std::fread(rawVertices.data(), rawVertices.size(), 1, fp);
		result = pD3D12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(rawVertices.size() * sizeof(rawVertices[0])), D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr, IID_PPV_ARGS(&m_pVertexBuffer)
		);
		if (FAILED(result)) {
			std::fclose(fp);
			ClearResources();
			return result;
		}

		unsigned char* mappedVertex = nullptr;
		result = m_pVertexBuffer->Map(0, nullptr, (void**)&mappedVertex);
		if (FAILED(result)) {
			std::fclose(fp);
			ClearResources();
			return result;
		}
		std::copy(std::begin(rawVertices), std::end(rawVertices), mappedVertex);
		m_pVertexBuffer->Unmap(0, nullptr);
		mappedVertex = nullptr;

		m_vertexBufferView.BufferLocation = m_pVertexBuffer->GetGPUVirtualAddress();
		m_vertexBufferView.SizeInBytes = static_cast<UINT>(rawVertices.size());
		m_vertexBufferView.StrideInBytes = VERTEX_SIZE;

		// インデックスデータの読み込みとインデックスバッファーの生成
		std::vector<unsigned short> rawIndices;
		std::fread(&m_numberOfIndex, sizeof(m_numberOfIndex), 1, fp);
		rawIndices.resize(m_numberOfIndex);
		fread(rawIndices.data(), rawIndices.size() * sizeof(rawIndices[0]), 1, fp);
		result = pD3D12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(rawIndices.size() * sizeof(rawIndices[0])), D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr, IID_PPV_ARGS(&m_pIndexBuffer)
		);
		if (FAILED(result)) {
			std::fclose(fp);
			ClearResources();
			return result;
		}
		unsigned short* mappedIndex = nullptr;
		result = m_pIndexBuffer->Map(0, nullptr, (void**)&mappedIndex);
		if (FAILED(result)) {
			std::fclose(fp);
			ClearResources();
			return result;
		}
		std::copy(std::begin(rawIndices), std::end(rawIndices), mappedIndex);
		m_pIndexBuffer->Unmap(0, nullptr);
		mappedIndex = nullptr;

		m_indexBufferView.BufferLocation = m_pIndexBuffer->GetGPUVirtualAddress();
		m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
		m_indexBufferView.SizeInBytes = static_cast<UINT>(rawIndices.size() * sizeof(rawIndices[0]));

		// マテリアルの読み込み
		fread(&m_numberOfMaterial, sizeof(m_numberOfMaterial), 1, fp);
		std::vector<SerializedMaterial> serializedMaterials(m_numberOfMaterial);
		fread(serializedMaterials.data(), serializedMaterials.size() * sizeof(SerializedMaterial), 1, fp);

		m_materials.resize(m_numberOfMaterial);
		for (int i = 0; i < serializedMaterials.size(); i++) {
			m_materials[i].indicesNum = serializedMaterials[i].indicesNum;
			m_materials[i].basicMaterial.diffuse = serializedMaterials[i].diffuse;
			m_materials[i].basicMaterial.alpha = serializedMaterials[i].alpha;
			m_materials[i].basicMaterial.specular = serializedMaterials[i].specular;
			m_materials[i].basicMaterial.specularity = serializedMaterials[i].specularity;
			m_materials[i].basicMaterial.ambient = serializedMaterials[i].ambient;

			m_materials[i].additionalMaterial.toonIdx = serializedMaterials[i].toonIdx;
			m_materials[i].additionalMaterial.edgeFlg = serializedMaterials[i].edgeFlg;
			auto len = std::strlen(serializedMaterials[i].texFilePath);
			if (len > 0) {
				std::wstring texPath = GetWString(serializedMaterials[i].texFilePath, len);
				auto filenames = Split(texPath, L'*');
				for (auto filename : filenames) {
					auto path = folderPath + L'/' + filename;
					auto ext = ::GetExtension(filename);
					if (ext == L"sph") {
						m_materials[i].pSPHResource = LoadTextureFromFile(pD3D12Device, path);
					}
					else if (ext == L"spa") {
						m_materials[i].pSPAResource = LoadTextureFromFile(pD3D12Device, path);
					}
					else {
						m_materials[i].pTextureResource = LoadTextureFromFile(pD3D12Device, path);
					}
#ifdef _DEBUG
					wprintf(L"material[%d]: texture=\"%s\"\n", i, filename.c_str());
#endif // _DEBUG
				}
			}

			char toonFileName[16];
			sprintf_s(toonFileName, "/toon%02d.bmp", serializedMaterials[i].toonIdx + 1);
			//printf("toon idx: %d\n", serializedMaterials[i].toonIdx);
			m_materials[i].pToonResource = LoadTextureFromFile(pD3D12Device, toonTexturePath + GetWString(toonFileName));
		}

		auto materialBufferSize = sizeof(BasicMatrial);
		materialBufferSize = (materialBufferSize + 0xff) & ~0xff;
		result = pD3D12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(materialBufferSize * m_numberOfMaterial),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr, IID_PPV_ARGS(&m_pMaterialBuffer)
		);
		if (FAILED(result)) {
			std::fclose(fp);
			ClearResources();
			return result;
		}

		unsigned char* pMappedMaterial = nullptr;
		result = m_pMaterialBuffer->Map(0, nullptr, (void**)&pMappedMaterial);
		for (auto& material : m_materials) {
			*reinterpret_cast<BasicMatrial*>(pMappedMaterial) = material.basicMaterial;
			pMappedMaterial += materialBufferSize;
		}

		D3D12_DESCRIPTOR_HEAP_DESC matDescHeapDesc = {};
		matDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		matDescHeapDesc.NodeMask = 0;
		matDescHeapDesc.NumDescriptors = m_numberOfMaterial * (1 + NUMBER_OF_TEXTURE);
		matDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		result = pD3D12Device->CreateDescriptorHeap(&matDescHeapDesc, IID_PPV_ARGS(&m_pMaterialDescHeap));
		if (FAILED(result)) {
			std::fclose(fp);
			ClearResources();
			return result;
		}

		D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc = {};
		matCBVDesc.BufferLocation = m_pMaterialBuffer->GetGPUVirtualAddress();
		matCBVDesc.SizeInBytes = static_cast<UINT>(materialBufferSize);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;

		m_pWhiteTexture = CreateSingleColorTexture(pD3D12Device, 0xff, 0xff, 0xff, 0xff);
		m_pBlackTexture = CreateSingleColorTexture(pD3D12Device, 0x00, 0x00, 0x00, 0xff);
		m_pGradTexture = CreateGrayGradationTexture(pD3D12Device);

		auto matDescHeapH = m_pMaterialDescHeap->GetCPUDescriptorHandleForHeapStart();
		auto incSize = pD3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		for (auto i = 0u; i < m_numberOfMaterial; i++) {
			pD3D12Device->CreateConstantBufferView(&matCBVDesc, matDescHeapH);
			matDescHeapH.ptr += incSize;

			// ディフューズ色テクスチャー
			matCBVDesc.BufferLocation += materialBufferSize;
			if (m_materials[i].pTextureResource)
			{
				srvDesc.Format = m_materials[i].pTextureResource->GetDesc().Format;
				pD3D12Device->CreateShaderResourceView(m_materials[i].pTextureResource, &srvDesc, matDescHeapH);
			}
			else
			{
				srvDesc.Format = m_pWhiteTexture->GetDesc().Format;
				pD3D12Device->CreateShaderResourceView(m_pWhiteTexture, &srvDesc, matDescHeapH);
			}
			matDescHeapH.ptr += incSize;

			// 乗算スフィアマップテクスチャー
			if (m_materials[i].pSPHResource)
			{
				srvDesc.Format = m_materials[i].pSPHResource->GetDesc().Format;
				pD3D12Device->CreateShaderResourceView(m_materials[i].pSPHResource, &srvDesc, matDescHeapH);
			}
			else
			{
				srvDesc.Format = m_pWhiteTexture->GetDesc().Format;
				pD3D12Device->CreateShaderResourceView(m_pWhiteTexture, &srvDesc, matDescHeapH);
			}
			matDescHeapH.ptr += incSize;

			// 加算スフィアマップテクスチャー
			if (m_materials[i].pSPAResource)
			{
				srvDesc.Format = m_materials[i].pSPAResource->GetDesc().Format;
				pD3D12Device->CreateShaderResourceView(m_materials[i].pSPAResource, &srvDesc, matDescHeapH);
			}
			else
			{
				srvDesc.Format = m_pBlackTexture->GetDesc().Format;
				pD3D12Device->CreateShaderResourceView(m_pBlackTexture, &srvDesc, matDescHeapH);
			}
			matDescHeapH.ptr += incSize;

			// トゥーンテクスチャー
			if (m_materials[i].pToonResource)
			{
				srvDesc.Format = m_materials[i].pToonResource->GetDesc().Format;
				pD3D12Device->CreateShaderResourceView(m_materials[i].pToonResource, &srvDesc, matDescHeapH);
			}
			else
			{
				srvDesc.Format = m_pGradTexture->GetDesc().Format;
				pD3D12Device->CreateShaderResourceView(m_pGradTexture, &srvDesc, matDescHeapH);
			}
			matDescHeapH.ptr += incSize;
		}

		std::fclose(fp);
		m_loadedModelPath = filename;
		return S_OK;
	}

	/**
	 * テクスチャーをファイルからロード
	 */
	ID3D12Resource* PMDMesh::LoadTextureFromFile(ID3D12Device* const pD3D12Device, const std::wstring& filename)
	{
		auto it = m_SharedResources.find(filename);
		if (it != m_SharedResources.end()) {
			return it->second;
		}

		DirectX::TexMetadata metadata = {};
		DirectX::ScratchImage scratchImg = {};
		HRESULT result;

		// テクスチャー読み込み関数テーブル
		using LoadLambda_t = std::function<HRESULT(const std::wstring& path, DirectX::TexMetadata*, DirectX::ScratchImage&)>;
		std::map<std::wstring, LoadLambda_t> loadLambdaTable;
		loadLambdaTable[L"sph"]
			= loadLambdaTable[L"spa"]
			= loadLambdaTable[L"bmp"]
			= loadLambdaTable[L"png"]
			= loadLambdaTable[L"jpg"]
			= [](const std::wstring& path, DirectX::TexMetadata* meta, DirectX::ScratchImage& img)
			-> HRESULT
		{
			return LoadFromWICFile(path.c_str(), DirectX::WIC_FLAGS_NONE, meta, img);
		};

		loadLambdaTable[L"tga"]
			= [](const std::wstring& path, DirectX::TexMetadata* meta, DirectX::ScratchImage& img)
			-> HRESULT
		{
			return LoadFromTGAFile(path.c_str(), meta, img);
		};

		loadLambdaTable[L"dds"]
			= [](const std::wstring& path, DirectX::TexMetadata* meta, DirectX::ScratchImage& img)
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

		D3D12_HEAP_PROPERTIES texHeapProp = {};
		texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
		texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
		texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
		texHeapProp.CreationNodeMask = 0;
		texHeapProp.VisibleNodeMask = 0;

		D3D12_RESOURCE_DESC resDesc = {};
		resDesc.Format = metadata.format;
		resDesc.Width = metadata.width;
		resDesc.Height = static_cast<UINT>(metadata.height);
		resDesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);
		resDesc.SampleDesc.Count = 1;
		resDesc.SampleDesc.Quality = 0;
		resDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
		resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
		resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		ID3D12Resource* pTextureResource = nullptr;
		result = pD3D12Device->CreateCommittedResource(
			&texHeapProp, D3D12_HEAP_FLAG_NONE,
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

		m_SharedResources.emplace(filename, pTextureResource);

		return pTextureResource;
	}

	/**
	 * リソースの破棄
	 */
	void PMDMesh::ClearResources()
	{
		if (m_pVertexBuffer) {
			m_pVertexBuffer->Release();
			m_pVertexBuffer = nullptr;
		}

		if (m_pIndexBuffer) {
			m_pIndexBuffer->Release();
			m_pIndexBuffer = nullptr;
		}

		if (m_pMaterialBuffer) {
			m_pMaterialBuffer->Release();
			m_pMaterialBuffer = nullptr;
		}

		if (m_pMaterialDescHeap) {
			m_pMaterialDescHeap->Release();
			m_pMaterialDescHeap = nullptr;
		}

		if (m_pWhiteTexture) {
			m_pWhiteTexture->Release();
			m_pWhiteTexture = nullptr;
		}

		if (m_pBlackTexture) {
			m_pBlackTexture->Release();
			m_pBlackTexture = nullptr;
		}
		
		if (m_pGradTexture) {
			m_pGradTexture->Release();
			m_pGradTexture = nullptr;
		}

		for (auto res : m_SharedResources)
		{
			res.second->Release();
		}
		m_SharedResources.clear();
	}
} // namespace pmd