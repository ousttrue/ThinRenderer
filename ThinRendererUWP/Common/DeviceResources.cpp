#include "pch.h"
#include "DeviceResources.h"
#include "DirectXHelper.h"


using namespace D2D1;
using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml::Controls;
using namespace Platform;




namespace DisplayMetrics
{
	// 高解像度ディスプレイは、レンダリングに多くの GPU とバッテリ電力を必要とします。
	// ゲームを完全な再現性を維持して毎秒 60 フレームでレンダリングしようとすると、
	// 高解像度の携帯電話などではバッテリの寿命の短さに悩まされる場合があります。
	// すべてのプラットフォームとフォーム ファクターにわたって完全な再現性を維持してのレンダリングは、
	// 慎重に検討して決定する必要があります。
	static const bool SupportHighResolutions = false;

	// "高解像度" ディスプレイを定義する既定のしきい値。しきい値を
	// 超え、SupportHighResolutions が false の場合は、ディメンションが
	// 50 % のスケールになります。
	static const float DpiThreshold = 192.0f;		// 標準のデスクトップの 200% 表示。
	static const float WidthThreshold = 1920.0f;	// 幅 1080p。
	static const float HeightThreshold = 1080.0f;	// 高さ 1080p。
};

// 画面の回転の計算に使用する定数
namespace ScreenRotation
{
	// 0 度 Z 回転
	static const XMFLOAT4X4 Rotation0(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// 90 度 Z 回転
	static const XMFLOAT4X4 Rotation90(
		0.0f, 1.0f, 0.0f, 0.0f,
		-1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// 180 度 Z 回転
	static const XMFLOAT4X4 Rotation180(
		-1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// 270 度 Z 回転
	static const XMFLOAT4X4 Rotation270(
		0.0f, -1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);
};

// DeviceResources に対するコンストラクター。
DX::DeviceResources::DeviceResources() :
	m_d3dRenderTargetSize(),
	m_outputSize(),
	m_nativeOrientation(DisplayOrientations::None),
	m_currentOrientation(DisplayOrientations::None),
	m_deviceNotify(nullptr),
    m_effectiveDpi(-1.0f)
{
	CreateDeviceIndependentResources();
	CreateDeviceResources();
}

// Direct3D デバイスに依存しないリソースを構成します。
void DX::DeviceResources::CreateDeviceIndependentResources()
{
}

// Direct3D デバイスを構成し、このハンドルとデバイスのコンテキストを保存します。
void DX::DeviceResources::CreateDeviceResources() 
{
    m_manager = std::make_unique<thinr::DeviceManager>();

}

// これらのリソースは、ウィンドウ サイズが変更されるたびに再作成する必要があります。
void DX::DeviceResources::CreateWindowSizeDependentResources() 
{
    m_manager->ClearContext();

	UpdateRenderTargetSize();

	// スワップ チェーンの幅と高さは、ウィンドウのネイティブ方向の幅と高さに
	// 基づいている必要があります。ウィンドウがネイティブではない場合は、
	// サイズを反転させる必要があります。
	DXGI_MODE_ROTATION displayRotation = ComputeDisplayRotation();

	bool swapDimensions = displayRotation == DXGI_MODE_ROTATION_ROTATE90 || displayRotation == DXGI_MODE_ROTATION_ROTATE270;
	m_d3dRenderTargetSize.Width = swapDimensions ? m_outputSize.Height : m_outputSize.Width;
	m_d3dRenderTargetSize.Height = swapDimensions ? m_outputSize.Width : m_outputSize.Height;

	if (m_swapChain != nullptr)
	{
		// スワップ チェーンが既に存在する場合は、そのサイズを変更します。
		HRESULT hr = m_swapChain->ResizeBuffers(
			2, // ダブル バッファーされたスワップ チェーンです。
			lround(m_d3dRenderTargetSize.Width),
			lround(m_d3dRenderTargetSize.Height),
			DXGI_FORMAT_B8G8R8A8_UNORM,
			0
			);

		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
		{
			// 何らかの理由でデバイスを削除した場合、新しいデバイスとスワップ チェーンを作成する必要があります。
			HandleDeviceLost();

			//すべての設定が完了しました。このメソッドの実行を続行しないでください。HandleDeviceLost はこのメソッドに再入し、
			// 新しいデバイスを正しく設定します。
			return;
		}
		else
		{
			DX::ThrowIfFailed(hr);
		}
	}
	else
	{
		// それ以外の場合は、既存の Direct3D デバイスと同じアダプターを使用して、新規作成します。
		DXGI_SCALING scaling = DisplayMetrics::SupportHighResolutions ? DXGI_SCALING_NONE : DXGI_SCALING_STRETCH;
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {0};

		swapChainDesc.Width = lround(m_d3dRenderTargetSize.Width);		// ウィンドウのサイズと一致させます。
		swapChainDesc.Height = lround(m_d3dRenderTargetSize.Height);
		swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;				// これは、最も一般的なスワップ チェーンのフォーマットです。
		swapChainDesc.Stereo = false;
		swapChainDesc.SampleDesc.Count = 1;								// マルチサンプリングは使いません。
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 2;									// 遅延を最小限に抑えるにはダブル バッファーを使用します。
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;	// Windows ストア アプリはすべて、この SwapEffect を使用する必要があります。
		swapChainDesc.Flags = 0;
		swapChainDesc.Scaling = scaling;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

		// このシーケンスは、上の Direct3D デバイスを作成する際に使用された DXGI ファクトリを取得します。
		ComPtr<IDXGIDevice3> dxgiDevice;
		DX::ThrowIfFailed(
			m_manager->GetD3DDevice().As(&dxgiDevice)
			);

		ComPtr<IDXGIAdapter> dxgiAdapter;
		DX::ThrowIfFailed(
			dxgiDevice->GetAdapter(&dxgiAdapter)
			);

		ComPtr<IDXGIFactory4> dxgiFactory;
		DX::ThrowIfFailed(
			dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory))
			);

		ComPtr<IDXGISwapChain1> swapChain;
		DX::ThrowIfFailed(
			dxgiFactory->CreateSwapChainForCoreWindow(
				m_manager->GetD3DDevice().Get(),
				reinterpret_cast<IUnknown*>(m_window.Get()),
				&swapChainDesc,
				nullptr,
				&swapChain
				)
			);
		DX::ThrowIfFailed(
			swapChain.As(&m_swapChain)
			);

		// DXGI が 1 度に複数のフレームをキュー処理していないことを確認します。これにより、遅延が減少し、
		// アプリケーションが各 VSync の後でのみレンダリングすることが保証され、消費電力が最小限に抑えられます。
		DX::ThrowIfFailed(
			dxgiDevice->SetMaximumFrameLatency(1)
			);
	}

	// スワップ チェーンの適切な方向を設定し、回転されたスワップ チェーンにレンダリングするための 2D および
	// 3D マトリックス変換を生成します。
	// 2D および 3D 変換の回転角度は異なります。
	//これは座標空間の違いによります。さらに、
	// 丸めエラーを回避するために 3D マトリックスが明示的に指定されます。

	switch (displayRotation)
	{
	case DXGI_MODE_ROTATION_IDENTITY:
        m_manager->SetOrientationTransform(
            Matrix3x2F::Identity(),
            ScreenRotation::Rotation0);
		break;

	case DXGI_MODE_ROTATION_ROTATE90:
        m_manager->SetOrientationTransform(
            Matrix3x2F::Rotation(90.0f) *
            Matrix3x2F::Translation(m_manager->GetLogicalSize().height, 0.0f),
            ScreenRotation::Rotation270);
		break;

	case DXGI_MODE_ROTATION_ROTATE180:
        m_manager->SetOrientationTransform(
            Matrix3x2F::Rotation(180.0f) *
            Matrix3x2F::Translation(m_manager->GetLogicalSize().width, m_manager->GetLogicalSize().height),
            ScreenRotation::Rotation180);
		break;

	case DXGI_MODE_ROTATION_ROTATE270:
        m_manager->SetOrientationTransform(
            Matrix3x2F::Rotation(270.0f) *
            Matrix3x2F::Translation(0.0f, m_manager->GetLogicalSize().width),
            ScreenRotation::Rotation90);
		break;

	default:
		throw ref new FailureException();
	}

	DX::ThrowIfFailed(
		m_swapChain->SetRotation(displayRotation)
		);

	// スワップ チェーンのバック バッファーのレンダリング ターゲット ビューを作成します。
	ComPtr<ID3D11Texture2D1> backBuffer;
	DX::ThrowIfFailed(
		m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))
		);

    m_manager->SetBackbuffer(backBuffer);
}

