#pragma once

#include <memory>
#include <wrl.h>

#include "D3D12Environment.h"
#include "D3D12ResourceCache.h"
#include "PMDActor.h"
#include "PMDRenderer.h"

// シェーダーに渡す行列
struct SceneMatrix
{
	DirectX::XMMATRIX view;
	DirectX::XMMATRIX proj;
	DirectX::XMMATRIX viewProj;
	DirectX::XMFLOAT3 eye;
};

class Application
{
public:
	// 定数
	static constexpr LONG DefaultWindowWidth = 1280;
	static constexpr LONG DefaultWindowHeight = 720;

public:
	Application();
	virtual ~Application();

	// 初期化処理
	HRESULT Initialize();

	// 実行と更新
	void Run();

	// 終了処理
	void Terminate();

	// ウィンドウハンドルの取得
	const HWND GetWindowHandle() const
	{
		return _hWnd;
	}

private:
	// ウィンドウハンドル
	HWND _hWnd;

	// ウィンドウクラス
	WNDCLASSEX _wndClass;

	// DirectX12描画環境
	std::unique_ptr<D3D12Environment> _d3d12Env;

	// DirectX12リソースキャッシュ
	std::unique_ptr<D3D12ResourceCache> _resourceCache;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _sceneMatrixDescHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> _sceneMatrixConstantBuffer;
	SceneMatrix* _mappedMatrix;

	// PMDモデル描画オブジェクト
	std::unique_ptr<pmd::PMDRenderer> _pmdRenderer;
	std::unique_ptr<pmd::PMDActor> _pmdActor;

private:
	// ウィンドウの初期化
	HWND InitWindow(WNDCLASSEX* const pWndClass);

	// ウィンドウプロシージャー
	static LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
};