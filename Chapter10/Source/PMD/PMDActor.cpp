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
		_transformBuff(nullptr), _transformDescHeap(nullptr), _mappedMatrices(nullptr),
		_angle(0.0f)
	{
	}

	// デストラクター
	PMDActor::~PMDActor()
	{
		if (_transformBuff && _mappedMatrices) {
			_transformBuff->Unmap(0, nullptr);
			_mappedMatrices = nullptr;
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

		// ファイル内容を全て読み込む
		fseek(fp, 0, SEEK_END);
		auto size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		std::vector<unsigned char> buff(size);
		fread(buff.data(), size, 1, fp);
		fclose(fp);

		auto pathIndex = filename.rfind('/');
		if (pathIndex == filename.npos) {
			pathIndex = filename.rfind('\\');
		}
		auto folderPath = filename.substr(0, pathIndex);

		// シグネチャーとヘッダー情報
		std::memcpy(_pmdSignature, buff.data(), sizeof(_pmdSignature));
		auto pHeader = reinterpret_cast<PMDHeader*>(buff.data() + sizeof(_pmdSignature));
		_pmdHeader = *pHeader;

		// 頂点データの読み込みと頂点バッファーの生成
		auto pNumberOfVertex = reinterpret_cast<unsigned int*>(pHeader + 1);
		auto numberOfVertex = *pNumberOfVertex;
		auto vertexDataSize = sizeof(SerializedVertex) * numberOfVertex;
		auto pVertexData = reinterpret_cast<unsigned char*>(pNumberOfVertex + 1);
		std::vector<unsigned char> rawVertices(vertexDataSize);
		std::memcpy(rawVertices.data(), pVertexData, vertexDataSize);
		result = CreateVertexBuffer(pD3D12Device, rawVertices);
		if (FAILED(result))
		{
			return result;
		}

		// インデックスデータの読み込みとインデックスバッファーの生成
		auto pNumberOfIndex = reinterpret_cast<unsigned int*>(pVertexData + vertexDataSize);
		auto numberOfIndex = *pNumberOfIndex;
		auto pIndexData = reinterpret_cast<unsigned short*>(pNumberOfIndex + 1);
		std::vector<unsigned short> rawIndices(numberOfIndex);
		std::memcpy(rawIndices.data(), pIndexData, sizeof(unsigned short) * numberOfIndex);
		result = CreateIndexBuffer(pD3D12Device, rawIndices);
		if (FAILED(result))
		{
			return result;
		}

		// メッシュ情報の読み込み
		auto pNumberOfMesh = reinterpret_cast<unsigned int*>(pIndexData + numberOfIndex);
		auto numberOfMesh = *pNumberOfMesh;
		auto pMeshData = reinterpret_cast<SerializedMeshData*>(pNumberOfMesh + 1);
		_meshes.resize(numberOfMesh);
		for (auto i = 0u; i < numberOfMesh; i++) {
			result = _meshes[i].LoadFromSerializedData(pResourceCache, pMeshData[i], folderPath, toonTexturePath);
			if (FAILED(result)) {
				return result;
			}
#ifdef _DEBUG
			printf("mesh[%d]:", i);
#endif // _DEBUG
		}
#ifdef _DEBUG
		printf("\n");
#endif // _DEBUG

		// マテリアルのバッファーを作成
		result = CreateMaterialBuffers(pD3D12Device, pResourceCache, numberOfMesh);

		// ボーン情報の読み込み
		auto pNumberOfBone = reinterpret_cast<unsigned short*>(pMeshData + numberOfMesh);
		auto numberOfBone = *pNumberOfBone;
		auto pBoneData = reinterpret_cast<PMDBone*>(pNumberOfBone + 1);
		std::vector<PMDBone> pmdBones(numberOfBone);
		std::memcpy(pmdBones.data(), pBoneData, sizeof(PMDBone) * numberOfBone);
		printf("boneNum = %d\n", numberOfBone);
		for (auto bone : pmdBones) {
			printf("boneName = %s\n", bone.boneName);
		}

		// ボーンノードマップを作る
		std::vector<std::string> boneNames(pmdBones.size());
		for (int i = 0; i < pmdBones.size(); i++)
		{
			const auto& bone = pmdBones[i];
			boneNames[i] = bone.boneName;
			auto& node = _boneNodeTable[bone.boneName];
			node.boneIdx = i;
			node.startPos = bone.pos;
		}

		// 親子関係を構築する
		for (auto& bone : pmdBones) {
			// 親インデックスをチェックしてあり得ない番号なら飛ばす
			if (bone.parentNo >= pmdBones.size()) {
				continue;
			}
			const auto parentName = boneNames[bone.parentNo];
			_boneNodeTable[parentName].children.emplace_back(&_boneNodeTable[bone.boneName]);
		}

		// 全てのボーンを初期化
		_boneMatrices.resize(pmdBones.size());
		std::fill(_boneMatrices.begin(), _boneMatrices.end(), DirectX::XMMatrixIdentity());

		// 変換行列の定数バッファー
		result = CreateTransformView(pD3D12Device);
		if (FAILED(result))
		{
			return result;
		}

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
		_vertexBufferView.StrideInBytes = sizeof(SerializedVertex);

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

	// メッシュ単位で参照されるマテリアルバッファーの作成
	HRESULT PMDActor::CreateMaterialBuffers(
		ID3D12Device* const pD3D12Device,
		D3D12ResourceCache* const pResourceCache,
		unsigned int numberOfMesh
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

	// 座標変換行列を格納する定数バッファービューの作成
	HRESULT PMDActor::CreateTransformView(ID3D12Device* const pD3D12Device)
	{
		HRESULT result;

		auto buffSize = sizeof(Transform) * (1 + _boneMatrices.size());
		buffSize = (buffSize + 0xff) & ~0xff;

		result = pD3D12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(buffSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(_transformBuff.ReleaseAndGetAddressOf()));
		_transformBuff->SetName(L"ConstantBuffer(PMDActor)");
		if (FAILED(result)) {
			return result;
		}

		// 変換行列の書き込みマップ
		result = _transformBuff->Map(0, nullptr, (void**)&_mappedMatrices);
		if (FAILED(result)) {
			return result;
		}

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

		// 特定のノード（左腕）をZ軸周りに90°回転させてみる
		auto armNode = _boneNodeTable["左腕"];
		auto& armPos = armNode.startPos;
		auto armMatrix = 
			DirectX::XMMatrixTranslation(-armPos.x, -armPos.y, -armPos.z)
			* DirectX::XMMatrixRotationZ(DirectX::XM_PIDIV2)
			* DirectX::XMMatrixTranslation(armPos.x, armPos.y, armPos.z);
		_boneMatrices[armNode.boneIdx] = armMatrix;

		auto elbowNode = _boneNodeTable["左ひじ"];
		auto& elbowPos = elbowNode.startPos;
		auto elbowMatrix = 
			DirectX::XMMatrixTranslation(-elbowPos.x, -elbowPos.y, -elbowPos.z)
			* DirectX::XMMatrixRotationZ(-DirectX::XM_PIDIV2)
			* DirectX::XMMatrixTranslation(elbowPos.x, elbowPos.y, elbowPos.z);
		_boneMatrices[elbowNode.boneIdx] = elbowMatrix;
		RecursiveMatrixMultiply(&_boneNodeTable["センター"], DirectX::XMMatrixIdentity());

		std::copy(_boneMatrices.begin(), _boneMatrices.end(), &_mappedMatrices[1]);

		return S_OK;
	}

	void PMDActor::RecursiveMatrixMultiply(const BoneNode* const pNode, const DirectX::XMMATRIX& matrix)
	{
		_boneMatrices[pNode->boneIdx] *= matrix;
		for (auto& childNode : pNode->children) {
			RecursiveMatrixMultiply(childNode, _boneMatrices[pNode->boneIdx]);
		}
	}

	// フレーム更新
	void PMDActor::Update()
	{
		_angle += 0.01f;
		_mappedMatrices[0] = DirectX::XMMatrixRotationY(_angle);
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