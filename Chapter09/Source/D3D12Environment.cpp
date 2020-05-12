#include "D3D12Environment.h"
#include <d3dx12.h>
#include <string>
#include <vector>

// コンストラクター
D3D12Environment::D3D12Environment() :
	_dxgiFactory(nullptr), _device(nullptr), _featureLevel(D3D_FEATURE_LEVEL_1_0_CORE),
	_commandAllocator(nullptr), _commandQueue(nullptr),
	_swapChain(nullptr), _rtvHeaps(nullptr), _backBuffers{},
	_dsvHeap(nullptr), _depthBuffer(nullptr),
	_fence(nullptr), _fenceVal(0),
	_viewport{}, _scissorRect{}
{
}

// デストラクター
D3D12Environment::~D3D12Environment()
{
}


// 初期化
HRESULT D3D12Environment::Initialize(HWND hWnd, UINT windowWidth, UINT windowHeight)
{
	HRESULT result = S_OK;

#ifdef _DEBUG
	// デバッグレイヤーの有効化
	ComPtr<ID3D12Debug> debugLayer = nullptr;
	result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));
	if (SUCCEEDED(result))
	{
		debugLayer->EnableDebugLayer();
	}
	else
	{
		wprintf(L"failed to enable debug layer.\n");
	}
#endif // _DEBUG

	// Direct3Dデバイスの初期化
	if (FAILED(InitializeDXGIDevice(hWnd)))
	{
		return E_FAIL;
	}

	// コマンドアロケーター／コマンドリスト／コマンドキューの作成
	if (FAILED(CreateCommandBuffers()))
	{
		return E_FAIL;
	}

	// スワップチェインの作成
	if (FAILED(CreateSwapChain(hWnd, windowWidth, windowHeight)))
	{
		return E_FAIL;
	}

	// バックバッファーを作成してスワップチェインに紐付け
	if (FAILED(CreateBackBuffers(hWnd, windowWidth, windowHeight)))
	{
		return E_FAIL;
	}

	if (FAILED(CreateDepthBuffer(windowWidth, windowHeight)))
	{
		return E_FAIL;
	}

	// フェンス
	result = _device->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(_fence.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		return result;
	}

	_viewport.Width = static_cast<float>(windowWidth);
	_viewport.Height = static_cast<float>(windowHeight);
	_viewport.TopLeftX = 0;
	_viewport.TopLeftY = 0;
	_viewport.MaxDepth = 1.0f;
	_viewport.MinDepth = 0.0f;

	_scissorRect.top = 0;
	_scissorRect.left = 0;
	_scissorRect.right = _scissorRect.left + windowWidth;
	_scissorRect.bottom = _scissorRect.top + windowHeight;

	return S_OK;
}

void
D3D12Environment::BeginDraw()
{
	auto bbIdx = _swapChain->GetCurrentBackBufferIndex();
	auto pBackBuffer = _backBuffers[bbIdx];

	_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		pBackBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	//commandList->SetPipelineState(_pipelineState.Get());

	auto rtvH = _rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	auto dsvH = _dsvHeap->GetCPUDescriptorHandleForHeapStart();
	rtvH.ptr += static_cast<size_t>(bbIdx) * _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	_commandList->OMSetRenderTargets(1, &rtvH, true, &dsvH);

	float clearColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	_commandList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
	_commandList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	_commandList->RSSetViewports(1, &_viewport);
	_commandList->RSSetScissorRects(1, &_scissorRect);
}


void
D3D12Environment::EndDraw()
{
	auto bbIdx = _swapChain->GetCurrentBackBufferIndex();
	auto pBackBuffer = _backBuffers[bbIdx];
	_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		pBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	_commandList->Close();

	ID3D12CommandList* cmdLists[] = { _commandList.Get() };
	ExecuteCommandLists(1, cmdLists);
	_commandList->Reset(_commandAllocator.Get(), nullptr);

	_swapChain->Present(1, 0);
}

// DXGIとDirectXデバイスの初期化
HRESULT
D3D12Environment::InitializeDXGIDevice(HWND hWnd)
{
#ifdef _DEBUG
	UINT flagsDXGI = 0;
	flagsDXGI |= DXGI_CREATE_FACTORY_DEBUG;
	auto result = ::CreateDXGIFactory2(flagsDXGI, IID_PPV_ARGS(_dxgiFactory.ReleaseAndGetAddressOf()));
#else
	auto result = ::CreateDXGIFactory1(IID_PPV_ARGS(_dxgiFactory.ReleaseAndGetAddressOf()));
#endif // _DEBUG
	if (FAILED(result)) {
		return result;
	}

	// グラフィックスカードがサポートする機能レベル
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	// ノートPC用にNVIDIAのグラフィックスを優先して検出
	auto adapter = FindDXGIAdapter(L"NVIDIA");
	for (auto lv : levels) {
		if (D3D12CreateDevice(adapter, lv, IID_PPV_ARGS(_device.ReleaseAndGetAddressOf())) == S_OK) {
			_featureLevel = lv;
			break;
		}
	}
	if (!_device) {
		::MessageBox(hWnd, TEXT("Error"), TEXT("D3Dデバイスの初期化に失敗しました。"), MB_ICONERROR);
		return E_FAIL;
	}
	adapter->Release();

	return S_OK;
}

