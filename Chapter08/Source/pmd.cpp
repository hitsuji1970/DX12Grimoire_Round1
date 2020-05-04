#include <d3dx12.h>
#include <tchar.h>
#include <iostream>
#include <Windows.h>
#include <vector>
#include "pmd.h"

PMDMesh::PMDMesh() :
	m_signature{}, m_header(),
	m_numberOfVertex(0), m_pVertexBuffer(nullptr), m_vertexBufferView{},
	m_numberOfIndex(0), m_pIndexBuffer(nullptr), m_indexBufferView{}
{
}

PMDMesh::~PMDMesh()
{
	ClearResources();
}

HRESULT PMDMesh::LoadFromFile(ID3D12Device* const pD3D12Device, LPCTSTR cpFileName)
{
	HRESULT result;
	FILE* fp = nullptr;

	auto state = _tfopen_s(&fp, cpFileName, TEXT("rb"));
	if (state != 0) {
		return E_FAIL;
	}

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

	std::fclose(fp);
	return S_OK;
}

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
}