﻿#include <Windows.h>
#include <tchar.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <DirectXTex.h>
#include <d3dx12.h>
#include <d3dcompiler.h>
#include <vector>

#ifdef _DEBUG
#include <iostream>
#endif

#include "pmd.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "DirectXTex.lib")

// 関数プロトタイプ
HWND InitWindow(WNDCLASSEX* const pWndClass);
LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
HRESULT CreateTextureResource(const DirectX::Image* const pImg, const DirectX::TexMetadata& texMetaData, ID3D12Resource** ppUploadBuffer, ID3D12Resource** ppTextureBuffer);
size_t AlignmentedSize(size_t size, size_t alignment);
void DebugOutputFromString(const char* format, ...);
void EnableDebugLayer();

// 定数
constexpr unsigned int window_width = 1280;
constexpr unsigned int window_height = 720;

// DirextXオブジェクト
IDXGIFactory6* _dxgiFactory = nullptr;
ID3D12Device* _dev = nullptr;
IDXGISwapChain4* _swapchain = nullptr;
ID3D12CommandAllocator* _cmdAllocator = nullptr;
ID3D12GraphicsCommandList* _cmdList = nullptr;
ID3D12CommandQueue* _cmdQueue = nullptr;

// シェーダーに渡す行列
struct MatricesData
{
	DirectX::XMMATRIX world;
	DirectX::XMMATRIX viewproj;
};

// テクスチャーテスト構造体
struct TexRGBA
{
	unsigned char R, G, B, A;
};

#ifdef _DEBUG
int main()
#else
int WINAPI _tWinMain(HINSTANCE, HINSTANCE, LPTSTR, int)
#endif
{
	DebugOutputFromString("Show window test.");

	WNDCLASSEX w = {};
	HWND hWnd = InitWindow(&w);

	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	HRESULT result = S_OK;

#ifdef _DEBUG
	EnableDebugLayer();
	if (FAILED(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_dxgiFactory)))) {
		if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&_dxgiFactory)))) {
			return -1;
		}
	}
#else
	if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory)))) {
		return -1;
	}
