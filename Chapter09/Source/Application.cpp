#include "Application.h"

// Windows
#include <Windows.h>

// DirectX
#include <d3d12.h>
#include <d3dx12.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXTex.h>
#include <dxgi1_6.h>

// User
#include "D3D12Environment.h"
#include "utils.h"
#include "PMDMaterial.h"

// モデルデータ読み込みパス
const std::wstring MMDDataPath = L"D:/MikuMikuDance_v932x64";
const std::wstring ModelPath = MMDDataPath + L"/UserFile/Model";
const std::wstring ToonBmpPath = MMDDataPath + L"/Data";

Application::Application() :
	_hWnd(nullptr), _cmdList(nullptr), _rootSignature(nullptr), _pipelineState(nullptr),
	_basicDescHeap(nullptr),
	_viewport{}, _scissorRect{}
{
}

Application::~Application()
{
}

/**
 * 初期化処理
 */
HRESULT Application::Initialize()
{
	HRESULT result = S_OK;

	_hWnd = InitWindow(&_wndClass);
	if (_hWnd == nullptr) {
		return E_FAIL;
	}

	d3d12Env.reset(new D3D12Environment());
	d3d12Env->Initialize(_hWnd, DefaultWindowWidth, DefaultWindowHeight);
	auto pDevice = d3d12Env->GetDevice();

	result = d3d12Env->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, nullptr, IID_PPV_ARGS(_cmdList.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		return result;
	}

	ShowWindow(_hWnd, SW_SHOW);

	// 定数バッファー
	result = pDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer((sizeof(SceneMatrix) + 0xff) & ~0xff),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_constBuff.ReleaseAndGetAddressOf()));
	_constBuff->SetName(L"ConstantBuffer");

	// 行列をコピー
	result = _constBuff->Map(0, nullptr, (void**)&_mappedMatrix);

	// シェーダーリソースビュー
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NodeMask = 0;

	// CBV1つ
	descHeapDesc.NumDescriptors = 1;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	result = pDevice->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(_basicDescHeap.ReleaseAndGetAddressOf()));
	_basicDescHeap->SetName(L"BasicDescHeap");
	auto basicHeapHandle = _basicDescHeap->GetCPUDescriptorHandleForHeapStart();

	// シェーダーリソースビュー（定数バッファー）
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = _constBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = static_cast<UINT>(_constBuff->GetDesc().Width);
	pDevice->CreateConstantBufferView(&cbvDesc, basicHeapHandle);

	result = pmd::PMDMaterial::LoadDefaultTextures(pDevice.Get());
	result = mesh.LoadFromFile(pDevice.Get(), ModelPath + L"/初音ミク.pmd", ToonBmpPath);
	//result = mesh.LoadFromFile(_device, ModelPath + L"/初音ミクmetal.pmd");
	//result = mesh.LoadFromFile(_device, ModelPath + L"/巡音ルカ.pmd");

	// シェーダー
	ComPtr<ID3DBlob> _vsBlob = nullptr;
	ComPtr<ID3DBlob> _psBlob = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	result = D3DCompileFromFile(
		L"Source/BasicVertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicVS", "vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		_vsBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());

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
		_psBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());

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

	// 行列1 + マテリアル1 + マテリアル用テクスチャー
	CD3DX12_DESCRIPTOR_RANGE descTblRanges[3] = {};

	// 変換行列用レジスター0番
	descTblRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

	// マテリアル用レジスター1番
	descTblRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

	// テクスチャー（マテリアルと1対1）
	descTblRanges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, pmd::PMDMesh::NUMBER_OF_TEXTURE, 0);

	CD3DX12_ROOT_PARAMETER rootParams[2] = {};
	rootParams[0].InitAsDescriptorTable(1, &descTblRanges[0]);
	rootParams[1].InitAsDescriptorTable(2, &descTblRanges[1], D3D12_SHADER_VISIBILITY_PIXEL);

	// サンプラー設定
	D3D12_STATIC_SAMPLER_DESC samplerDescs[2] = {};
	samplerDescs[0] = CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_MIN_MAG_MIP_POINT);
	samplerDescs[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	samplerDescs[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplerDescs[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	samplerDescs[1] = CD3DX12_STATIC_SAMPLER_DESC(1);
	samplerDescs[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDescs[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDescs[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

	D3D12_ROOT_SIGNATURE_DESC rootsignatureDesc = {};
	rootsignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootsignatureDesc.pParameters = rootParams;
	rootsignatureDesc.NumParameters = 2;
	rootsignatureDesc.pStaticSamplers = samplerDescs;
	rootsignatureDesc.NumStaticSamplers = 2;

	ID3DBlob* rootSigBlob = nullptr;
	result = D3D12SerializeRootSignature(
		&rootsignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob,
		&errorBlob);

	result = pDevice->CreateRootSignature(
		0,
		rootSigBlob->GetBufferPointer(),
		rootSigBlob->GetBufferSize(),
		IID_PPV_ARGS(_rootSignature.ReleaseAndGetAddressOf()));
	rootSigBlob->Release();

	// パイプラインステート
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc = {};
	pipelineStateDesc.pRootSignature = _rootSignature.Get();
	pipelineStateDesc.VS = CD3DX12_SHADER_BYTECODE(_vsBlob.Get());
	pipelineStateDesc.PS = CD3DX12_SHADER_BYTECODE(_psBlob.Get());
	pipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	pipelineStateDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	pipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	pipelineStateDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	pipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	pipelineStateDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

	D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};
	renderTargetBlendDesc.BlendEnable = false;
	renderTargetBlendDesc.LogicOpEnable = false;
	renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	pipelineStateDesc.BlendState.RenderTarget[0] = renderTargetBlendDesc;
	pipelineStateDesc.InputLayout.pInputElementDescs = pmd::PMDMesh::INPUT_LAYOUT.data();
	pipelineStateDesc.InputLayout.NumElements = static_cast<UINT>(pmd::PMDMesh::INPUT_LAYOUT.size());
	pipelineStateDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateDesc.NumRenderTargets = 1;
	pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	pipelineStateDesc.SampleDesc.Count = 1;
	pipelineStateDesc.SampleDesc.Quality = 0;

	result = pDevice->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(_pipelineState.ReleaseAndGetAddressOf()));
	_pipelineState->SetName(L"PipelineState");

	_viewport.Width = DefaultWindowWidth;
	_viewport.Height = DefaultWindowHeight;
	_viewport.TopLeftX = 0;
	_viewport.TopLeftY = 0;
	_viewport.MaxDepth = 1.0f;
	_viewport.MinDepth = 0.0f;

	_scissorRect.top = 0;
	_scissorRect.left = 0;
	_scissorRect.right = _scissorRect.left + DefaultWindowWidth;
	_scissorRect.bottom = _scissorRect.top + DefaultWindowHeight;

	return S_OK;
}

/**
 * 実行／更新処理
 */
void Application::Run()
{
	MSG msg = {};
	auto angle = 0.0f;

	auto pDevice = d3d12Env->GetDevice();
	auto swapChain = d3d12Env->GetSwapChain();
	auto cmdAllocator = d3d12Env->GetCommandAllocator();
	auto rtvHeaps = d3d12Env->GetRenderTargetViewHeaps();
	auto dsvHeap = d3d12Env->GetDepthStencilViewHeap();

	// 定数バッファーに行列を設定
	DirectX::XMFLOAT3 eye(0, 15, -15);
	DirectX::XMFLOAT3 target(0, 15, 0);
	DirectX::XMFLOAT3 up(0, 1, 0);
	DirectX::XMMATRIX worldMatrix;

	auto viewMatrix = DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat3(&eye), DirectX::XMLoadFloat3(&target), DirectX::XMLoadFloat3(&up));
	auto aspectRatio = static_cast<float>(DefaultWindowWidth) / DefaultWindowHeight;
	auto projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV4, aspectRatio, 1.0f, 100.f);

	_mappedMatrix->view = viewMatrix;
	_mappedMatrix->proj = projectionMatrix;
	_mappedMatrix->viewProj = viewMatrix * projectionMatrix;
	_mappedMatrix->eye = eye;

	while (true) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (msg.message == WM_QUIT) {
			break;
		}

		auto bbIdx = swapChain->GetCurrentBackBufferIndex();
		auto pBackBuffer = d3d12Env->GetBackBuffer(bbIdx);

		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			pBackBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

		_cmdList->SetPipelineState(_pipelineState.Get());

		auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		auto dsvH = dsvHeap->GetCPUDescriptorHandleForHeapStart();
		rtvH.ptr += static_cast<size_t>(bbIdx) * pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		_cmdList->OMSetRenderTargets(1, &rtvH, true, &dsvH);

		float clearColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
		_cmdList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		_cmdList->RSSetViewports(1, &_viewport);
		_cmdList->RSSetScissorRects(1, &_scissorRect);

		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_cmdList->IASetVertexBuffers(0, 1, &mesh.GetVertexBufferView());
		_cmdList->IASetIndexBuffer(&mesh.GetIndexBufferView());

		_cmdList->SetGraphicsRootSignature(_rootSignature.Get());

		ID3D12DescriptorHeap* descHeaps[] = { _basicDescHeap.Get() };
		_cmdList->SetDescriptorHeaps(1, descHeaps);
		_cmdList->SetGraphicsRootDescriptorTable(0, _basicDescHeap->GetGPUDescriptorHandleForHeapStart());

		ID3D12DescriptorHeap* materialDescHeap[] = { mesh.GetMaterialDescriptorHeap() };
		_cmdList->SetDescriptorHeaps(1, materialDescHeap);

		auto materialH = materialDescHeap[0]->GetGPUDescriptorHandleForHeapStart();
		auto cbvsrvIncSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cbvsrvIncSize *= (1 + pmd::PMDMesh::NUMBER_OF_TEXTURE);
		unsigned int idxOffset = 0;
		for (auto& material : mesh.GetMaterials()) {
			_cmdList->SetGraphicsRootDescriptorTable(1, materialH);
			_cmdList->DrawIndexedInstanced(material.GetIndicesNum(), 1, idxOffset, 0, 0);
			materialH.ptr += cbvsrvIncSize;
			idxOffset += material.GetIndicesNum();
		}

		angle += 0.01f;
		worldMatrix = DirectX::XMMatrixRotationY(angle);
		_mappedMatrix->world = worldMatrix;

		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			pBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

		_cmdList->Close();

		ID3D12CommandList* cmdLists[] = { _cmdList.Get() };
		d3d12Env->ExecuteCommandLists(1, cmdLists);
		//_cmdList->Reset(cmdAllocator, nullptr);
		_cmdList->Reset(cmdAllocator.Get(), nullptr);

		swapChain->Present(1, 0);
	}

}