// レンダー ターゲットのディメンションを決定し、それをスケール ダウンするかどうかを判断します。
void DX::DeviceResources::UpdateRenderTargetSize()
{
    auto dpi = m_manager->GetDpi();
    m_effectiveDpi = dpi;

	// 高解像度のデバイスのバッテリ寿命を上げるためには、より小さいレンダー ターゲットにレンダリングして
	// 出力が提示された場合は GPU で出力をスケーリングできるようにします。
	if (!DisplayMetrics::SupportHighResolutions && dpi > DisplayMetrics::DpiThreshold)
	{
		float width = DX::ConvertDipsToPixels(m_manager->GetLogicalSize().width, dpi);
		float height = DX::ConvertDipsToPixels(m_manager->GetLogicalSize().height, dpi);

		// デバイスが縦の向きの場合、高さ > 幅となります。
		// 寸法の大きい方を幅しきい値と、小さい方を高さしきい値と
		// それぞれ比較します。
		if (max(width, height) > DisplayMetrics::WidthThreshold && min(width, height) > DisplayMetrics::HeightThreshold)
		{
			// アプリをスケーリングするには有効な DPI を変更します。論理サイズは変更しません。
			m_effectiveDpi /= 2.0f;
		}
	}


	// 必要なレンダリング ターゲットのサイズをピクセル単位で計算します。
	m_outputSize.Width = DX::ConvertDipsToPixels(m_manager->GetLogicalSize().width, m_effectiveDpi);
	m_outputSize.Height = DX::ConvertDipsToPixels(m_manager->GetLogicalSize().height, m_effectiveDpi);

	// サイズ 0 の DirectX コンテンツが作成されることを防止します。
	m_outputSize.Width = max(m_outputSize.Width, 1);
	m_outputSize.Height = max(m_outputSize.Height, 1);

    m_manager->SetDpi(dpi);
}

