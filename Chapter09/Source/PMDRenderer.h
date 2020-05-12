#pragma once
#include <d3d12.h>
#include <wrl.h>

namespace pmd
{
	class PMDRenderer
	{
	public:
		PMDRenderer(ID3D12Device* pD3D12Device);
		virtual ~PMDRenderer();

		ID3D12RootSignature* GetRootSingnature()
		{
			return _rootSignature.Get();
		}

		ID3D12PipelineState* GetPipelineState()
		{
			return _pipelineState.Get();
		}

	private:
		// IAに設定する頂点レイアウト
		static const D3D12_INPUT_ELEMENT_DESC InputLayout[];

		// ルートシグネチャー
		Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootSignature;

		// パイプラインステート
		Microsoft::WRL::ComPtr<ID3D12PipelineState> _pipelineState;

	private:
		// ルートシグネチャーの作成
		HRESULT CreateRootSignature(ID3D12Device* const pD3D12Device);

		// パイプラインステートの作成
		HRESULT CreateGraphicsPiplieState(ID3D12Device* const pD3D12Device);

		// ファイルからのシェーダーコンパイル
		HRESULT CompileShaderFromFile(LPCWSTR pFileName, LPCSTR pEntrypoint, LPCSTR pShaderModel, ID3DBlob** ppByteCode);
	};
}
