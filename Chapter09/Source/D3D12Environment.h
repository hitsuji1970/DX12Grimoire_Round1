#pragma once

#include <string>
#include <vector>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "DirectXTex.lib")

/**
 * Direct3D12の描画環境
 */
class D3D12Environment
{
private:
	/** Microsoft::WRL::ComPtrの型名を短縮 */
	template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
	/** コンストラクター */
	D3D12Environment();

	/** デストラクター */
	virtual ~D3D12Environment();

	/** 初期化 */
	HRESULT Initialize(HWND hWnd, UINT windowWidth, UINT windowHeight);

	/** コマンドリストの生成 */
	HRESULT CreateCommandList(UINT nodeMask, D3D12_COMMAND_LIST_TYPE type, ID3D12PipelineState* pInitialState, REFIID riid, void** ppCommandList);

	/** コマンドリストの実行 */
	void ExecuteCommandLists(UINT numCommandLists, ID3D12CommandList* const* ppCommandLists);

	/** DirectXインターフェイスの取得 */
	const ComPtr<ID3D12Device>& GetDevice() const
	{
		return _device;
	}

	const ComPtr<IDXGISwapChain4> GetSwapChain() const
	{
		return _swapChain;
	}

	const ComPtr<ID3D12CommandAllocator> GetCommandAllocator() const
	{
		return _cmdAllocator;
	}

	const ComPtr<ID3D12DescriptorHeap> GetRenderTargetViewHeaps() const
	{
		return _rtvHeaps;
	}

	const ComPtr<ID3D12DescriptorHeap> GetDepthStencilViewHeap() const
	{
		return _dsvHeap;
	}

	ID3D12Resource* GetBackBuffer(size_t index)
	{
		return _backBuffers[index];
	}

private:
	/** ディスプレイアダプターのインターフェイス */
	ComPtr<IDXGIFactory6> _dxgiFactory;

	/** DirectXデバイスインターフェイス */
	ComPtr<ID3D12Device> _device;

	/** コマンドアロケーター: コマンドの実体を格納するメモリーのインターフェイス */
	ComPtr<ID3D12CommandAllocator> _cmdAllocator;

	/** コマンドキュー */
	ComPtr<ID3D12CommandQueue> _cmdQueue;

	/** フレームバッファー */
	ComPtr<IDXGISwapChain4> _swapChain;
	ComPtr<ID3D12DescriptorHeap> _rtvHeaps = nullptr;
	std::vector<ID3D12Resource*> _backBuffers;

	/** 深度バッファー */
	ComPtr<ID3D12DescriptorHeap> _dsvHeap = nullptr;
	ComPtr<ID3D12Resource> _depthBuffer = nullptr;

	/** フェンス */
	ComPtr<ID3D12Fence> _fence;
	UINT64 _fenceVal;

private:
	/** スワップチェインとバックバッファーの生成 */
	HRESULT CreateBackBuffers(HWND hWnd, UINT bufferWidth, UINT bufferHight);

	/** 深度バッファーの生成 */
	HRESULT CreateDepthBuffer(UINT bufferWidth, UINT bufferHeight);

	/** デバッグレイヤーの有効化 */
	void EnableDebugLayer();

	/** 複数のグラフィックスカードを搭載するシステムで特定のアダプターを取得 */
	ComPtr<IDXGIAdapter> FindDXGIAdapter(const std::wstring& key);
};