// コマンドアロケーター／コマンドリスト／コマンドキューの作成
HRESULT
D3D12Environment::CreateCommandBuffers()
{
	constexpr D3D12_COMMAND_LIST_TYPE CommandListType = D3D12_COMMAND_LIST_TYPE_DIRECT;
	constexpr UINT NodeMask = 0;

	HRESULT result;

	// コマンドアロケーター
	result = _device->CreateCommandAllocator(CommandListType, IID_PPV_ARGS(_commandAllocator.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		return result;
	}
	_commandAllocator->SetName(L"CommandAllocator");

	// コマンドリスト
	//_device->CreateCommandList(0, type, _cmdAllocator.Get(), pInitialState, riid, ppCommandList);
	result = _device->CreateCommandList(NodeMask, CommandListType, _commandAllocator.Get(), nullptr, IID_PPV_ARGS(_commandList.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		return result;
	}
	_commandList->SetName(L"CommandList");

	// コマンドキュー
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cmdQueueDesc.NodeMask = NodeMask;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmdQueueDesc.Type = CommandListType;
	result = _device->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(_commandQueue.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		return result;
	}
	_commandQueue->SetName(L"CommandQueue");

	return S_OK;
}

// スワップチェインの生成
HRESULT
D3D12Environment::CreateSwapChain(HWND hWnd, UINT bufferWidth, UINT bufferHeight)
{
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = bufferWidth;
	swapChainDesc.Height = bufferHeight;
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

	auto result = _dxgiFactory->CreateSwapChainForHwnd(
		_commandQueue.Get(), hWnd, &swapChainDesc, nullptr, nullptr,
		(IDXGISwapChain1**)_swapChain.ReleaseAndGetAddressOf());

	if (FAILED(result)) {
		return result;
	}

	return S_OK;
}

// バックバッファーの生成
HRESULT
D3D12Environment::CreateBackBuffers(HWND hWnd, UINT bufferWidth, UINT bufferHeight)
{
	HRESULT result;

	// ディスクリプターヒープ
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	result = _device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_rtvHeaps.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		return result;
	}
	_rtvHeaps->SetName(L"RenderTargetHeap");

	// スワップチェインにバインド
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	result = _swapChain->GetDesc1(&swapChainDesc);
	if (FAILED(result)) {
		return result;
	}

	_backBuffers.resize(swapChainDesc.BufferCount);
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle = _rtvHeaps->GetCPUDescriptorHandleForHeapStart();

	for (auto idx = 0u; idx < swapChainDesc.BufferCount; ++idx) {
		result = _swapChain->GetBuffer(idx, IID_PPV_ARGS(&_backBuffers[idx]));
		if (FAILED(result)) {
			return result;
		}
		wchar_t nameBuff[32];
		swprintf_s(nameBuff, 32l, L"BackBuffer[%d]", idx);
		_backBuffers[idx]->SetName(nameBuff);
		_device->CreateRenderTargetView(_backBuffers[idx], nullptr, cpuDescHandle);
		cpuDescHandle.ptr += _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	return S_OK;
}

// 深度バッファーの生成
HRESULT
D3D12Environment::CreateDepthBuffer(UINT bufferWidth, UINT bufferHeight)
{
	HRESULT result = S_OK;

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Width = bufferWidth;
	resDesc.Height = bufferHeight;
	resDesc.DepthOrArraySize = 1;
	resDesc.Format = DXGI_FORMAT_D32_FLOAT;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.MipLevels = 1;
	resDesc.Alignment = 0;

	// 深度値ヒーププロパティ
	auto depthHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	// 深度クリアー値（最大値でクリアー）
	auto depthClearValue = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.0f, 0);

	// 深度バッファー本体
	result = _device->CreateCommittedResource(
		&depthHeapProp, D3D12_HEAP_FLAG_NONE,
		&resDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue, IID_PPV_ARGS(_depthBuffer.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		return result;
	}

	// 深度バッファー用のディスクリプターヒープを作成
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	result = _device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(_dsvHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		return result;
	}

	// 深度ビューを作成
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	_device->CreateDepthStencilView(_depthBuffer.Get(), &dsvDesc, _dsvHeap->GetCPUDescriptorHandleForHeapStart());
	if (FAILED(result)) {
		return result;
	}

	return S_OK;
}


/**
 * コマンドリストの実行
 */
void D3D12Environment::ExecuteCommandLists(UINT numCommandLists, ID3D12CommandList* const* ppCommandLists)
{
	_commandQueue->ExecuteCommandLists(numCommandLists, ppCommandLists);
	_commandQueue->Signal(_fence.Get(), ++_fenceVal);
	if (_fence->GetCompletedValue() != _fenceVal) {
		auto event = CreateEvent(nullptr, false, false, nullptr);
		_fence->SetEventOnCompletion(_fenceVal, event);
		WaitForSingleObject(event, INFINITE);
		CloseHandle(event);
	}

	_commandAllocator->Reset();
}

// 複数のグラフィックスカードを搭載するシステムで特定のアダプターを取得
IDXGIAdapter*
D3D12Environment::FindDXGIAdapter(const std::wstring& key)
{
	std::vector<IDXGIAdapter*> adapters;
	IDXGIAdapter* adapter = nullptr;

	for (int i = 0; _dxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++) {
		adapters.emplace_back(adapter);
	}

	for (auto adpt : adapters) {
		DXGI_ADAPTER_DESC adesc = {};
		adpt->GetDesc(&adesc);
		std::wstring strDesc = adesc.Description;
		if (strDesc.find(key) != std::string::npos) {
			adapter = adpt;
		}
		else {
			adpt->Release();
		}
	}

	return adapter;
}