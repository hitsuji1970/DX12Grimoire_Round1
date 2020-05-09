#include <Windows.h>
#include <tchar.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <DirectXTex.h>
#include <d3dx12.h>
#include <d3dcompiler.h>
#include <wrl.h>
#include <vector>

#include "Application.h"
#include "D3D12Environment.h"
#include "utils.h"

using namespace Microsoft::WRL;

#ifdef _DEBUG
#include <iostream>
#endif

#include "pmd.h"

// モデルデータ読み込みパス
const std::wstring MMDDataPath = L"D:/MikuMikuDance_v932x64";
const std::wstring ModelPath = MMDDataPath + L"/UserFile/Model";
const std::wstring ToonBmpPath = MMDDataPath + L"/Data";

// 関数プロトタイプ
HWND InitWindow(WNDCLASSEX* const pWndClass);
LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

// シェーダーに渡す行列
struct SceneMatrix
{
	DirectX::XMMATRIX world;
	DirectX::XMMATRIX view;
	DirectX::XMMATRIX proj;
	DirectX::XMMATRIX viewProj;
	DirectX::XMFLOAT3 eye;
};

/** コマンドリスト */
ComPtr<ID3D12GraphicsCommandList> _cmdList;

#ifdef _DEBUG
int main()
#else
int WINAPI _tWinMain(HINSTANCE, HINSTANCE, LPTSTR, int)
#endif
{
	DebugOutputFromString("toon shading test.");

	HRESULT result = S_OK;

	Application app;
	D3D12Environment d3d12Env;

	app.Initialize();
	d3d12Env.Initialize(app.GetWindowHandle(), app.DefaultWindowWidth, app.DefaultWindowHeight);
	auto pDevice = d3d12Env.GetDevice();
	auto cmdAllocator = d3d12Env.GetCommandAllocator();

	result = d3d12Env.CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,  nullptr, IID_PPV_ARGS(_cmdList.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		return result;
	}

	ShowWindow(app.GetWindowHandle(), SW_SHOW);

	// 深度バッファー
	// 定数バッファーに行列を設定
	auto worldMatrix = DirectX::XMMatrixRotationY(DirectX::XM_PIDIV4);
	DirectX::XMFLOAT3 eye(0, 15, -15);
	DirectX::XMFLOAT3 target(0, 15, 0);
	DirectX::XMFLOAT3 up(0, 1, 0);

	auto viewMatrix = DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat3(&eye), DirectX::XMLoadFloat3(&target), DirectX::XMLoadFloat3(&up));
	auto aspectRatio = static_cast<float>(app.DefaultWindowWidth) / app.DefaultWindowHeight;
	auto projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV4, aspectRatio, 1.0f, 100.f);

	// 定数バッファー
	ComPtr<ID3D12Resource> constBuff = nullptr;
	result = pDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer((sizeof(SceneMatrix) + 0xff) & ~0xff),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(constBuff.ReleaseAndGetAddressOf()));

	// 行列をコピー
	SceneMatrix* mapMatrix;
	result = constBuff->Map(0, nullptr, (void**)&mapMatrix);

	// シェーダーリソースビュー
	ComPtr<ID3D12DescriptorHeap> basicDescHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NodeMask = 0;

	// CBV1つ
	descHeapDesc.NumDescriptors = 1;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	result = pDevice->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(basicDescHeap.ReleaseAndGetAddressOf()));
	auto basicHeapHandle = basicDescHeap->GetCPUDescriptorHandleForHeapStart();

	// シェーダーリソースビュー（定数バッファー）
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = constBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = static_cast<UINT>(constBuff->GetDesc().Width);
	pDevice->CreateConstantBufferView(&cbvDesc, basicHeapHandle);

	pmd::PMDMesh mesh;
	result = mesh.LoadFromFile(pDevice, ModelPath + L"/初音ミク.pmd", ToonBmpPath);
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

	ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	result = pDevice->CreateRootSignature(
		0,
		rootSigBlob->GetBufferPointer(),
		rootSigBlob->GetBufferSize(),
		IID_PPV_ARGS(rootSignature.ReleaseAndGetAddressOf()));
	rootSigBlob->Release();

	// パイプラインステート
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc = {};
	pipelineStateDesc.pRootSignature = rootSignature.Get();
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

	ComPtr<ID3D12PipelineState> _pipelineState = nullptr;
	result = pDevice->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(_pipelineState.ReleaseAndGetAddressOf()));

	D3D12_VIEWPORT viewport = {};
	viewport.Width = app.DefaultWindowWidth;
	viewport.Height = app.DefaultWindowHeight;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MaxDepth = 1.0f;
	viewport.MinDepth = 0.0f;

	D3D12_RECT scissorRect = {};
	scissorRect.top = 0;
	scissorRect.left = 0;
	scissorRect.right = scissorRect.left + app.DefaultWindowWidth;
	scissorRect.bottom = scissorRect.top + app.DefaultWindowHeight;

	MSG msg = {};
	auto angle = 0.0f;

	auto swapChain = d3d12Env.GetSwapChain();
	auto rtvHeaps = d3d12Env.GetRenderTargetViewHeaps();
	auto dsvHeap = d3d12Env.GetDepthStencilViewHeap();

	while (true) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (msg.message == WM_QUIT) {
			break;
		}

		auto bbIdx = swapChain->GetCurrentBackBufferIndex();
		auto pBackBuffer = d3d12Env.GetBackBuffer(bbIdx);

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

		_cmdList->RSSetViewports(1, &viewport);
		_cmdList->RSSetScissorRects(1, &scissorRect);

		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_cmdList->IASetVertexBuffers(0, 1, &mesh.GetVertexBufferView());
		_cmdList->IASetIndexBuffer(&mesh.GetIndexBufferView());

		_cmdList->SetGraphicsRootSignature(rootSignature.Get());

		ID3D12DescriptorHeap* descHeaps[] = { basicDescHeap.Get() };
		_cmdList->SetDescriptorHeaps(1, descHeaps);
		_cmdList->SetGraphicsRootDescriptorTable(0, basicDescHeap->GetGPUDescriptorHandleForHeapStart());

		ID3D12DescriptorHeap* materialDescHeap[] = { mesh.GetMaterialDescriptorHeap() };
		_cmdList->SetDescriptorHeaps(1, materialDescHeap);

		auto materialH = materialDescHeap[0]->GetGPUDescriptorHandleForHeapStart();
		auto cbvsrvIncSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cbvsrvIncSize *= (1 + pmd::PMDMesh::NUMBER_OF_TEXTURE);
		unsigned int idxOffset = 0;
		for (auto& m : mesh.GetMaterials()) {
			_cmdList->SetGraphicsRootDescriptorTable(1, materialH);
			_cmdList->DrawIndexedInstanced(m.indicesNum, 1, idxOffset, 0, 0);
			materialH.ptr += cbvsrvIncSize;
			idxOffset += m.indicesNum;
		}

		angle += 0.01f;
		worldMatrix = DirectX::XMMatrixRotationY(angle);
		mapMatrix->world = worldMatrix;
		mapMatrix->view = viewMatrix;
		mapMatrix->proj = projectionMatrix;
		mapMatrix->viewProj = viewMatrix * projectionMatrix;
		mapMatrix->eye = eye;

		_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			pBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

		_cmdList->Close();

		ID3D12CommandList* cmdLists[] = { _cmdList.Get() };
		d3d12Env.ExecuteCommandLists(1, cmdLists);
		_cmdList->Reset(cmdAllocator.Get(), nullptr);

		swapChain->Present(1, 0);
	}

	app.Terminate();

	return 0;
}

