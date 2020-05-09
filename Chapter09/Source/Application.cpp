#include "Application.h"

Application::Application():
	_hWnd(nullptr)
{
}

Application::~Application()
{
}

/**
 * 初期化処理
 */
HRESULT Application::Initialize()
{
	_hWnd = InitWindow(&_wndClass);
	if (_hWnd == nullptr) {
		return E_FAIL;
	}
	return S_OK;
}

/**
 * 終了処理
 */
void Application::Terminate()
{
	::UnregisterClass(_wndClass.lpszClassName, _wndClass.hInstance);
}

/**
 * ウィンドウの初期化
 */
HWND Application::InitWindow(WNDCLASSEX* const pWndClass)
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

/**
 * ウィンドウプロシージャー
 */
LRESULT Application::WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
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