/**
 * 終了処理
 */
void Application::Terminate()
{
	pmd::PMDMaterial::ReleaseDefaultTextures();

	ID3D12DebugDevice* debugInterface;
	if (SUCCEEDED(d3d12Env->GetDevice()->QueryInterface(&debugInterface)))
	{
		debugInterface->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
		debugInterface->Release();
	}

	::UnregisterClass(_wndClass.lpszClassName, _wndClass.hInstance);
}

/**
 * ウィンドウの初期化
 */
HWND Application::InitWindow(WNDCLASSEX* const pWndClass)
{
	pWndClass->cbSize = sizeof(WNDCLASSEX);
	pWndClass->lpfnWndProc = WNDPROC(WindowProcedure);
	pWndClass->lpszClassName = TEXT("DX12Sample");
	pWndClass->hInstance = GetModuleHandle(nullptr);

	::RegisterClassEx(pWndClass);

	RECT wrc = { 0, 0, DefaultWindowWidth, DefaultWindowHeight };
	::AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	HWND hWnd = ::CreateWindow(pWndClass->lpszClassName,
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

/**
 * ウィンドウプロシージャー
 */
LRESULT Application::WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_KEYDOWN:
		switch (wparam)
		{
		case VK_ESCAPE:
			PostQuitMessage(0);
			return 0;
		default:
			break;
		}
	default:
		break;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

