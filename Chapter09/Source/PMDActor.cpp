#include "PMDActor.h"

// std
#include <algorithm>
#include <iostream>
#include <functional>
#include <map>
#include <vector>

// Windows
#include <tchar.h>
#include <Windows.h>

// DirectX
#include <d3dx12.h>
#include <DirectXTex.h>

#include "utils.h"

namespace pmd
{
	using namespace Microsoft::WRL;

	// PMD頂点レイアウト
	const std::vector<D3D12_INPUT_ELEMENT_DESC> PMDActor::InputLayout = {
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

	// コンストラクター
	PMDActor::PMDActor() :
		m_signature{}, m_header(),
		m_vertexBuffer(nullptr), m_vertexBufferView{},
		m_numberOfIndex(0), m_indexBuffer(nullptr), m_indexBufferView{},
		m_numberOfMaterial(0), m_materialBuffer(nullptr),
		m_materialDescHeap(nullptr), m_materials{}
	{
	}

	// デストラクター
	PMDActor::~PMDActor()
	{
	}

	// PMDファイルからの読み込み
	HRESULT PMDActor::LoadFromFile(ID3D12Device* const pD3D12Device, const std::wstring& filename, const std::wstring& toonTexturePath)
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
		unsigned int numberOfVertex;
		std::vector<unsigned char> rawVertices;
		std::fread(&numberOfVertex, sizeof(numberOfVertex), 1, fp);
		rawVertices.resize(numberOfVertex * VERTEX_SIZE);
		std::fread(rawVertices.data(), rawVertices.size(), 1, fp);

		result = CreateVertexBuffer(pD3D12Device, rawVertices);
		if (FAILED(result))
		{
			std::fclose(fp);
			return result;
		}

		// インデックスデータの読み込みとインデックスバッファーの生成
		std::vector<unsigned short> rawIndices;
		std::fread(&m_numberOfIndex, sizeof(m_numberOfIndex), 1, fp);
		rawIndices.resize(m_numberOfIndex);
		fread(rawIndices.data(), rawIndices.size() * sizeof(rawIndices[0]), 1, fp);
		result = pD3D12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(rawIndices.size() * sizeof(rawIndices[0])), D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr, IID_PPV_ARGS(m_indexBuffer.ReleaseAndGetAddressOf())
		);
		if (FAILED(result)) {
			std::fclose(fp);
			return result;
		}
		unsigned short* mappedIndex = nullptr;
		result = m_indexBuffer->Map(0, nullptr, (void**)&mappedIndex);
		if (FAILED(result)) {
			std::fclose(fp);
			return result;
		}
		std::copy(std::begin(rawIndices), std::end(rawIndices), mappedIndex);
		m_indexBuffer->Unmap(0, nullptr);
		mappedIndex = nullptr;

		m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
		m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
		m_indexBufferView.SizeInBytes = static_cast<UINT>(rawIndices.size() * sizeof(rawIndices[0]));

		// マテリアルの読み込み
		fread(&m_numberOfMaterial, sizeof(m_numberOfMaterial), 1, fp);
		std::vector<SerializedMaterialData> serializedMaterials(m_numberOfMaterial);
		fread(serializedMaterials.data(), serializedMaterials.size() * sizeof(SerializedMaterialData), 1, fp);

		m_materials.resize(m_numberOfMaterial);
		for (int i = 0; i < serializedMaterials.size(); i++) {
			m_materials[i].LoadFromSerializedData(pD3D12Device, serializedMaterials[i], folderPath, toonTexturePath);
#ifdef _DEBUG
			wprintf(L"material[%d]:", i);
#endif // _DEBUG
		}