#endif // _DEBUG

	D3D_FEATURE_LEVEL featureLevel;
	for (auto lv : levels) {
		if (D3D12CreateDevice(nullptr, lv, IID_PPV_ARGS(&_dev)) == S_OK) {
			featureLevel = lv;
			break;
		}
	}

	if (!_dev) {
		::MessageBox(hWnd, TEXT("Error"), TEXT("D3Dデバイスの初期化に失敗しました。"), MB_ICONERROR);
		exit(-1);
	}

	// コマンドリスト
	result = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmdAllocator));
	result = _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator, nullptr, IID_PPV_ARGS(&_cmdList));

	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	result = _dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&_cmdQueue));

	// スワップチェーン
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = window_width;
	swapChainDesc.Height = window_height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = false;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	result = _dxgiFactory->CreateSwapChainForHwnd(
		_cmdQueue, hWnd, &swapChainDesc, nullptr, nullptr,
		(IDXGISwapChain1**)&_swapchain);

	// ディスクリプターヒープ
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ID3D12DescriptorHeap* rtvHeaps = nullptr;
	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeaps));

	// スワップチェーンに関連付け
	std::vector<ID3D12Resource*> _backBuffers(swapChainDesc.BufferCount);
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	for (auto idx = 0u; idx < swapChainDesc.BufferCount; ++idx) {
		result = _swapchain->GetBuffer(idx, IID_PPV_ARGS(&_backBuffers[idx]));
		_dev->CreateRenderTargetView(_backBuffers[idx], nullptr, cpuDescHandle);
		cpuDescHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	// フェンス
	ID3D12Fence* _fence = nullptr;
	UINT64 _fenceVal = 0;
	result = _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));

	ShowWindow(hWnd, SW_SHOW);

	// 頂点バッファー
	constexpr size_t pmdvertex_size = 38;
	FILE* fp;
	_tfopen_s(&fp, TEXT("model/初音ミク.pmd"), TEXT("rb"));

	char signature[3] = {};
	PMDHeader pmdHeader;
	fread(signature, sizeof(signature), 1, fp);
	fread(&pmdHeader, sizeof(pmdHeader), 1, fp);

	unsigned int vertNum;
	fread(&vertNum, sizeof(vertNum), 1, fp);
	std::vector<unsigned char> vertices(vertNum * pmdvertex_size);
	fread(vertices.data(), vertices.size(), 1, fp);

	ID3D12Resource* vertBuff = nullptr;
	result = _dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vertices.size()), D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr, IID_PPV_ARGS(&vertBuff));

	unsigned char* vertMap = nullptr;
	result = vertBuff->Map(0, nullptr, (void**)&vertMap);
	std::copy(std::begin(vertices), std::end(vertices), vertMap);
	vertBuff->Unmap(0, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();
	vbView.SizeInBytes = vertices.size();
	vbView.StrideInBytes = pmdvertex_size;

	// インデックスバッファー
	std::vector<unsigned short> indices;
	unsigned int indicesNum;
	fread(&indicesNum, sizeof(indicesNum), 1, fp);
	indices.resize(indicesNum);
	fread(indices.data(), indices.size() * sizeof(indices[0]), 1, fp);

	fclose(fp);

	ID3D12Resource* idxBuff = nullptr;
	result = _dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(indices.size() * sizeof(indices[0])), D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr, IID_PPV_ARGS(&idxBuff));

	unsigned short* mappedIndex = nullptr;
	idxBuff->Map(0, nullptr, (void**)&mappedIndex);
	std::copy(std::begin(indices), std::end(indices), mappedIndex);
	idxBuff->Unmap(0, nullptr);

	D3D12_INDEX_BUFFER_VIEW ibView = {};
	ibView.BufferLocation = idxBuff->GetGPUVirtualAddress();
	ibView.Format = DXGI_FORMAT_R16_UINT;
	ibView.SizeInBytes = indices.size() * sizeof(indices[0]);

	// 深度バッファー
	D3D12_RESOURCE_DESC depthResDesc = {};
	depthResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthResDesc.Width = window_width;
	depthResDesc.Height = window_height;
	depthResDesc.DepthOrArraySize = 1;
	depthResDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthResDesc.SampleDesc.Count = 1;
	depthResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	// 深度値ヒーププロパティ
	D3D12_HEAP_PROPERTIES depthHeapProp = {};
	depthHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	depthHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	depthHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_CLEAR_VALUE depthClearValue = {};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;

	ID3D12Resource* depthBuffer = nullptr;
	result = _dev->CreateCommittedResource(
		&depthHeapProp, D3D12_HEAP_FLAG_NONE,
		&depthResDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue, IID_PPV_ARGS(&depthBuffer));

	// 深度バッファー用のディスクリプターヒープ
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	ID3D12DescriptorHeap* dsvHeap = nullptr;
	result = _dev->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap));
	
	// 深度ビュー
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	_dev->CreateDepthStencilView(depthBuffer, &dsvDesc, dsvHeap->GetCPUDescriptorHandleForHeapStart());

	// テクスチャーリソース
	const wchar_t* textureFileName = L"img/textest200x200.png";
	DirectX::TexMetadata texMetaData = {};
	DirectX::ScratchImage scratchImg = {};
	result = DirectX::LoadFromWICFile(
		textureFileName, DirectX::WIC_FLAGS_NONE,
		&texMetaData, scratchImg);

	auto img = scratchImg.GetImage(0, 0, 0);

	// テクスチャーバッファー
	ID3D12Resource* uploadBuff = nullptr;
	ID3D12Resource* texBuff = nullptr;
	result = CreateTextureResource(img, texMetaData, &uploadBuff, &texBuff);

	uint8_t* mapforImg = nullptr;
	result = uploadBuff->Map(0, nullptr, (void**)&mapforImg);
	auto srcAddress = img->pixels;
	auto rowPitch = AlignmentedSize(img->rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	for (int y = 0; y < img->height; y++) {
		std::copy_n(srcAddress, rowPitch, mapforImg);
		srcAddress += img->rowPitch;
		mapforImg += rowPitch;
	}
	//std::copy_n(img->pixels, img->slicePitch, mapforImg);
	uploadBuff->Unmap(0, nullptr);

	{
		// コピー元
		D3D12_TEXTURE_COPY_LOCATION src = {};
		src.pResource = uploadBuff;
		src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		src.PlacedFootprint.Offset = 0;
		src.PlacedFootprint.Footprint.Width = (UINT)texMetaData.width;
		src.PlacedFootprint.Footprint.Height = (UINT)texMetaData.height;
		src.PlacedFootprint.Footprint.Depth = (UINT)texMetaData.depth;
		src.PlacedFootprint.Footprint.RowPitch = (UINT)AlignmentedSize(img->rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
		src.PlacedFootprint.Footprint.Format = img->format;

		// コピー先
		D3D12_TEXTURE_COPY_LOCATION dst = {};
		dst.pResource = texBuff;
		dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst.SubresourceIndex = 0;

		// コピーコマンド
		_cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
		//result = texBuff->WriteToSubresource(0, nullptr, img->pixels, img->rowPitch, img->slicePitch);

		D3D12_RESOURCE_BARRIER BarrierDesc;
		BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		BarrierDesc.Transition.pResource = texBuff;
		BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		_cmdList->ResourceBarrier(1, &BarrierDesc);
		_cmdList->Close();

		ID3D12CommandList* cmdLists[] = { _cmdList };
		_cmdQueue->ExecuteCommandLists(1, cmdLists);
		_cmdQueue->Signal(_fence, ++_fenceVal);
		if (_fence->GetCompletedValue() != _fenceVal) {
			auto event = CreateEvent(nullptr, false, false, nullptr);
			_fence->SetEventOnCompletion(_fenceVal, event);
			WaitForSingleObject(event, INFINITE);
			CloseHandle(event);
		}
		_cmdAllocator->Reset();
		_cmdList->Reset(_cmdAllocator, nullptr);
	}

	// 定数バッファーに行列を設定
	auto worldMatrix = DirectX::XMMatrixRotationY(DirectX::XM_PIDIV4);
	DirectX::XMFLOAT3 eye(0, 10, -15);
	DirectX::XMFLOAT3 target(0, 10, 0);
	DirectX::XMFLOAT3 up(0, 1, 0);

	auto viewMatrix = DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat3(&eye), DirectX::XMLoadFloat3(&target), DirectX::XMLoadFloat3(&up));
	auto projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV2, static_cast<float>(window_width) / static_cast<float>(window_height), 1.0f, 100.f);

	// 定数バッファー
	ID3D12Resource* constBuff = nullptr;
	result = _dev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer((sizeof(MatricesData) + 0xff) & ~0xff),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&constBuff));

	// 行列をコピー
	MatricesData* mapMatrix;
	result = constBuff->Map(0, nullptr, (void**)&mapMatrix);

	// シェーダーリソースビュー
	ID3D12DescriptorHeap* basicDescHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NodeMask = 0;

	// SRV1つとCBV1つ
	descHeapDesc.NumDescriptors = 2;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	result = _dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&basicDescHeap));
	auto basicHeapHandle = basicDescHeap->GetCPUDescriptorHandleForHeapStart();

	// シェーダーリソースビュー（テクスチャー）
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = texMetaData.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	_dev->CreateShaderResourceView(texBuff, &srvDesc, basicHeapHandle);
	basicHeapHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// シェーダーリソースビュー（定数バッファー）
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = constBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = constBuff->GetDesc().Width;
	_dev->CreateConstantBufferView(&cbvDesc, basicHeapHandle);

	// シェーダー
	ID3DBlob* _vsBlob = nullptr;
	ID3DBlob* _psBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;

	result = D3DCompileFromFile(
		L"Source/BasicVertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicVS", "vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&_vsBlob, &errorBlob);

	if (FAILED(result)) {
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
			::OutputDebugStringA("ファイルが見つかりません");
			return 0;
		}
		else {
			std::string errStr;
			errStr.resize(errorBlob->GetBufferSize());
			std::copy_n((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize(), errStr.begin());
			errStr += "\n";
			::OutputDebugStringA(errStr.c_str());
		}
	}

	result = D3DCompileFromFile(
		L"Source/BasicPixelShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&_psBlob, &errorBlob);

	if (FAILED(result)) {
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
			::OutputDebugStringA("ファイルが見つかりません");
			return 0;
		}
		else {
			std::string errStr;
			errStr.resize(errorBlob->GetBufferSize());
			std::copy_n((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize(), errStr.begin());
			errStr += "\n";
			::OutputDebugStringA(errStr.c_str());
		}
	}

	// 頂点レイアウト
	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
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

	D3D12_DESCRIPTOR_RANGE descTblRange[2] = {};

	// テクスチャー用レジスター0番
	descTblRange[0].NumDescriptors = 1;
	descTblRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descTblRange[0].BaseShaderRegister = 0;
	descTblRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// 定数用レジスタ－1番
	descTblRange[1].NumDescriptors = 1;
	descTblRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	descTblRange[1].BaseShaderRegister = 0;
	descTblRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;


	D3D12_ROOT_PARAMETER rootParam = {};
	rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam.DescriptorTable.pDescriptorRanges = descTblRange;
	rootParam.DescriptorTable.NumDescriptorRanges = 2;
	rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// サンプラー設定
	D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.MaxLOD = D3D12_SHADER_VISIBILITY_PIXEL;
	samplerDesc.MinLOD = 0;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

	D3D12_ROOT_SIGNATURE_DESC rootsignatureDesc = {};
	rootsignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootsignatureDesc.pParameters = &rootParam;
	rootsignatureDesc.NumParameters = 1;
	rootsignatureDesc.pStaticSamplers = &samplerDesc;
	rootsignatureDesc.NumStaticSamplers = 1;

	ID3DBlob* rootSigBlob = nullptr;
	result = D3D12SerializeRootSignature(
		&rootsignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob,
		&errorBlob);

	ID3D12RootSignature* rootSignature = nullptr;
	result = _dev->CreateRootSignature(
		0,
		rootSigBlob->GetBufferPointer(),
		rootSigBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature));
	rootSigBlob->Release();

	// パイプラインステート
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc = {};
	pipelineStateDesc.pRootSignature = rootSignature;
	pipelineStateDesc.VS.pShaderBytecode = _vsBlob->GetBufferPointer();
	pipelineStateDesc.VS.BytecodeLength = _vsBlob->GetBufferSize();
	pipelineStateDesc.PS.pShaderBytecode = _psBlob->GetBufferPointer();
	pipelineStateDesc.PS.BytecodeLength = _psBlob->GetBufferSize();
	pipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	pipelineStateDesc.DepthStencilState.DepthEnable = true;
	pipelineStateDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	pipelineStateDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	pipelineStateDesc.DepthStencilState.StencilEnable = false;
	pipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	pipelineStateDesc.RasterizerState.MultisampleEnable = false;
	pipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	pipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	pipelineStateDesc.RasterizerState.DepthClipEnable = true;
	pipelineStateDesc.BlendState.AlphaToCoverageEnable = false;
	pipelineStateDesc.BlendState.IndependentBlendEnable = false;

	D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};
	renderTargetBlendDesc.BlendEnable = false;
	renderTargetBlendDesc.LogicOpEnable = false;
	renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	pipelineStateDesc.BlendState.RenderTarget[0] = renderTargetBlendDesc;

	pipelineStateDesc.InputLayout.pInputElementDescs = inputLayout;
	pipelineStateDesc.InputLayout.NumElements = _countof(inputLayout);
	pipelineStateDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateDesc.NumRenderTargets = 1;
	pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	pipelineStateDesc.SampleDesc.Count = 1;
	pipelineStateDesc.SampleDesc.Quality = 0;

	ID3D12PipelineState* _pipelineState = nullptr;
	result = _dev->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(&_pipelineState));

	D3D12_VIEWPORT viewport = {};
	viewport.Width = window_width;
	viewport.Height = window_height;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MaxDepth = 1.0f;
	viewport.MinDepth = 0.0f;

	D3D12_RECT scissorRect = {};
	scissorRect.top = 0;
	scissorRect.left = 0;
	scissorRect.right = scissorRect.left + window_width;
	scissorRect.bottom = scissorRect.top + window_height;

	MSG msg = {};
	auto angle = 0.0f;

	while (true) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (msg.message == WM_QUIT) {
			break;
		}

		auto bbIdx = _swapchain->GetCurrentBackBufferIndex();

		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			_backBuffers[bbIdx], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

		_cmdList->SetPipelineState(_pipelineState);

		auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		auto dsvH = dsvHeap->GetCPUDescriptorHandleForHeapStart();
		rtvH.ptr += static_cast<size_t>(bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
		_cmdList->OMSetRenderTargets(1, &rtvH, true, &dsvH);

		float clearColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
		_cmdList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		_cmdList->RSSetViewports(1, &viewport);
		_cmdList->RSSetScissorRects(1, &scissorRect);
		_cmdList->SetGraphicsRootSignature(rootSignature);

		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_cmdList->IASetVertexBuffers(0, 1, &vbView);
		_cmdList->IASetIndexBuffer(&ibView);

		_cmdList->SetGraphicsRootSignature(rootSignature);
		_cmdList->SetDescriptorHeaps(1, &basicDescHeap);
		_cmdList->SetGraphicsRootDescriptorTable(0, basicDescHeap->GetGPUDescriptorHandleForHeapStart());
		_cmdList->DrawIndexedInstanced(indicesNum, 1, 0, 0, 0);

		angle = DirectX::XM_PI;
		worldMatrix = DirectX::XMMatrixRotationY(angle);
		mapMatrix->world = worldMatrix;
		mapMatrix->viewproj = viewMatrix * projectionMatrix;

		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			_backBuffers[bbIdx], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

		_cmdList->Close();

		ID3D12CommandList* cmdLists[] = { _cmdList };
		_cmdQueue->ExecuteCommandLists(1, cmdLists);
		_cmdQueue->Signal(_fence, ++_fenceVal);
		if (_fence->GetCompletedValue() != _fenceVal) {
			auto event = CreateEvent(nullptr, false, false, nullptr);
			_fence->SetEventOnCompletion(_fenceVal, event);
			WaitForSingleObject(event, INFINITE);
			CloseHandle(event);
		}

		_cmdAllocator->Reset();
		_cmdList->Reset(_cmdAllocator, nullptr);

		_swapchain->Present(1, 0);
	}

	UnregisterClass(w.lpszClassName, w.hInstance);

	return 0;
}

