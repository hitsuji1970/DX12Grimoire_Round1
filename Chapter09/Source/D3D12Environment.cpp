#include "D3D12Environment.h"
#include <d3dx12.h>
#include <string>
#include <vector>

/**
 *�R���X�g���N�^�[
 */
D3D12Environment::D3D12Environment() :
	_dxgiFactory(nullptr), _device(nullptr), _swapChain(nullptr),
	_rtvHeaps(nullptr), _dsvHeap(nullptr), _backBuffers{}, _depthBuffer(nullptr),
	_cmdAllocator(nullptr), _cmdQueue(nullptr), _fence(nullptr), _fenceVal(0)
{
}

/**
 * �f�X�g���N�^�[
 */
D3D12Environment::~D3D12Environment()
{
}

/**
 * ������
 */
HRESULT D3D12Environment::Initialize(HWND hWnd, UINT windowWidth, UINT windowHeight)
{
	HRESULT result = S_OK;

	// DXGI
#ifdef _DEBUG
	EnableDebugLayer();
	if (FAILED(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(_dxgiFactory.ReleaseAndGetAddressOf())))) {
		if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(_dxgiFactory.ReleaseAndGetAddressOf())))) {
			return -1;
		}
	}
#else
	if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(_dxgiFactory.ReleaseAndGetAddressOf())))) {
		return -1;
	}
#endif // _DEBUG

	// �O���t�B�b�N�X�J�[�h���T�|�[�g����@�\���x��
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	// �m�[�gPC�p��NVIDIA�̃O���t�B�b�N�X��D�悵�Č��o
	auto adapter = FindDXGIAdapter(L"NVIDIA");
	D3D_FEATURE_LEVEL featureLevel;
	for (auto lv : levels) {
		if (D3D12CreateDevice(adapter.Get(), lv, IID_PPV_ARGS(_device.ReleaseAndGetAddressOf())) == S_OK) {
			featureLevel = lv;
			break;
		}
	}
	if (!_device) {
		::MessageBox(hWnd, TEXT("Error"), TEXT("D3D�f�o�C�X�̏������Ɏ��s���܂����B"), MB_ICONERROR);
		exit(-1);
	}

	// �R�}���h�A���P�[�^�[
	result = _device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(_cmdAllocator.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		return result;
	}

	// �R�}���h���X�g

	// �R�}���h�L���[
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	result = _device->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(_cmdQueue.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		return result;
	}

	// �X���b�v�`�F�C���ƃo�b�N�o�b�t�@�[
	CreateBackBuffers(hWnd, windowWidth, windowHeight);
	CreateDepthBuffer(windowWidth, windowHeight);

	// �t�F���X
	result = _device->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(_fence.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		return result;
	}

	return S_OK;
}

/**
 * �R�}���h���X�g�C���^�[�t�F�C�X�I�u�W�F�N�g�̐���
 */
HRESULT D3D12Environment::CreateCommandList(
	UINT nodeMask,
	D3D12_COMMAND_LIST_TYPE type,
	ID3D12PipelineState* pInitialState,
	REFIID riid,
	void** ppCommandList)
{
	return _device->CreateCommandList(nodeMask, type, _cmdAllocator.Get(), pInitialState, riid, ppCommandList);
}

/**
 * �R�}���h���X�g�̎��s
 */
void D3D12Environment::ExecuteCommandLists(UINT numCommandLists, ID3D12CommandList* const* ppCommandLists)
{
	_cmdQueue->ExecuteCommandLists(numCommandLists, ppCommandLists);
	_cmdQueue->Signal(_fence.Get(), ++_fenceVal);
	if (_fence->GetCompletedValue() != _fenceVal) {
		auto event = CreateEvent(nullptr, false, false, nullptr);
		_fence->SetEventOnCompletion(_fenceVal, event);
		WaitForSingleObject(event, INFINITE);
		CloseHandle(event);
	}
	
	_cmdAllocator->Reset();
}

/**
 * �X���b�v�`�F�C���ƃo�b�N�o�b�t�@�[�̐���
 */