//このメソッドは、CoreWindow オブジェクトが作成 (または再作成) されるときに呼び出されます。
void DX::DeviceResources::SetWindow(CoreWindow^ window)
{
	DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

	m_window = window;
    m_manager->SetLogicalSize(D2D1::SizeF(window->Bounds.Width, window->Bounds.Height));
	m_nativeOrientation = currentDisplayInformation->NativeOrientation;
	m_currentOrientation = currentDisplayInformation->CurrentOrientation;
	auto dpi = currentDisplayInformation->LogicalDpi;
    m_manager->SetDpi(dpi);

	CreateWindowSizeDependentResources();
}

// このメソッドは、SizeChanged イベント用のイベント ハンドラーの中で呼び出されます。
void DX::DeviceResources::SetLogicalSize(Windows::Foundation::Size _logicalSize)
{
    auto logicalSize = D2D1::SizeF(_logicalSize.Width, _logicalSize.Height);
	if (m_manager->GetLogicalSize().width != logicalSize.width || m_manager->GetLogicalSize().height != logicalSize.height)
	{
		m_manager->SetLogicalSize(logicalSize);
		CreateWindowSizeDependentResources();
	}
}

// このメソッドは、DpiChanged イベント用のイベント ハンドラーの中で呼び出されます。
void DX::DeviceResources::SetDpi(float dpi)
{
	if (dpi != m_manager->GetDpi())
	{
		// ディスプレイ DPI の変更時に、ウィンドウの論理サイズ (Dip 単位) も変更されるため、更新する必要があります。
		m_manager->SetLogicalSize(D2D1::SizeF(m_window->Bounds.Width, m_window->Bounds.Height));

        m_manager->SetDpi(dpi);

		CreateWindowSizeDependentResources();
	}
}

// このメソッドは、OrientationChanged イベント用のイベント ハンドラーの中で呼び出されます。
void DX::DeviceResources::SetCurrentOrientation(DisplayOrientations currentOrientation)
{
	if (m_currentOrientation != currentOrientation)
	{
		m_currentOrientation = currentOrientation;
		CreateWindowSizeDependentResources();
	}
}

// このメソッドは、DisplayContentsInvalidated イベント用のイベント ハンドラーの中で呼び出されます。
void DX::DeviceResources::ValidateDevice()
{
	//デバイスが作成された後に既定のアダプターが変更された、
	// またはこのデバイスが削除された場合は、D3D デバイスが有効でなくなります。

	// まず、デバイスが作成された時点から、既定のアダプターに関する情報を取得します。

	ComPtr<IDXGIDevice3> dxgiDevice;
	DX::ThrowIfFailed(m_manager->GetD3DDevice().As(&dxgiDevice));

	ComPtr<IDXGIAdapter> deviceAdapter;
	DX::ThrowIfFailed(dxgiDevice->GetAdapter(&deviceAdapter));

	ComPtr<IDXGIFactory4> deviceFactory;
	DX::ThrowIfFailed(deviceAdapter->GetParent(IID_PPV_ARGS(&deviceFactory)));

	ComPtr<IDXGIAdapter1> previousDefaultAdapter;
	DX::ThrowIfFailed(deviceFactory->EnumAdapters1(0, &previousDefaultAdapter));

	DXGI_ADAPTER_DESC1 previousDesc;
	DX::ThrowIfFailed(previousDefaultAdapter->GetDesc1(&previousDesc));

	// 次に、現在の既定のアダプターの情報を取得します。

	ComPtr<IDXGIFactory4> currentFactory;
	DX::ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&currentFactory)));

	ComPtr<IDXGIAdapter1> currentDefaultAdapter;
	DX::ThrowIfFailed(currentFactory->EnumAdapters1(0, &currentDefaultAdapter));

	DXGI_ADAPTER_DESC1 currentDesc;
	DX::ThrowIfFailed(currentDefaultAdapter->GetDesc1(&currentDesc));

	// アダプターの LUID が一致しない、またはデバイスで LUID が削除されたとの報告があった場合は、
	// 新しい D3D デバイスを作成する必要があります。

	if (previousDesc.AdapterLuid.LowPart != currentDesc.AdapterLuid.LowPart ||
		previousDesc.AdapterLuid.HighPart != currentDesc.AdapterLuid.HighPart ||
		FAILED(m_manager->GetD3DDevice()->GetDeviceRemovedReason()))
	{
		// 古いデバイスに関連したリソースへの参照を解放します。
		dxgiDevice = nullptr;
		deviceAdapter = nullptr;
		deviceFactory = nullptr;
		previousDefaultAdapter = nullptr;

		// 新しいデバイスとスワップ チェーンを作成します。
		HandleDeviceLost();
	}
}

