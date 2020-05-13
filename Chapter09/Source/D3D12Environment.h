#pragma once

#include <string>
#include <vector>

#include <d3d12.h>
#include <dxgi1_4.h>
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
	D3D12Environment();
	virtual ~D3D12Environment();

	// 初期化
	HRESULT Initialize(HWND hWnd, UINT windowWidth, UINT windowHeight);

	// 描画の開始と終了
	void BeginDraw();
	void EndDraw();

	// DirectXインターフェイスの取得
	const ComPtr<ID3D12Device>& GetDevice() const
	{
		return _device;
	}

	const ComPtr<ID3D12GraphicsCommandList> GetCommandList() const
	{
		return _commandList;
	}

private:
	// ディスプレイアダプターのインターフェイス
	ComPtr<IDXGIFactory4> _dxgiFactory;
	ComPtr<IDXGISwapChain3> _swapChain;

	// DirectXデバイスインターフェイス
	ComPtr<ID3D12Device> _device;

	// ディスプレイアダプターがサポートしている機能レベル
	D3D_FEATURE_LEVEL _featureLevel;

	// フレームバッファー
	ComPtr<ID3D12DescriptorHeap> _rtvHeaps = nullptr;
	std::vector<ID3D12Resource*> _backBuffers;

	// 深度バッファー
	ComPtr<ID3D12DescriptorHeap> _dsvHeap = nullptr;
	ComPtr<ID3D12Resource> _depthBuffer = nullptr;

	// コマンドアロケーター: コマンドの実体を格納するメモリーのインターフェイス
	ComPtr<ID3D12CommandAllocator> _commandAllocator;

	// コマンドリスト
	ComPtr<ID3D12GraphicsCommandList> _commandList;

	// コマンドキュー
	ComPtr<ID3D12CommandQueue> _commandQueue;

	// フェンス
	ComPtr<ID3D12Fence> _fence;
	UINT64 _fenceVal;

	// ビューポート
	D3D12_VIEWPORT _viewport = {};

	// シザー矩形
	D3D12_RECT _scissorRect = {};

private:
	HRESULT InitializeDXGIDevice(HWND hWnd);
	HRESULT CreateSwapChain(HWND hWnd, UINT bufferWidth, UINT bufferHeight);
	HRESULT CreateCommandBuffers();
	HRESULT CreateBackBuffers(HWND hWnd, UINT bufferWidth, UINT bufferHight);
	HRESULT CreateDepthBuffer(UINT bufferWidth, UINT bufferHeight);

	// 複数のグラフィックスカードを搭載するシステムで特定のアダプターを取得
	IDXGIAdapter* FindDXGIAdapter(const std::wstring& key);
};
