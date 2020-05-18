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

	// トランスフォームは16Byte境界でnew()
	void* Transform::operator new(size_t size)
	{
		return _aligned_malloc(size, 16);
	}

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
		_pmdSignature{}, _pmdHeader(),
		_vertexBuffer(nullptr), _vertexBufferView{},
		_indexBuffer(nullptr), _indexBufferView{},
		_materialBuffer(nullptr), _materialDescHeap(nullptr), _meshes{},
		_transformBuff(nullptr), _transformDescHeap(nullptr), _mappedTransform(nullptr),
		_angle(0.0f)
	{
	}

	// デストラクター
	PMDActor::~PMDActor()
	{
		if (_transformBuff && _mappedTransform) {
			_transformBuff->Unmap(0, nullptr);
			_mappedTransform = nullptr;
		}
	}

	// PMDファイルからの読み込み
	HRESULT PMDActor::LoadFromFile(
		ID3D12Device* const pD3D12Device,
		D3D12ResourceCache* const pResourceCache,
		const std::wstring& filename,
		const std::wstring& toonTexturePath)
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
		std::fread(_pmdSignature, sizeof(_pmdSignature), 1, fp);
		std::fread(&_pmdHeader, sizeof(_pmdHeader), 1, fp);

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
		unsigned int numberOfIndex;
		std::vector<unsigned short> rawIndices;
		std::fread(&numberOfIndex, sizeof(numberOfIndex), 1, fp);
		rawIndices.resize(numberOfIndex);
		fread(rawIndices.data(), rawIndices.size() * sizeof(rawIndices[0]), 1, fp);
		result = CreateIndexBuffer(pD3D12Device, rawIndices);
		if (FAILED(result))
		{
			std::fclose(fp);
			return result;
		}

		// メッシュ情報の読み込み
		unsigned int numberOfMesh;
		fread(&numberOfMesh, sizeof(numberOfMesh), 1, fp);
		std::vector<SerializedMeshData> serializedMeshes(numberOfMesh);
		fread(serializedMeshes.data(), serializedMeshes.size() * sizeof(SerializedMeshData), 1, fp);
		std::fclose(fp);

		_meshes.resize(numberOfMesh);
		for (int i = 0; i < serializedMeshes.size(); i++) {
			_meshes[i].LoadFromSerializedData(pResourceCache, serializedMeshes[i], folderPath, toonTexturePath);
#ifdef _DEBUG
			wprintf(L"material[%d]:", i);
#endif // _DEBUG
		}

		// マテリアルのバッファーを作成
		result = CreateMaterialBuffers(pD3D12Device, pResourceCache, numberOfMesh, serializedMeshes);

		// 変換行列の定数バッファー
		result = pD3D12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer((sizeof(Transform) + 0xff) & ~0xff),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(_transformBuff.ReleaseAndGetAddressOf()));
		_transformBuff->SetName(L"ConstantBuffer(PMDActor)");
		if (FAILED(result))
		{
			return result;
		}

		// 変換行列の書き込みマップ
		result = _transformBuff->Map(0, nullptr, (void**)&_mappedTransform);

		// 変換行列のシェーダーリソースビュー
		D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
		descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		descHeapDesc.NodeMask = 0;
		descHeapDesc.NumDescriptors = 1;
		descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		result = pD3D12Device->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(_transformDescHeap.ReleaseAndGetAddressOf()));
		if (FAILED(result))
		{
			return result;
		}

		_transformDescHeap->SetName(L"TransformDescHeap(PMDActor)");
		auto heapHandle = _transformDescHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = _transformBuff->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = static_cast<UINT>(_transformBuff->GetDesc().Width);
		pD3D12Device->CreateConstantBufferView(&cbvDesc, heapHandle);

		// ロードしたモデルのパスを一応保持
		m_loadedModelPath = filename;

		return S_OK;
	}

	// 頂点バッファーの作成
	HRESULT PMDActor::CreateVertexBuffer(ID3D12Device* const pD3D12Device, const std::vector<unsigned char>& rawVertices)
	{
		HRESULT result;

		result = pD3D12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(rawVertices.size() * sizeof(rawVertices[0])), D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr, IID_PPV_ARGS(_vertexBuffer.ReleaseAndGetAddressOf())
		);
		if (FAILED(result)) {
			return result;
		}

		unsigned char* mappedVertex = nullptr;
		result = _vertexBuffer->Map(0, nullptr, (void**)&mappedVertex);
		if (FAILED(result)) {
			return result;
		}
		std::copy(std::begin(rawVertices), std::end(rawVertices), mappedVertex);
		_vertexBuffer->Unmap(0, nullptr);
		mappedVertex = nullptr;

		_vertexBufferView.BufferLocation = _vertexBuffer->GetGPUVirtualAddress();
		_vertexBufferView.SizeInBytes = static_cast<UINT>(rawVertices.size());
		_vertexBufferView.StrideInBytes = VERTEX_SIZE;

		return S_OK;
	}

	// インデックスバッファーの作成
	HRESULT PMDActor::CreateIndexBuffer(ID3D12Device* const pD3D12Device, const std::vector<unsigned short>& rawIndices)
	{
		HRESULT result;

		result = pD3D12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(rawIndices.size() * sizeof(rawIndices[0])), D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr, IID_PPV_ARGS(_indexBuffer.ReleaseAndGetAddressOf())
		);
		if (FAILED(result)) {
			return result;
		}

		unsigned short* mappedIndex = nullptr;
		result = _indexBuffer->Map(0, nullptr, (void**)&mappedIndex);
		if (FAILED(result)) {
			return result;
		}
		std::copy(std::begin(rawIndices), std::end(rawIndices), mappedIndex);
		_indexBuffer->Unmap(0, nullptr);
		mappedIndex = nullptr;

		_indexBufferView.BufferLocation = _indexBuffer->GetGPUVirtualAddress();
		_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
		_indexBufferView.SizeInBytes = static_cast<UINT>(rawIndices.size() * sizeof(rawIndices[0]));

		return S_OK;
	}

	// マテリアルバッファーの作成
	HRESULT PMDActor::CreateMaterialBuffers(
		ID3D12Device* const pD3D12Device,
		D3D12ResourceCache* const pResourceCache,
		unsigned int numberOfMesh,
		const std::vector<SerializedMeshData>& serializedMaterials
	) {
		HRESULT result;
		auto materialBufferSize = sizeof(BasicMaterial);
		materialBufferSize = (materialBufferSize + 0xff) & ~0xff;
		result = pD3D12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(materialBufferSize * numberOfMesh),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr, IID_PPV_ARGS(_materialBuffer.ReleaseAndGetAddressOf())
		);
		if (FAILED(result)) {
			return result;
		}

		unsigned char* pMappedMaterial = nullptr;
		result = _materialBuffer->Map(0, nullptr, (void**)&pMappedMaterial);
		for (auto& mesh : _meshes) {
			*reinterpret_cast<BasicMaterial*>(pMappedMaterial) = mesh.GetBasicMaterial();
			pMappedMaterial += materialBufferSize;
		}
		_materialBuffer->Unmap(0, nullptr);
		pMappedMaterial = nullptr;

		D3D12_DESCRIPTOR_HEAP_DESC matDescHeapDesc = {};
		matDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		matDescHeapDesc.NodeMask = 0;
		matDescHeapDesc.NumDescriptors = numberOfMesh * (1 + NUMBER_OF_TEXTURE);
		matDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		result = pD3D12Device->CreateDescriptorHeap(&matDescHeapDesc, IID_PPV_ARGS(_materialDescHeap.ReleaseAndGetAddressOf()));
		if (FAILED(result)) {
			return result;
		}

		D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc = {};
		matCBVDesc.BufferLocation = _materialBuffer->GetGPUVirtualAddress();
		matCBVDesc.SizeInBytes = static_cast<UINT>(materialBufferSize);

		auto matDescHeapH = _materialDescHeap->GetCPUDescriptorHandleForHeapStart();
		auto incSize = pD3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		for (auto i = 0u; i < numberOfMesh; i++) {
			pD3D12Device->CreateConstantBufferView(&matCBVDesc, matDescHeapH);
			matCBVDesc.BufferLocation += materialBufferSize;
			matDescHeapH.ptr += incSize;
			_meshes[i].CreateMaterialTextureViews(pD3D12Device, pResourceCache, &matDescHeapH);
		}

		return S_OK;
	}

	// フレーム更新
	void PMDActor::Update()
	{
		_angle += 0.01f;
		_mappedTransform->world = DirectX::XMMatrixRotationY(_angle);
	}

	// 描画
	void PMDActor::Draw(ID3D12Device* const pD3D12Device, ID3D12GraphicsCommandList* const pCommandList)
	{
		ID3D12DescriptorHeap* descHeaps[] = { _transformDescHeap.Get() };
		pCommandList->SetDescriptorHeaps(1, descHeaps);
		pCommandList->SetGraphicsRootDescriptorTable(1, _transformDescHeap->GetGPUDescriptorHandleForHeapStart());

		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommandList->IASetVertexBuffers(0, 1, &_vertexBufferView);
		pCommandList->IASetIndexBuffer(&_indexBufferView);

		ID3D12DescriptorHeap* materialDescHeap[] = { _materialDescHeap.Get() };
		pCommandList->SetDescriptorHeaps(1, materialDescHeap);

		auto gpuDescHandle = materialDescHeap[0]->GetGPUDescriptorHandleForHeapStart();
		auto handleIncSize = pD3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		handleIncSize *= (1 + pmd::PMDActor::NUMBER_OF_TEXTURE);
		unsigned int idxOffset = 0;
		for (const auto& mesh : _meshes) {
			pCommandList->SetGraphicsRootDescriptorTable(2, gpuDescHandle);
			pCommandList->DrawIndexedInstanced(mesh.GetIndicesNum(), 1, idxOffset, 0, 0);
			gpuDescHandle.ptr += handleIncSize;
			idxOffset += mesh.GetIndicesNum();
		}
	}

} // namespace pmd