HWND InitWindow(WNDCLASSEX* const pWndClass)
{
	DebugOutputFromString("Show window test.");
	pWndClass->cbSize = sizeof(WNDCLASSEX);
	pWndClass->lpfnWndProc = WNDPROC(WindowProcedure);
	pWndClass->lpszClassName = TEXT("DX12Sample");
	pWndClass->hInstance = GetModuleHandle(nullptr);

	RegisterClassEx(pWndClass);

	RECT wrc = { 0, 0, window_width, window_height };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	HWND hWnd = CreateWindow(pWndClass->lpszClassName,
		TEXT("DX12テスト PMDモデル表示"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		pWndClass->hInstance,
		nullptr);

	return hWnd;
}

LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_DESTROY) {
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

HRESULT CreateTextureResource(
	const DirectX::Image* const pImg,
	const DirectX::TexMetadata& texMetaData,
	ID3D12Resource** ppUploadbuffer,
	ID3D12Resource** ppTextureBuffer)
{
	D3D12_HEAP_PROPERTIES uploadHepProp = {};
	uploadHepProp.Type = D3D12_HEAP_TYPE_UPLOAD;
	uploadHepProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	uploadHepProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	uploadHepProp.CreationNodeMask = 0;
	uploadHepProp.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Format = DXGI_FORMAT_UNKNOWN;
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Width = AlignmentedSize(pImg->rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) * pImg->height;
	resDesc.Height = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;

	auto result = _dev->CreateCommittedResource(
		&uploadHepProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(ppUploadbuffer));

	if (result != S_OK) {
		return result;
	}

	D3D12_HEAP_PROPERTIES texHeapProp = {};
	texHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	texHeapProp.CreationNodeMask = 0;
	texHeapProp.VisibleNodeMask = 0;

	resDesc.Format = texMetaData.format;
	resDesc.Width = (UINT)texMetaData.width;
	resDesc.Height = (UINT)texMetaData.height;
	resDesc.DepthOrArraySize = (UINT)texMetaData.arraySize;
	resDesc.MipLevels = (UINT)texMetaData.mipLevels;
	resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(texMetaData.dimension);
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	return _dev->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(ppTextureBuffer));
}

size_t AlignmentedSize(size_t size, size_t alignment)
{
	return size + alignment - size % alignment;
}

void EnableDebugLayer()
{
	ID3D12Debug* debugLayer = nullptr;
	auto result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));
	debugLayer->EnableDebugLayer();
	debugLayer->Release();
}

// デバッグ出力
void DebugOutputFromString(const char* format, ...)
{
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	std::vprintf(format, valist);
	va_end(valist);
#endif
}

