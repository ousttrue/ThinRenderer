#include "pch.h"
#include "DeviceManager.h"


using namespace Microsoft::WRL;


namespace thinr
{
    DeviceManager::DeviceManager()
        :
        m_screenViewport(),
        m_d3dFeatureLevel(D3D_FEATURE_LEVEL_9_1),
        m_dpi(-1.0f)
    {
        // Direct2D リソースを初期化します。
        D2D1_FACTORY_OPTIONS options;
        ZeroMemory(&options, sizeof(D2D1_FACTORY_OPTIONS));

#if defined(_DEBUG)
        // プロジェクトがデバッグ ビルドに含まれている場合は、Direct2D デバッグを SDK レイヤーを介して有効にします。
        options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

        // Direct2D ファクトリを初期化します。
        ThrowIfFailed(
            D2D1CreateFactory(
                D2D1_FACTORY_TYPE_SINGLE_THREADED,
                __uuidof(ID2D1Factory3),
                &options,
                &m_d2dFactory
            )
        );

        // DirectWrite ファクトリを初期化します。
        ThrowIfFailed(
            DWriteCreateFactory(
                DWRITE_FACTORY_TYPE_SHARED,
                __uuidof(IDWriteFactory3),
                &m_dwriteFactory
            )
        );

        // Windows Imaging Component (WIC) ファクトリを初期化します。
        ThrowIfFailed(
            CoCreateInstance(
                CLSID_WICImagingFactory2,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&m_wicFactory)
            )
        );

        ////

        //このフラグは、カラー チャネルの順序が API の既定値とは異なるサーフェスのサポートを追加します。
        // これは、Direct2D との互換性を保持するために必要です。
        UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
        if (SdkLayersAvailable())
        {
            // プロジェクトがデバッグ ビルドに含まれる場合、このフラグを使用して SDK レイヤーによるデバッグを有効にします。
            creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
        }
#endif

        // この配列では、このアプリケーションでサポートされる DirectX ハードウェア機能レベルのセットを定義します。
        // 順序が保存されることに注意してください。
        //アプリケーションの最低限必要な機能レベルをその説明で宣言することを忘れないでください。
        //特に記載がない限り、すべてのアプリケーションは 9.1 をサポートすることが想定されます。
        D3D_FEATURE_LEVEL featureLevels[] =
        {
            D3D_FEATURE_LEVEL_12_1,
            D3D_FEATURE_LEVEL_12_0,
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_9_3,
            D3D_FEATURE_LEVEL_9_2,
            D3D_FEATURE_LEVEL_9_1
        };

        // Direct3D 11 API デバイス オブジェクトと、対応するコンテキストを作成します。
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;

        HRESULT hr = D3D11CreateDevice(
            nullptr,					// 既定のアダプターを使用する nullptr を指定します。
            D3D_DRIVER_TYPE_HARDWARE,	// ハードウェア グラフィックス ドライバーを使用してデバイスを作成します。
            0,							// ドライバーが D3D_DRIVER_TYPE_SOFTWARE でない限り、0 を使用してください。
            creationFlags,				// デバッグ フラグと Direct2D 互換性フラグを設定します。
            featureLevels,				// このアプリがサポートできる機能レベルの一覧を表示します。
            ARRAYSIZE(featureLevels),	// 上記リストのサイズ。
            D3D11_SDK_VERSION,			// Windows ストア アプリでは、これには常に D3D11_SDK_VERSION を設定します。
            &device,					// 作成された Direct3D デバイスを返します。
            &m_d3dFeatureLevel,			// 作成されたデバイスの機能レベルを返します。
            &context					// デバイスのイミディエイト コンテキストを返します。
        );

        if (FAILED(hr))
        {
            // 初期化が失敗した場合は、WARP デバイスにフォール バックします。
            // WARP の詳細については、次を参照してください: 
            // https://go.microsoft.com/fwlink/?LinkId=286690
            ThrowIfFailed(
                D3D11CreateDevice(
                    nullptr,
                    D3D_DRIVER_TYPE_WARP, // ハードウェア デバイスの代わりに WARP デバイスを作成します。
                    0,
                    creationFlags,
                    featureLevels,
                    ARRAYSIZE(featureLevels),
                    D3D11_SDK_VERSION,
                    &device,
                    &m_d3dFeatureLevel,
                    &context
                )
            );
        }

        // Direct3D 11.3 API デバイスへのポインターとイミディエイト コンテキストを保存します。
        ThrowIfFailed(
            device.As(&m_d3dDevice)
        );

        ThrowIfFailed(
            context.As(&m_d3dContext)
        );

