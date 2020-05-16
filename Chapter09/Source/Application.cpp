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

// PMDモデルファイル名
const std::wstring ModelPath = L"MMD/UserFile/Model";
const std::wstring ModelFile = ModelPath + L"/初音ミク.pmd";
//const std::wstring ModelFile = ModelPath + L"/MMD-Nanami/Nanami.pmd";
//const std::wstring ModelFile = ModelPath + L"/MMD-Claudia/Claudia.pmd";

// トゥーンシェーディング用テクスチャー読み込みパス
const std::wstring ToonBmpPath = L"MMD/Data";

// コンストラクター
Application::Application() :
	_hWnd(nullptr), _wndClass(), _d3d12Env(nullptr), _sceneMatrixDescHeap(nullptr),
	_sceneMatrixConstantBuffer(nullptr), _mappedMatrix(nullptr), _pmdRenderer(nullptr)
{
}

// デストラクター
Application::~Application()
{
}

// 初期化処理
HRESULT
Application::Initialize()
{
	HRESULT result = S_OK;

	_hWnd = InitWindow(&_wndClass);
	if (_hWnd == nullptr) {
		return E_FAIL;
	}

	// Direct3D12描画環境の初期化
	_d3d12Env.reset(new D3D12Environment());
	result = _d3d12Env->Initialize(_hWnd, DefaultWindowWidth, DefaultWindowHeight);
	if (FAILED(result))
	{
		return result;
	}
	auto pDevice = _d3d12Env->GetDevice();

	// リソースキャッシュ
	_resourceCache.reset(new D3D12ResourceCache(pDevice.Get()));

	// ウィンドウの表示を開始
	ShowWindow(_hWnd, SW_SHOW);

	// 定数バッファー
	result = pDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer((sizeof(SceneMatrix) + 0xff) & ~0xff),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_sceneMatrixConstantBuffer.ReleaseAndGetAddressOf()));
	_sceneMatrixConstantBuffer->SetName(L"ConstantBuffer");

	// 行列をコピー
	result = _sceneMatrixConstantBuffer->Map(0, nullptr, (void**)&_mappedMatrix);

	// シェーダーリソースビュー
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NodeMask = 0;

	// CBV1つ
	descHeapDesc.NumDescriptors = 1;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	result = pDevice->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(_sceneMatrixDescHeap.ReleaseAndGetAddressOf()));
	_sceneMatrixDescHeap->SetName(L"BasicDescHeap");
	auto basicHeapHandle = _sceneMatrixDescHeap->GetCPUDescriptorHandleForHeapStart();

	// シェーダーリソースビュー（定数バッファー）
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = _sceneMatrixConstantBuffer->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = static_cast<UINT>(_sceneMatrixConstantBuffer->GetDesc().Width);
	pDevice->CreateConstantBufferView(&cbvDesc, basicHeapHandle);

	// PMD共通描画環境の初期化
	_pmdRenderer.reset(new pmd::PMDRenderer(_d3d12Env->GetDevice().Get()));

	// PMDモデルの初期化
	_pmdActor.reset(new pmd::PMDActor());
	result = _pmdActor->LoadFromFile(pDevice.Get(), _resourceCache.get(), ModelFile, ToonBmpPath);

	return S_OK;
}

// 実行／更新処理
void
Application::Run()
{
	MSG msg = {};

	auto pDevice = _d3d12Env->GetDevice();

	// 定数バッファーに行列を設定
	DirectX::XMFLOAT3 eye(0, 15, -15);
	DirectX::XMFLOAT3 target(0, 15, 0);
	DirectX::XMFLOAT3 up(0, 1, 0);

	auto viewMatrix = DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat3(&eye), DirectX::XMLoadFloat3(&target), DirectX::XMLoadFloat3(&up));
	auto aspectRatio = static_cast<float>(DefaultWindowWidth) / DefaultWindowHeight;
	auto projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV4, aspectRatio, 1.0f, 100.f);

	_mappedMatrix->view = viewMatrix;
	_mappedMatrix->proj = projectionMatrix;
	_mappedMatrix->viewProj = viewMatrix * projectionMatrix;
	_mappedMatrix->eye = eye;

	auto commandList = _d3d12Env->GetCommandList();

	while (true) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (msg.message == WM_QUIT) {
			break;
		}

		_d3d12Env->BeginDraw();

		ID3D12DescriptorHeap* descHeaps[] = { _sceneMatrixDescHeap.Get() };
		commandList->SetDescriptorHeaps(1, descHeaps);
		commandList->SetGraphicsRootSignature(_pmdRenderer->GetRootSingnature());
		commandList->SetPipelineState(_pmdRenderer->GetPipelineState());
		commandList->SetGraphicsRootDescriptorTable(0, _sceneMatrixDescHeap->GetGPUDescriptorHandleForHeapStart());
		_pmdActor->Update();
		_pmdActor->Draw(pDevice.Get(), commandList.Get());

		_d3d12Env->EndDraw();
	}
}

// 終了処理
void
Application::Terminate()
{
	ID3D12DebugDevice* debugInterface;
	if (SUCCEEDED(_d3d12Env->GetDevice()->QueryInterface(&debugInterface)))
	{
		debugInterface->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
		debugInterface->Release();
	}

	::UnregisterClass(_wndClass.lpszClassName, _wndClass.hInstance);
}

// ウィンドウの初期化
HWND
Application::InitWindow(WNDCLASSEX* const pWndClass)
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

// ウィンドウプロシージャー
LRESULT
Application::WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
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