HRESULT D3D12Environment::CreateBackBuffers(HWND hWnd, UINT bufferWidth, UINT bufferHeight)
{
	HRESULT result;

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
	result = _dxgiFactory->CreateSwapChainForHwnd(
		_cmdQueue.Get(), hWnd, &swapChainDesc, nullptr, nullptr,
		(IDXGISwapChain1**)_swapChain.ReleaseAndGetAddressOf());
	if (FAILED(result)) {
		return result;
	}

	// �f�B�X�N���v�^�[�q�[�v
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	result = _device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_rtvHeaps.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		return result;
	}

	// �X���b�v�`�F�C���Ɋ֘A�t��
	_backBuffers.resize(swapChainDesc.BufferCount);
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle = _rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	for (auto idx = 0u; idx < swapChainDesc.BufferCount; ++idx) {
		result = _swapChain->GetBuffer(idx, IID_PPV_ARGS(&_backBuffers[idx]));
		_device->CreateRenderTargetView(_backBuffers[idx], nullptr, cpuDescHandle);
		cpuDescHandle.ptr += _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}
	if (FAILED(result)) {
		return result;
	}

	return S_OK;
}

/**
 * �[�x�o�b�t�@�[�̐���
 */
HRESULT D3D12Environment::CreateDepthBuffer(UINT bufferWidth, UINT bufferHeight)
{
	HRESULT result = S_OK;

	D3D12_RESOURCE_DESC depthResDesc = {};
	depthResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthResDesc.Width = bufferWidth;
	depthResDesc.Height = bufferHeight;
	depthResDesc.DepthOrArraySize = 1;
	depthResDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthResDesc.SampleDesc.Count = 1;
	depthResDesc.SampleDesc.Quality = 0;
	depthResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	depthResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthResDesc.MipLevels = 1;
	depthResDesc.Alignment = 0;

	// �[�x�l�q�[�v�v���p�e�B
	D3D12_HEAP_PROPERTIES depthHeapProp = {};
	depthHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	depthHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	depthHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	// �[�x�N���A�[�l�i�ő�l�ŃN���A�[�j
	D3D12_CLEAR_VALUE depthClearValue = {};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;

	// �[�x�o�b�t�@�[�{��
	result = _device->CreateCommittedResource(
		&depthHeapProp, D3D12_HEAP_FLAG_NONE,
		&depthResDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue, IID_PPV_ARGS(_depthBuffer.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		return result;
	}

	// �[�x�o�b�t�@�[�p�̃f�B�X�N���v�^�[�q�[�v
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	result = _device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(_dsvHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		return result;
	}

	// �[�x�r���[
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	_device->CreateDepthStencilView(_depthBuffer.Get(), &dsvDesc, _dsvHeap->GetCPUDescriptorHandleForHeapStart());

	return S_OK;
}

/**
 * �f�o�b�O���C���[�̐ݒ�
 */
void D3D12Environment::EnableDebugLayer()
{
	ID3D12Debug* debugLayer = nullptr;
	auto result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));
	debugLayer->EnableDebugLayer();
	debugLayer->Release();
}

/**
 * �O���t�B�b�N�X�J�[�h�̑I��
 */
Microsoft::WRL::ComPtr<IDXGIAdapter> D3D12Environment::FindDXGIAdapter(const std::wstring& key)
{
	std::vector<ComPtr<IDXGIAdapter>> adapters;
	ComPtr<IDXGIAdapter> tmpAdapter = nullptr;
	ComPtr<IDXGIAdapter> adapter = nullptr;

	for (int i = 0; _dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; i++) {
		adapters.push_back(tmpAdapter);
	}

	for (auto adpt : adapters) {
		DXGI_ADAPTER_DESC adesc = {};
		adpt->GetDesc(&adesc);
		std::wstring strDesc = adesc.Description;
		if (strDesc.find(key) != std::string::npos) {
			adapter = adpt;
			break;
		}
	}

	return adapter;
}