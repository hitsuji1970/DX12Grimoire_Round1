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
	_hWnd(nullptr), _basicDescHeap(nullptr),
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

	_pmdRenderer.reset(new pmd::PMDRenderer());
	_pmdRenderer->CreateRootSignature(d3d12Env->GetDevice().Get());
	_pmdRenderer->CreateGraphicsPiplieState(d3d12Env->GetDevice().Get());

	result = pmd::PMDMaterial::LoadDefaultTextures(pDevice.Get());
	result = mesh.LoadFromFile(pDevice.Get(), ModelPath + L"/初音ミク.pmd", ToonBmpPath);
	//result = mesh.LoadFromFile(_device, ModelPath + L"/初音ミクmetal.pmd");
	//result = mesh.LoadFromFile(_device, ModelPath + L"/巡音ルカ.pmd");

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

	auto commandList = d3d12Env->GetCommandList();

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

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			pBackBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

		//commandList->SetPipelineState(_pipelineState.Get());
		commandList->SetPipelineState(_pmdRenderer->GetPipelineState());

		auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		auto dsvH = dsvHeap->GetCPUDescriptorHandleForHeapStart();
		rtvH.ptr += static_cast<size_t>(bbIdx) * pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		commandList->OMSetRenderTargets(1, &rtvH, true, &dsvH);

		float clearColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		commandList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
		commandList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		commandList->RSSetViewports(1, &_viewport);
		commandList->RSSetScissorRects(1, &_scissorRect);

		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->IASetVertexBuffers(0, 1, &mesh.GetVertexBufferView());
		commandList->IASetIndexBuffer(&mesh.GetIndexBufferView());

		//commandList->SetGraphicsRootSignature(_rootSignature.Get());
		commandList->SetGraphicsRootSignature(_pmdRenderer->GetRootSingnature());

		ID3D12DescriptorHeap* descHeaps[] = { _basicDescHeap.Get() };
		commandList->SetDescriptorHeaps(1, descHeaps);
		commandList->SetGraphicsRootDescriptorTable(0, _basicDescHeap->GetGPUDescriptorHandleForHeapStart());

		ID3D12DescriptorHeap* materialDescHeap[] = { mesh.GetMaterialDescriptorHeap() };
		commandList->SetDescriptorHeaps(1, materialDescHeap);

		auto materialH = materialDescHeap[0]->GetGPUDescriptorHandleForHeapStart();
		auto cbvsrvIncSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cbvsrvIncSize *= (1 + pmd::PMDMesh::NUMBER_OF_TEXTURE);
		unsigned int idxOffset = 0;
		for (auto& material : mesh.GetMaterials()) {
			commandList->SetGraphicsRootDescriptorTable(1, materialH);
			commandList->DrawIndexedInstanced(material.GetIndicesNum(), 1, idxOffset, 0, 0);
			materialH.ptr += cbvsrvIncSize;
			idxOffset += material.GetIndicesNum();
		}

		angle += 0.01f;
		worldMatrix = DirectX::XMMatrixRotationY(angle);
		_mappedMatrix->world = worldMatrix;

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			pBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

		commandList->Close();

		ID3D12CommandList* cmdLists[] = { commandList.Get() };
		d3d12Env->ExecuteCommandLists(1, cmdLists);
		//_cmdList->Reset(cmdAllocator, nullptr);
		commandList->Reset(cmdAllocator.Get(), nullptr);

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