// すべてのデバイス リソースを再作成し、現在の状態に再設定します。
void DX::DeviceResources::HandleDeviceLost()
{
	m_swapChain = nullptr;

	if (m_deviceNotify != nullptr)
	{
		m_deviceNotify->OnDeviceLost();
	}

    auto dpi = m_manager->GetDpi();
    CreateDeviceResources();
    m_manager->SetDpi(dpi);
	CreateWindowSizeDependentResources();

	if (m_deviceNotify != nullptr)
	{
		m_deviceNotify->OnDeviceRestored();
	}
}

// デバイスが失われたときと作成されたときに通知を受けるように、DeviceNotify を登録します。
void DX::DeviceResources::RegisterDeviceNotify(DX::IDeviceNotify* deviceNotify)
{
	m_deviceNotify = deviceNotify;
}

//アプリが停止したときに、このメソッドを呼び出します。アプリがアイドル状態になっていること、
// 一時バッファーが他のアプリで使用するために解放できることを、ドライバーに示します。
void DX::DeviceResources::Trim()
{
	ComPtr<IDXGIDevice3> dxgiDevice;
	m_manager->GetD3DDevice().As(&dxgiDevice);

	dxgiDevice->Trim();
}

// スワップ チェーンの内容を画面に表示します。
void DX::DeviceResources::Present() 
{
	// 最初の引数は、DXGI に VSync までブロックするよう指示し、アプリケーションを次の VSync まで
	// スリープさせます。これにより、画面に表示されることのないフレームをレンダリングして
	// サイクルを無駄にすることがなくなります。
	DXGI_PRESENT_PARAMETERS parameters = { 0 };
	HRESULT hr = m_swapChain->Present1(1, 0, &parameters);

    m_manager->DiscardView();


	//デバイスが切断またはドライバーの更新によって削除された場合は、
	// すべてのデバイス リソースを再作成する必要があります。
	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
	{
		HandleDeviceLost();
	}
	else
	{
		DX::ThrowIfFailed(hr);
	}
}

// このメソッドは、表示デバイスのネイティブの方向と、現在の表示の方向との間での回転を決定します。
// 回転を決定します。
DXGI_MODE_ROTATION DX::DeviceResources::ComputeDisplayRotation()
{
	DXGI_MODE_ROTATION rotation = DXGI_MODE_ROTATION_UNSPECIFIED;

	// メモ: DisplayOrientations 列挙型に他の値があっても、NativeOrientation として使用できるのは、
	// Landscape または Portrait のどちらかのみです。
	switch (m_nativeOrientation)
	{
	case DisplayOrientations::Landscape:
		switch (m_currentOrientation)
		{
		case DisplayOrientations::Landscape:
			rotation = DXGI_MODE_ROTATION_IDENTITY;
			break;

		case DisplayOrientations::Portrait:
			rotation = DXGI_MODE_ROTATION_ROTATE270;
			break;

		case DisplayOrientations::LandscapeFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE180;
			break;

		case DisplayOrientations::PortraitFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE90;
			break;
		}
		break;

	case DisplayOrientations::Portrait:
		switch (m_currentOrientation)
		{
		case DisplayOrientations::Landscape:
			rotation = DXGI_MODE_ROTATION_ROTATE90;
			break;

		case DisplayOrientations::Portrait:
			rotation = DXGI_MODE_ROTATION_IDENTITY;
			break;

		case DisplayOrientations::LandscapeFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE270;
			break;

		case DisplayOrientations::PortraitFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE180;
			break;
		}
		break;
	}
	return rotation;
}