#pragma once

namespace thinr
{
    class DeviceManager;
}


namespace DX
{
	// DeviceResources を所有しているアプリケーションが、デバイスが失われたときまたは作成されたときに通知を受けるためのインターフェイスを提供します。
	interface IDeviceNotify
	{
		virtual void OnDeviceLost() = 0;
		virtual void OnDeviceRestored() = 0;
	};



    // CoreWindowからのSwapChain作成を担当する
	class DeviceResources
	{
	public:
		DeviceResources();
		void SetWindow(Windows::UI::Core::CoreWindow^ window);
		void SetLogicalSize(Windows::Foundation::Size logicalSize);
		void SetCurrentOrientation(Windows::Graphics::Display::DisplayOrientations currentOrientation);
		void SetDpi(float dpi);
		void ValidateDevice();
		void HandleDeviceLost();
		void RegisterDeviceNotify(IDeviceNotify* deviceNotify);
		void Trim();
		void Present();

		// レンダー ターゲットのサイズ (ピクセル単位)。
		Windows::Foundation::Size	GetOutputSize() const					{ return m_outputSize; }

		// レンダー ターゲットのサイズ (DIP 単位)。
		float						GetDpi() const							{ return m_effectiveDpi; }

		// D3D アクセサー。
		IDXGISwapChain3*			GetSwapChain() const					{ return m_swapChain.Get(); }

        std::shared_ptr<thinr::DeviceManager> GetManager()const { return m_manager; }

	private:
		void CreateDeviceIndependentResources();
		void CreateDeviceResources();
		void CreateWindowSizeDependentResources();
		void UpdateRenderTargetSize();
		DXGI_MODE_ROTATION ComputeDisplayRotation();

		// Direct3D オブジェクト。
        std::shared_ptr<thinr::DeviceManager> m_manager;

		Microsoft::WRL::ComPtr<IDXGISwapChain3>			m_swapChain;


		//ウィンドウへのキャッシュされた参照 (省略可能)。
		Platform::Agile<Windows::UI::Core::CoreWindow> m_window;

		// キャッシュされたデバイス プロパティ。
		Windows::Foundation::Size						m_d3dRenderTargetSize;
		Windows::Foundation::Size						m_outputSize;
		//Windows::Foundation::Size						m_logicalSize;
		Windows::Graphics::Display::DisplayOrientations	m_nativeOrientation;
		Windows::Graphics::Display::DisplayOrientations	m_currentOrientation;

		// IDeviceNotify は DeviceResources を所有しているため、直接保持することもできます。
		IDeviceNotify* m_deviceNotify;

        // これはアプリに戻されるレポートである DPI です。アプリが高解像度スクリーンをサポートしているかどうかが考慮されます。
        float m_effectiveDpi;
	};
}