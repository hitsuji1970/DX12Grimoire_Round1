#pragma once
#include <d3d12.h>
#include <wrl.h>

namespace pmd
{
	class PMDRenderer
	{
	public:
		PMDRenderer();
		virtual ~PMDRenderer();

		HRESULT CreateGraphicsPiplieState(ID3D12Device* const pD3D12Device);
		HRESULT CreateRootSignature(ID3D12Device* const pD3D12Device);

		ID3D12RootSignature* GetRootSingnature()
		{
			return _rootSignature.Get();
		}

		ID3D12PipelineState* GetPipelineState()
		{
			return _pipelineState.Get();
		}

	private:
		// ルートシグネチャー
		Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootSignature;

		// パイプラインステート
		Microsoft::WRL::ComPtr<ID3D12PipelineState> _pipelineState;
	};
}
