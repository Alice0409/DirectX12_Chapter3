#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <tchar.h>
#ifdef _DEBUG
#include <iostream>
#include <vector>
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#endif

using namespace std;

// @brief コンソール画面にフォーマット付き文字列を表示
// @param format フォーマット (%d とか %f とかの)
// @param 可変長引数
// @remarks この関数はデバッグ用です。デバッグ時にしか動作しない
void DebugOutputFormatString(const char* format, ...)
{
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	vprintf(format, valist);
	va_end(valist);
#endif
}

// 面倒だけど書かなければいけない関数
LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	// ウィンドウが破棄されたら呼ばれる
	if (msg == WM_DESTROY)
	{
		PostQuitMessage(0); // OS に対して「もうこのアプリは終わる」と伝える
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam); // 既定の処理を行う
}

const unsigned int window_width = 1280;
const unsigned int window_height = 720;

ID3D12Device* _dev = nullptr;
IDXGIFactory6* _dxgiFactory = nullptr;
IDXGISwapChain4* _swapchain = nullptr;

/*HRESULT D3D12CreateDevice(
	IUnknown* 　　　　pAdapter, // ひとまずは nullptr で OK
	D3D_FEATURE_LEVEL MinimumFeatureLevel, // 最低限必要なフィーチャーレベル
	REFIID 　　　　　 riid,     // 後述
	void**            ppDevice  // 後述
);*/

#ifdef _DEBUG
int main()
{
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#endif
	DebugOutputFormatString("Show window test.");
	HINSTANCE hlnst = GetModuleHandle(nullptr);

	// ウィンドウクラスの生成＆登録
	WNDCLASSEX w = {};

	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)WindowProcedure; // コールバック関数の指定
	w.lpszClassName = _T("DX12Sample");       // アプリケーションクラス名（適当でよい）
	w.hInstance = GetModuleHandle(nullptr);   // ハンドルの取得

	RegisterClassEx(&w); // アプリケーションクラス（ウィンドウクラスの指定を OS に伝える）

	RECT wrc = { 0,0,window_width,window_height }; // ウィンドウサイズを決める

	// 関数を使ってウィンドウのサイズを補正する
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	// ウィンドウオブジェクトの生成
	HWND hwnd = CreateWindow(w.lpszClassName, // クラス名指定
		_T("DX12 テスト"),      // タイトルバーの文字
		WS_OVERLAPPEDWINDOW,    // タイトルバーと境界線があるウィンドウ
		CW_USEDEFAULT,          // 表示 x 座標は OS にお任せ
		CW_USEDEFAULT,          // 表示 y 座標は OS にお任せ
		wrc.right - wrc.left,   // ウィンドウ幅
		wrc.bottom - wrc.top,   // ウィンドウ高
		nullptr,                // 親ウィンドウハンドル
		nullptr,                // メニューハンドル
		w.hInstance,            // 呼び出しアプリケーションハンドル
		nullptr);               // 追加パラメーター

	//DirectX12まわり初期化
	//フィーチャレベル列挙
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	auto result = CreateDXGIFactory(IID_PPV_ARGS(&_dxgiFactory));
	if (result != S_OK)
	{
		DebugOutputFormatString("FAILED CreateDXGIFactory1");
		return -1;
	}

	/*HRESULT result = S_OK;
	if (FAILED(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&dxgiFactory_))))
	{
		if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory_)))) {
			return -1;
		}
	}*/

	// アダプターの列挙用
	std::vector <IDXGIAdapter*> adapters;

	// ここに特定の名前を持つアダプターオブジェクトが入る
	IDXGIAdapter* tmpAdapter = nullptr;
	for (int i = 0; _dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		adapters.push_back(tmpAdapter);
	}

	for (auto adpt : adapters)
	{
		DXGI_ADAPTER_DESC adesc = {};
		adpt->GetDesc(&adesc); // アダプターの説明オブジェクト取得

		std::wstring strDesc = adesc.Description;

		// 探したいアダプターの名前を確認
		if (strDesc.find(L"NVIDIA") != std::string::npos)
		{
			tmpAdapter = adpt;
			break;
		}
	}

	//Direct3Dデバイスの初期化
	D3D_FEATURE_LEVEL featureLevel;
	for (auto l : levels) {
		if (D3D12CreateDevice(tmpAdapter, l, IID_PPV_ARGS(&_dev)) == S_OK) {
			featureLevel = l;
			break;
		}
	}

	// ウィンドウ表示
	ShowWindow(hwnd, SW_SHOW);

	MSG msg = {};

	while (true)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		// アプリケーションが終わるときに message が WM_QUIT になる
		if (msg.message == WM_QUIT)
		{
			break;
		}
	}

	// もうクラスは使わないので登録解除する
	UnregisterClass(w.lpszClassName, w.hInstance);

	getchar();
	return 0;
}