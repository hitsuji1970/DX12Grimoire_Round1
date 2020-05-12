﻿#include "PMDRenderer.h"

#include <d3dcompiler.h>
#include <d3dx12.h>

namespace pmd
{
	template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

	// PMD頂点レイアウト
	const D3D12_INPUT_ELEMENT_DESC INPUT_LAYOUT[] = {
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
	PMDRenderer::PMDRenderer() :
		_rootSignature(nullptr), _pipelineState(nullptr)
	{
	}

	// デストラクター
	PMDRenderer::~PMDRenderer()
	{
	}

	// PMDモデル描画用のグラフィックスパイプラインステートを作成
	HRESULT PMDRenderer::CreateGraphicsPiplieState(ID3D12Device* const pD3D12Device)
	{
		HRESULT result;

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
		pipelineStateDesc.InputLayout.pInputElementDescs = INPUT_LAYOUT;
		pipelineStateDesc.InputLayout.NumElements = static_cast<UINT>(sizeof(INPUT_LAYOUT) / sizeof(INPUT_LAYOUT[0]));
		pipelineStateDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
		pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateDesc.NumRenderTargets = 1;
		pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		pipelineStateDesc.SampleDesc.Count = 1;
		pipelineStateDesc.SampleDesc.Quality = 0;

		result = pD3D12Device->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(_pipelineState.ReleaseAndGetAddressOf()));
		if (FAILED(result))
		{
			return result;
		}
		_pipelineState->SetName(L"PipelineState");

		return S_OK;
	}

	// ルートシグネチャーの作成
	HRESULT PMDRenderer::CreateRootSignature(ID3D12Device* const pD3D12Device)
	{
		HRESULT result;

		// レンジ: テクスチャーと定数の2つ
		CD3DX12_DESCRIPTOR_RANGE descTblRanges[3] = {};
		descTblRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); // 定数[b0]: ビュープロジェクション用
		descTblRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1); // 定数[b1]: マテリアル用
		descTblRanges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0); // テクスチャー4つ（diffuse, sph, spa, toon）

		CD3DX12_ROOT_PARAMETER rootParams[2] = {};
		// descTableRanges[0]から1つ
		rootParams[0].InitAsDescriptorTable(1, &descTblRanges[0]);
		// descTableRanges[1]から2つ
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

		ComPtr<ID3DBlob> rootSigBlob = nullptr;
		ComPtr<ID3DBlob> errorBlob = nullptr;
		result = D3D12SerializeRootSignature(
			&rootsignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0,
			rootSigBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
		if (FAILED(result))
		{
			return result;
		}

		result = pD3D12Device->CreateRootSignature(
			0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(),
			IID_PPV_ARGS(_rootSignature.ReleaseAndGetAddressOf()));
		if (FAILED(result)) {
			return result;
		}

		return S_OK;
	}
}
