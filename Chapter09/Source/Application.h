#pragma once
#include "D3D12Environment.h"

class Application
{
public:
	// 定数
	static constexpr LONG DefaultWindowWidth = 1280;
	static constexpr LONG DefaultWindowHeight = 720;

public:
	Application();
	virtual ~Application();

	/** 初期化処理 */
	HRESULT Initialize();

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

private:
	// ウィンドウの初期化
	HWND InitWindow(WNDCLASSEX* const pWndClass);

	// ウィンドウプロシージャー
	static LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
};