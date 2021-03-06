//ポリゴン表示
#include<Windows.h>
#include<tchar.h>
#include<d3d12.h>
#include<dxgi1_6.h>
#include<DirectXMath.h>
#include<vector>
#include<string>
#include<d3dcompiler.h>
//#include<DirectXTex.h>

#ifdef _DEBUG
#include<iostream>
#endif

//#pragma comment(lib,"DirectXTex.lib")
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

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
ID3D12CommandAllocator* _cmdAllocator = nullptr;
ID3D12GraphicsCommandList* _cmdList = nullptr;
ID3D12CommandQueue* _cmdQueue = nullptr;

void EnableDebugLayer()
{
	ID3D12Debug* debugLayer = nullptr;
	auto result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));

	debugLayer->EnableDebugLayer(); // デバッグレイヤーを有効化する
	debugLayer->Release(); // 有効化したらインターフェイスを解放する
}

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

#ifdef _DEBUG
	// デバッグレイヤーをオンに
	EnableDebugLayer();
#endif

	// DirectX12 の導入
	// 1.IDXGIFactory6 を生成
	auto result = S_OK;
#ifdef _DEBUG
	result = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_dxgiFactory));
#else
	result = CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory));
#endif

	// 2.VGAアダプタ IDXGIAdapter の配列を IDXGIFactory6 から取り出す
	// アダプターの列挙用
	std::vector <IDXGIAdapter*> adapters;

	// ここに特定の名前を持つアダプターオブジェクトが入る
	IDXGIAdapter* tmpAdapter = nullptr;
	for (int i = 0; _dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		adapters.push_back(tmpAdapter);
	}

	// 3.使いたいアダプターを VGA のメーカーで選ぶ
	for (auto adpt : adapters)
	{
		DXGI_ADAPTER_DESC adesc = {};
		adpt->GetDesc(&adesc); // アダプターの説明オブジェクト取得
		std::wstring strDesc = adesc.Description;

		// 探したいアダプターの名前を確認
		if (strDesc.find(L"NVIDIA") != std::string::npos)
		{
			tmpAdapter = adpt;
			break; // 生成可能なバージョンが見つかったらループを打ち切り
		}
	}

	// 4.ID3D12Device を選んだアダプタを用いて初期化し生成する
	// フィーチャレベル列挙
	D3D_FEATURE_LEVEL levels[] =
	{
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	// Direct3Dデバイスの初期化
	D3D_FEATURE_LEVEL featureLevel;
	for (auto l : levels) {
		if (D3D12CreateDevice(tmpAdapter, l, IID_PPV_ARGS(&_dev)) == S_OK) {
			featureLevel = l;
			break; // 生成可能なバージョンが見つかったらループを打ち切り
		}
	}

	// コマンドリストの作成とコマンドアロケーター
	result = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmdAllocator));
	result = _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator, nullptr, IID_PPV_ARGS(&_cmdList));

	// コマンドキュー
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; // タイムアウトなし
	cmdQueueDesc.NodeMask = 0; // アダプターを1つしか使わないときは0でよい	
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL; // プライオリティは特に指定なし
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // コマンドリストと合わせる

	// キュー生成
	result = _dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&_cmdQueue));

	// スワップチェーンの生成
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
	swapchainDesc.Width = window_width;
	swapchainDesc.Height = window_height;
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchainDesc.Stereo = false;
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapchainDesc.BufferCount = 2;
	swapchainDesc.Scaling = DXGI_SCALING_STRETCH; // バックバファーは伸び縮み可能
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // フリップ後は速やかに破棄
	swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED; // 特に指定なし
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; // ウィンドウ⇔フルスクリーン切り替え可能
	result = _dxgiFactory->CreateSwapChainForHwnd(
		_cmdQueue,
		hwnd,
		&swapchainDesc,
		nullptr,
		nullptr,
		(IDXGISwapChain1**)&_swapchain);

	// ディスクリプターの設定
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // レンダーターゲットビューなので RTV
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2; // 表裏の2つ
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // 特に指定なし
	ID3D12DescriptorHeap* rtvHeaps = nullptr;
	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeaps));

	// スワップチェーンのメモリとひも付ける
	DXGI_SWAP_CHAIN_DESC swcDesc = {};
	result = _swapchain->GetDesc(&swcDesc);

	std::vector<ID3D12Resource*> _backBuffers(swcDesc.BufferCount);

	// 先頭のアドレスを得る
	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();

	// SRGBレンダーターゲットビュー設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // ガンマ補正あり（sRGB）
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	for (UINT idx = 0; idx < swcDesc.BufferCount; ++idx)
	{
		result = _swapchain->GetBuffer(static_cast<UINT>(idx), IID_PPV_ARGS(&_backBuffers[idx]));

		// レンダーターゲットビューを生成する
		_dev->CreateRenderTargetView(_backBuffers[idx], nullptr, handle);

		// ポインターをずらす
		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	// フェンスを用意
	ID3D12Fence* _fence = nullptr;
	UINT64 _fenceVal = 0;
	result = _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));

	// ウィンドウ表示
	ShowWindow(hwnd, SW_SHOW);

	// 座標の定義
	/*struct Vertex {
		XMFLOAT3 pos; // XYZ座標
		XMFLOAT2 uv; // UV座標
	};

	Vertex  vertices[] = {
		{ { -0.4f, -0.7f, 0.0f }, { 0,1 } }, //左下
		{ { -0.4f, 0.7f, 0.0f }, { 0,0 } }, //左上
		{ { 0.4f, -0.7f, 0.0f }, { 1,1 } }, //右下
		{ { 0.4f, 0.7f, 0.0f }, { 1,0 } } //右上
	};*/

	XMFLOAT3 vertices[] = {
		// 五角形
		{ 0.0f, 0.5f, 0.0f },     // 真上
		{ -0.175f, -0.5f, 0.0f }, // 左下 
		{ -0.3f, 0.1f, 0.0f },    // 左上
		{ 0.175f, -0.5f, 0.0f },  // 右下
		{ 0.3f, 0.1f, 0.0f },     // 右上

		// 六角形
		{ 0.8f, -0.2f, 0.0f },    // 真上
		{ 0.7f, -0.5f, 0.0f },    // 左下 
		{ 0.7f, -0.3f, 0.0f },    // 左上
		{ 0.9f, -0.5f, 0.0f },    // 右下
		{ 0.9f, -0.3f, 0.0f },    // 右上
		{ 0.8f, -0.6f, 0.0f },    // 真下
	};

	unsigned short indices[] = {
		// 五角形
		0,1,2, 0,1,3, 0,3,4,

		// 六角形
		5,6,7, 5,10,6, 5,10,8, 5,8,9
	};

	// 頂点バッファーの生成
	D3D12_HEAP_PROPERTIES heapprop = {};
	heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	//auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD); // UPLOADヒープとして
	D3D12_RESOURCE_DESC resdesc = {};
	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Width = sizeof(vertices); // 頂点情報が入るだけのサイズ
	resdesc.Height = 1;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.Format = DXGI_FORMAT_UNKNOWN;
	resdesc.SampleDesc.Count = 1;
	resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	//auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertices));
	ID3D12Resource* vertBuff = nullptr;
	result = _dev->CreateCommittedResource(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertBuff));

	// 頂点情報のコピー
	XMFLOAT3* vertMap = nullptr;
	result = vertBuff->Map(0, nullptr, (void**)&vertMap);
	std::copy(std::begin(vertices), std::end(vertices), vertMap);
	vertBuff->Unmap(0, nullptr);

	// 頂点バッファビューの作成
	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	vbView.BufferLocation = vertBuff->GetGPUVirtualAddress(); // バッファーの仮想アドレス
	vbView.SizeInBytes = sizeof(vertices);      // 全バイト数
	vbView.StrideInBytes = sizeof(vertices[0]); // 1頂点あたりのバイト数

	ID3D12Resource* idxBuff = nullptr;

	// 設定は、バッファーのサイズ以外、頂点バッファーの設定を使い回してよい
	resdesc.Width = sizeof(indices);

	result = _dev->CreateCommittedResource(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&idxBuff));

	// 作ったバッファーにインデックスデータをコピー
	unsigned short* mappedIdx = nullptr;
	idxBuff->Map(0, nullptr, (void**)&mappedIdx);
	std::copy(std::begin(indices), std::end(indices), mappedIdx);
	idxBuff->Unmap(0, nullptr);

	// インデックスバッファービューを作成
	D3D12_INDEX_BUFFER_VIEW ibView = {};
	ibView.BufferLocation = idxBuff->GetGPUVirtualAddress();
	ibView.Format = DXGI_FORMAT_R16_UINT;
	ibView.SizeInBytes = sizeof(indices);

	// Shader用の Blob を用意
	ID3DBlob* _vsBlob = nullptr; // 頂点シェーダー
	ID3DBlob* _psBlob = nullptr; // ピクセルシェーダー
	ID3DBlob* errorBlob = nullptr; // エラー格納用

	// 頂点シェーダーをコンパイルする
	result = D3DCompileFromFile(
		L"BasicVertexShader.hlsl", // シェーダー名
		nullptr, // define はなし
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルードはデフォルト
		"BasicVS", "vs_5_0", // 関数は BasicVS、対象シェーダーは vs_5_0
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用および最適化なし
		0, &_vsBlob, &errorBlob); // エラー時は errorBlob にメッセージが入る

	// 頂点シェーダーのコンパイル成功、ピクセルシェーダーのコンパイル
	if (SUCCEEDED(result))
	{
		result = D3DCompileFromFile(
			L"BasicPixelShader.hlsl",
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"BasicPS", "ps_5_0",
			D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
			0, &_psBlob, &errorBlob);

		if (FAILED(result)) // コンパイルエラーの場合
		{
			if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
			{
				::OutputDebugStringA("シェーダーファイルが見当たりません");
			}
			else {
				std::string errstr;
				errstr.resize(errorBlob->GetBufferSize());
				std::copy_n((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize(), errstr.begin());
				errstr += "\n";
				::OutputDebugStringA(errstr.c_str());
			}
			exit(1);
		}
	}
	else {
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
			::OutputDebugStringA("シェーダーファイルが見当たりません");
		}
		else {
			std::string errstr;
			errstr.resize(errorBlob->GetBufferSize());
			std::copy_n((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize(), errstr.begin());
			errstr += "\n";
			::OutputDebugStringA(errstr.c_str());
		}
		exit(1);
	}

	// 頂点レイアウト
	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{
			"POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,	0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{ // uv（追加）
			"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,0, // float二つ分
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,	0
		},
	};

	// パイプラインステートの設定
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};
	gpipeline.pRootSignature = nullptr; // あとで設定する
	gpipeline.VS.pShaderBytecode = _vsBlob->GetBufferPointer();
	gpipeline.VS.BytecodeLength = _vsBlob->GetBufferSize();
	gpipeline.PS.pShaderBytecode = _psBlob->GetBufferPointer();
	gpipeline.PS.BytecodeLength = _psBlob->GetBufferSize();
	// gpipeline.StreamOutput.NumEntriesについては未指定

	// デフォルトのサンプルマスクを表す定数（0xffffffff）
	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	gpipeline.BlendState.AlphaToCoverageEnable = false;
	gpipeline.BlendState.IndependentBlendEnable = false;

	D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};

	//ひとまず加算や乗算やαブレンディングは使用しない
	renderTargetBlendDesc.BlendEnable = false;
	renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	//ひとまず論理演算は使用しない
	renderTargetBlendDesc.LogicOpEnable = false;

	gpipeline.BlendState.RenderTarget[0] = renderTargetBlendDesc;

	// まだアンチエイリアスは使わないため false
	gpipeline.RasterizerState.MultisampleEnable = false;

	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // カリングしない
	gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; // 中身を塗りつぶす
	gpipeline.RasterizerState.DepthClipEnable = true; // 深度方向のクリッピングは有効に

	gpipeline.RasterizerState.FrontCounterClockwise = false;
	gpipeline.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	gpipeline.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	gpipeline.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	gpipeline.RasterizerState.AntialiasedLineEnable = false;
	gpipeline.RasterizerState.ForcedSampleCount = 0;
	gpipeline.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	// D3D12_DEPTH_STENCIL_DESC 深度ステンシル   
	gpipeline.DepthStencilState.DepthEnable = false;
	gpipeline.DepthStencilState.StencilEnable = false;

	// 予め用意した頂点レイアウトを設定
	gpipeline.InputLayout.pInputElementDescs = inputLayout; // レイアウト先頭アドレス
	gpipeline.InputLayout.NumElements = _countof(inputLayout); // レイアウト配列の要素数

	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED; // カットなし

	// 三角形で構成
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	// レンダーターゲットの設定
	gpipeline.NumRenderTargets = 1; // 今は1つのみ
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // 0〜1に正規化された RGBA

	gpipeline.SampleDesc.Count = 1; // サンプリングは1ピクセルにつき1
	gpipeline.SampleDesc.Quality = 0; // クオリティは最低

	// ルートシグネチャの作成
	ID3D12RootSignature* rootsignature = nullptr;
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_DESCRIPTOR_RANGE descTblRange = {};
	descTblRange.NumDescriptors = 1; // テクスチャひとつ
	descTblRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // 種別はテクスチャ
	descTblRange.BaseShaderRegister = 0; // 0番スロットから
	descTblRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootparam = {};
	rootparam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootparam.DescriptorTable.pDescriptorRanges = &descTblRange; // デスクリプタレンジのアドレス
	rootparam.DescriptorTable.NumDescriptorRanges = 1; // デスクリプタレンジ数
	rootparam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダから見える

	rootSignatureDesc.pParameters = &rootparam; // ルートパラメータの先頭アドレス
	rootSignatureDesc.NumParameters = 1; // ルートパラメータ数

	D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 横繰り返し
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 縦繰り返し
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 奥行繰り返し
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK; // ボーダーの時は黒
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT; // 補間しない（ニアレストネイバー）
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX; // ミップマップ最大値
	samplerDesc.MinLOD = 0.0f; // ミップマップ最小値
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // オーバーサンプリングの際リサンプリングしない
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダからのみ可視

	rootSignatureDesc.pStaticSamplers = &samplerDesc;
	rootSignatureDesc.NumStaticSamplers = 1;

	// バイナリコードの作成
	ID3DBlob* rootSigBlob = nullptr;
	result = D3D12SerializeRootSignature(
		&rootSignatureDesc,	//ルートシグネチャ設定        
		D3D_ROOT_SIGNATURE_VERSION_1_0, // ルートシグネチャバージョン        
		&rootSigBlob, // シェーダーを作ったときと同じ        
		&errorBlob); // エラー処理も同じ

	result = _dev->CreateRootSignature(
		0, // nodemask。0でよい    
		rootSigBlob->GetBufferPointer(), // シェーダーのときと同様    
		rootSigBlob->GetBufferSize(),    // シェーダーのときと同様    
		IID_PPV_ARGS(&rootsignature));

	rootSigBlob->Release(); // 不要になったので解放
	gpipeline.pRootSignature = rootsignature;

	// グラフィックスパイプラインステートオブジェクトの生成
	ID3D12PipelineState* _pipelinestate = nullptr;
	result = _dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&_pipelinestate));

	// ビューポート
	D3D12_VIEWPORT viewport = {};
	viewport.Width = window_width;   // 出力先の幅（ピクセル数）
	viewport.Height = window_height; // 出力先の高さ（ピクセル数）
	viewport.TopLeftX = 0;    // 出力先の左上座標X
	viewport.TopLeftY = 0;    // 出力先の左上座標Y
	viewport.MaxDepth = 1.0f; // 深度最大値
	viewport.MinDepth = 0.0f; // 深度最小値

	// シザー矩形
	D3D12_RECT scissorrect = {};
	scissorrect.top = 0;  // 切り抜き上座標
	scissorrect.left = 0; // 切り抜き左座標
	scissorrect.right = scissorrect.left + window_width;  // 切り抜き右座標
	scissorrect.bottom = scissorrect.top + window_height; // 切り抜き下座標

	/*// WICテクスチャのロード
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};
	result = LoadFromWICFile(L"img/textest.png", WIC_FLAGS_NONE, &metadata, scratchImg);
	auto img = scratchImg.GetImage(0, 0, 0); // 生データ抽出

	// WriteToSubresource で転送する用のヒープ設定
	D3D12_HEAP_PROPERTIES texHeapProp = {};
	texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM; // 特殊な設定なのでdefaultでもuploadでもなく
	texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK; // ライトバックで
	texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0; // 転送がL0つまりCPU側から直で
	texHeapProp.CreationNodeMask = 0; // 単一アダプタのため0
	texHeapProp.VisibleNodeMask = 0; // 単一アダプタのため0

	D3D12_RESOURCE_DESC resDesc = {};
	//resDesc.Format = metadata.format;DXGI_FORMAT_R8G8B8A8_UNORM; // RGBAフォーマット
	//resDesc.Width = static_cast<UINT>(metadata.width); // 幅
	//resDesc.Height = static_cast<UINT>(metadata.height); // 高さ
	//resDesc.DepthOrArraySize = static_cast<uint16_t>(metadata.arraySize); // 2Dで配列でもないので１
	resDesc.SampleDesc.Count = 1; // 通常テクスチャなのでアンチェリしない
	resDesc.SampleDesc.Quality = 0;
	//resDesc.MipLevels = static_cast<uint16_t>(metadata.mipLevels); // ミップマップしないのでミップ数は１つ
	//resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension); // 2Dテクスチャ用
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; // レイアウトについては決定しない
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE; // とくにフラグなし

	ID3D12Resource* texbuff = nullptr;
	result = _dev->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,//特に指定なし
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,//テクスチャ用(ピクセルシェーダから見る用)
		nullptr,
		IID_PPV_ARGS(&texbuff)
	);

	result = texbuff->WriteToSubresource(0,
		nullptr, // 全領域へコピー
		img->pixels, // 元データアドレス
		static_cast<UINT>(img->rowPitch), // 1ラインサイズ
		static_cast<UINT>(img->slicePitch) // 全サイズ
	);

	ID3D12DescriptorHeap* texDescHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // シェーダから見えるように
	descHeapDesc.NodeMask = 0; // マスクは0
	descHeapDesc.NumDescriptors = 1; // ビューは今のところ１つだけ
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; // シェーダリソースビュー（および定数、UAVも）
	result = _dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&texDescHeap)); // 生成

	// 通常テクスチャビュー作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	//srvDesc.Format = metadata.format;//DXGI_FORMAT_R8G8B8A8_UNORM; // RGBA（0.0f〜1.0fに正規化）
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // 後述
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = 1; // ミップマップは使用しないので1

	_dev->CreateShaderResourceView(texbuff, // ビューと関連付けるバッファ
		&srvDesc, // 先ほど設定したテクスチャ設定情報
		texDescHeap->GetCPUDescriptorHandleForHeapStart() // ヒープのどこに割り当てるか
	);*/

	MSG msg = {};
	unsigned int frame = 0;
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

		/*angle += 0.1f;
		worldMat = XMMatrixRotationY(angle);
		worldMat *= XMMatrixRotationX(sin(angle / 16.0f) * XM_PI);*/

		auto bbIdx = _swapchain->GetCurrentBackBufferIndex();

		D3D12_RESOURCE_BARRIER BarrierDesc = {};
		BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; // 遷移
		BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;   // 特に指定なし
		BarrierDesc.Transition.pResource = _backBuffers[bbIdx]; // バックバッファーリソース
		BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT; // 直前は PRESENT 状態
		BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET; // 今からレンダーターゲット状態
		_cmdList->ResourceBarrier(1, &BarrierDesc); // バリア指定実行

		// パイプラインの設定
		_cmdList->SetPipelineState(_pipelinestate);

		// レンダーターゲットを指定
		auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		rtvH.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		_cmdList->OMSetRenderTargets(1, &rtvH, true, nullptr);

		// 画面クリア
		float r, g, b;
		r = (float)(0xff & frame >> 0) / 255.0f;
		g = (float)(0xff & frame >> 0) * 255.0f;
		b = (float)(0xff & frame >> 16) / 255.0f;
		float clearColor[] = { r,g,b,1.0f };
		//float clearColor[] = { 0.5f, 1.0f, 0.8f, 1.0f }; // アクアマリン色
		_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
		++frame;

		// ビューポート、シザー、ルートシグネチャの設定
		_cmdList->RSSetViewports(1, &viewport);
		_cmdList->RSSetScissorRects(1, &scissorrect);
		_cmdList->SetGraphicsRootSignature(rootsignature);

		// トポロジ
		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// 頂点移動
		for (int i = 0; i < 5; i++)
		{
			vertices[i].x = (vertices[i].x * cos(frame / 2500.0f));
			vertices[i].y = (vertices[i].y + sin(frame / 100000.0f));
		}
		XMFLOAT3* vertMap = nullptr;
		result = vertBuff->Map(0, nullptr, (void**)&vertMap);
		std::copy(std::begin(vertices), std::end(vertices), vertMap);
		vertBuff->Unmap(0, nullptr);

		// 頂点
		_cmdList->IASetVertexBuffers(0, 1, &vbView);
		_cmdList->IASetIndexBuffer(&ibView);

		// 頂点を描画
		//_cmdList->DrawInstanced(4, 1, 0, 0);
		_cmdList->DrawIndexedInstanced(21, 1, 0, 0, 0);

		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		_cmdList->ResourceBarrier(1, &BarrierDesc);

		// 命令のクローズ
		_cmdList->Close();

		// コマンドリストの実行
		ID3D12CommandList* cmdlists[] = { _cmdList };
		_cmdQueue->ExecuteCommandLists(1, cmdlists);

		// フェンスで処理を待つ
		_cmdQueue->Signal(_fence, ++_fenceVal);

		if (_fence->GetCompletedValue() != _fenceVal)
		{
			auto event = CreateEvent(nullptr, false, false, nullptr); // イベントハンドルの取得
			_fence->SetEventOnCompletion(_fenceVal, event);
			WaitForSingleObject(event, INFINITE); // イベントが発生するまで待ち続ける（INFINITE）
			CloseHandle(event); // イベントハンドルを閉じる
		}

		_cmdAllocator->Reset(); // キューをクリア
		_cmdList->Reset(_cmdAllocator, nullptr); // 再びコマンドリストをためる準備

		// フリップ
		_swapchain->Present(1, 0);
	}

	// もうクラスは使わないので登録解除する
	UnregisterClass(w.lpszClassName, w.hInstance);
	char c = getchar();
	return 0;
}