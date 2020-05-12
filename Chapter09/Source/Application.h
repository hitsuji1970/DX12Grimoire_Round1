#pragma once

#include <memory>
#include <wrl.h>

#include "D3D12Environment.h"
#include "PMDActor.h"
#include "PMDRenderer.h"

// シェーダーに渡す行列
struct SceneMatrix
{
	DirectX::XMMATRIX world;
	DirectX::XMMATRIX view;
	DirectX::XMMATRIX proj;
	DirectX::XMMATRIX viewProj;
	DirectX::XMFLOAT3 eye;
};

class Application
{
private:
	/** Microsoft::WRL::ComPtrの型名を短縮 */
	template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
	// 定数
	static constexpr LONG DefaultWindowWidth = 1280;
	static constexpr LONG DefaultWindowHeight = 720;

public:
	Application();
	virtual ~Application();

	/** 初期化処理 */
	HRESULT Initialize();

	/** 実行と更新 */
	void Run();

	/** 終了処理 */
	void Terminate();

	/** ウィンドウハンドルの取得 */
	const HWND GetWindowHandle() const
	{
		return _hWnd;
	}

private:
	// ウィンドウハンドル
	HWND _hWnd;

	// ウィンドウクラス
	WNDCLASSEX _wndClass;

	// DirectX 12描画環境
	std::unique_ptr<D3D12Environment> _d3d12Env;

	ComPtr<ID3D12DescriptorHeap> _basicDescHeap;
	ComPtr<ID3D12Resource> _constBuff;
	SceneMatrix* _mappedMatrix;


	std::unique_ptr<pmd::PMDRenderer> _pmdRenderer;


	// PMDモデル
	pmd::PMDActor mesh;


private:
	// ウィンドウの初期化
	HWND InitWindow(WNDCLASSEX* const pWndClass);

	// ウィンドウプロシージャー
	static LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
};