		// マテリアルのバッファーを作成
		auto materialBufferSize = sizeof(BasicMaterial);
		materialBufferSize = (materialBufferSize + 0xff) & ~0xff;
		result = pD3D12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(materialBufferSize * m_numberOfMaterial),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr, IID_PPV_ARGS(m_materialBuffer.ReleaseAndGetAddressOf())
		);
		if (FAILED(result)) {
			std::fclose(fp);
			return result;
		}

		unsigned char* pMappedMaterial = nullptr;
		result = m_materialBuffer->Map(0, nullptr, (void**)&pMappedMaterial);
		for (auto& material : m_materials) {
			*reinterpret_cast<BasicMaterial*>(pMappedMaterial) = material.GetBasicMaterial();
			pMappedMaterial += materialBufferSize;
		}

		D3D12_DESCRIPTOR_HEAP_DESC matDescHeapDesc = {};
		matDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		matDescHeapDesc.NodeMask = 0;
		matDescHeapDesc.NumDescriptors = m_numberOfMaterial * (1 + NUMBER_OF_TEXTURE);
		matDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		result = pD3D12Device->CreateDescriptorHeap(&matDescHeapDesc, IID_PPV_ARGS(m_materialDescHeap.ReleaseAndGetAddressOf()));
		if (FAILED(result)) {
			std::fclose(fp);
			return result;
		}

		D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc = {};
		matCBVDesc.BufferLocation = m_materialBuffer->GetGPUVirtualAddress();
		matCBVDesc.SizeInBytes = static_cast<UINT>(materialBufferSize);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;

		auto matDescHeapH = m_materialDescHeap->GetCPUDescriptorHandleForHeapStart();
		auto incSize = pD3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		for (auto i = 0u; i < m_numberOfMaterial; i++) {
			pD3D12Device->CreateConstantBufferView(&matCBVDesc, matDescHeapH);
			matCBVDesc.BufferLocation += materialBufferSize;
			matDescHeapH.ptr += incSize;
			m_materials[i].CreateTextureBuffers(pD3D12Device, &srvDesc, &matDescHeapH, incSize);
		}

		std::fclose(fp);
		m_loadedModelPath = filename;
		return S_OK;
	}

	// 描画
	void PMDActor::Draw(ID3D12Device* const pD3D12Device, ID3D12GraphicsCommandList* const pCommandList)
	{
		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
		pCommandList->IASetIndexBuffer(&m_indexBufferView);

		ID3D12DescriptorHeap* materialDescHeap[] = { m_materialDescHeap.Get() };
		pCommandList->SetDescriptorHeaps(1, materialDescHeap);

		auto materialH = materialDescHeap[0]->GetGPUDescriptorHandleForHeapStart();
		auto cbvsrvIncSize = pD3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cbvsrvIncSize *= (1 + pmd::PMDActor::NUMBER_OF_TEXTURE);
		unsigned int idxOffset = 0;
		for (const auto& material : m_materials) {
			pCommandList->SetGraphicsRootDescriptorTable(1, materialH);
			pCommandList->DrawIndexedInstanced(material.GetIndicesNum(), 1, idxOffset, 0, 0);
			materialH.ptr += cbvsrvIncSize;
			idxOffset += material.GetIndicesNum();
		}
	}

	// 頂点バッファーの作成
	HRESULT PMDActor::CreateVertexBuffer(ID3D12Device* const pD3D12Device, const std::vector<unsigned char>& rawVertices)
	{
		HRESULT result;

		result = pD3D12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(rawVertices.size() * sizeof(rawVertices[0])), D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr, IID_PPV_ARGS(m_vertexBuffer.ReleaseAndGetAddressOf())
		);
		if (FAILED(result)) {
			return result;
		}

		unsigned char* mappedVertex = nullptr;
		result = m_vertexBuffer->Map(0, nullptr, (void**)&mappedVertex);
		if (FAILED(result)) {
			return result;
		}
		std::copy(std::begin(rawVertices), std::end(rawVertices), mappedVertex);
		m_vertexBuffer->Unmap(0, nullptr);
		mappedVertex = nullptr;

		m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
		m_vertexBufferView.SizeInBytes = static_cast<UINT>(rawVertices.size());
		m_vertexBufferView.StrideInBytes = VERTEX_SIZE;

		return S_OK;
	}

} // namespace pmd