        // Direct2D デバイス オブジェクトと、対応するコンテキストを作成します。
        ComPtr<IDXGIDevice3> dxgiDevice;
        ThrowIfFailed(
            m_d3dDevice.As(&dxgiDevice)
        );

        ThrowIfFailed(
            m_d2dFactory->CreateDevice(dxgiDevice.Get(), &m_d2dDevice)
        );

        ThrowIfFailed(
            m_d2dDevice->CreateDeviceContext(
                D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
                &m_d2dContext
            )
        );
    }

    DeviceManager::~DeviceManager()
    {

    }

    void DeviceManager::ClearContext()
    {
        // 前のウィンドウ サイズに固有のコンテキストをクリアします。
        ID3D11RenderTargetView* nullViews[] = { nullptr };
        m_d3dContext->OMSetRenderTargets(ARRAYSIZE(nullViews), nullViews, nullptr);
        m_d3dRenderTargetView = nullptr;
        m_d2dContext->SetTarget(nullptr);
        m_d2dTargetBitmap = nullptr;
        m_d3dDepthStencilView = nullptr;
        m_d3dContext->Flush1(D3D11_CONTEXT_TYPE_ALL, nullptr);
    }

    void DeviceManager::SetBackbuffer(const Microsoft::WRL::ComPtr<ID3D11Texture2D1> &backBuffer)
    {
        ThrowIfFailed(
            m_d3dDevice->CreateRenderTargetView1(
                backBuffer.Get(),
                nullptr,
                &m_d3dRenderTargetView
            )
        );

        D3D11_TEXTURE2D_DESC desc;
        backBuffer->GetDesc(&desc);

        // 必要な場合は 3D レンダリングで使用する深度ステンシル ビューを作成します。
        CD3D11_TEXTURE2D_DESC1 depthStencilDesc(
            DXGI_FORMAT_D24_UNORM_S8_UINT,
            desc.Width,
            desc.Height,
            1, // この深度ステンシル ビューには、1 つのテクスチャしかありません。
            1, // 1 つの MIPMAP レベルを使用します。
            D3D11_BIND_DEPTH_STENCIL
        );

        ComPtr<ID3D11Texture2D1> depthStencil;
        ThrowIfFailed(
            m_d3dDevice->CreateTexture2D1(
                &depthStencilDesc,
                nullptr,
                &depthStencil
            )
        );

        CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(D3D11_DSV_DIMENSION_TEXTURE2D);
        ThrowIfFailed(
            m_d3dDevice->CreateDepthStencilView(
                depthStencil.Get(),
                &depthStencilViewDesc,
                &m_d3dDepthStencilView
            )
        );

        // 3D レンダリング ビューポートをウィンドウ全体をターゲットにするように設定します。
        m_screenViewport = CD3D11_VIEWPORT(
            0.0f,
            0.0f,
            (float)desc.Width,
            (float)desc.Height
        );

        m_d3dContext->RSSetViewports(1, &m_screenViewport);

        // スワップ チェーン バック バッファーに関連付けられた Direct2D ターゲット ビットマップを作成し、
        // それを現在のターゲットとして設定します。
        D2D1_BITMAP_PROPERTIES1 bitmapProperties =
            D2D1::BitmapProperties1(
                D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
                m_dpi,
                m_dpi
            );

        ComPtr<IDXGISurface2> dxgiBackBuffer;
        /*
        ThrowIfFailed(
        m_swapChain->GetBuffer(0, IID_PPV_ARGS(&dxgiBackBuffer))
        );
        */
        ThrowIfFailed(
            backBuffer.As(&dxgiBackBuffer)
        );

        ThrowIfFailed(
            m_d2dContext->CreateBitmapFromDxgiSurface(
                dxgiBackBuffer.Get(),
                &bitmapProperties,
                &m_d2dTargetBitmap
            )
        );

        m_d2dContext->SetTarget(m_d2dTargetBitmap.Get());
        m_d2dContext->SetDpi(m_dpi, m_dpi);

        // すべての Windows ストア アプリで、グレースケール テキストのアンチエイリアシングをお勧めします。
        m_d2dContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    }

    void DeviceManager::DiscardView()
    {
        // レンダリング ターゲットのコンテンツを破棄します。
        //この操作は、既存のコンテンツ全体が上書きされる場合のみ有効です。
        // dirty rect または scroll rect を使用する場合は、この呼び出しを削除する必要があります。
        m_d3dContext->DiscardView1(m_d3dRenderTargetView.Get(), nullptr, 0);

        // 深度ステンシルのコンテンツを破棄します。
        m_d3dContext->DiscardView1(m_d3dDepthStencilView.Get(), nullptr, 0);
    }
}
