#pragma once
#include "pch.h"
#include "DirectXHelper.h"


namespace thinr
{
    // すべての DirectX デバイス リソースを制御します。
    class DeviceManager
    {
    public:
        DeviceManager();
        ~DeviceManager();
        void ClearContext();
        void SetBackbuffer(const Microsoft::WRL::ComPtr<ID3D11Texture2D1> &backbuffer);
        void DiscardView();

        // D3D アクセサー。
        Microsoft::WRL::ComPtr<ID3D11Device3>				GetD3DDevice() const { return m_d3dDevice; }
        ID3D11DeviceContext3*		GetD3DDeviceContext() const { return m_d3dContext.Get(); }
        D3D_FEATURE_LEVEL			GetDeviceFeatureLevel() const { return m_d3dFeatureLevel; }
        ID3D11RenderTargetView1*	GetBackBufferRenderTargetView() const { return m_d3dRenderTargetView.Get(); }
        ID3D11DepthStencilView*		GetDepthStencilView() const { return m_d3dDepthStencilView.Get(); }
        D3D11_VIEWPORT				GetScreenViewport() const { return m_screenViewport; }

        // D2D アクセサー。
        ID2D1Factory3*				GetD2DFactory() const { return m_d2dFactory.Get(); }
        ID2D1Device2*				GetD2DDevice() const { return m_d2dDevice.Get(); }
        ID2D1DeviceContext2*		GetD2DDeviceContext() const { return m_d2dContext.Get(); }
        ID2D1Bitmap1*				GetD2DTargetBitmap() const { return m_d2dTargetBitmap.Get(); }
        IDWriteFactory3*			GetDWriteFactory() const { return m_dwriteFactory.Get(); }
        IWICImagingFactory2*		GetWicImagingFactory() const { return m_wicFactory.Get(); }

        void SetOrientationTransform(const D2D1::Matrix3x2F &o2d, const DirectX::XMFLOAT4X4 &o3d)
        {
            m_orientationTransform2D = o2d;
            m_orientationTransform3D = o3d;
        }
        DirectX::XMFLOAT4X4			GetOrientationTransform3D() const { return m_orientationTransform3D; }
        D2D1::Matrix3x2F			GetOrientationTransform2D() const { return m_orientationTransform2D; }

        float GetDpi()const { return m_dpi; }
        void SetDpi(float dpi) {
            m_dpi = dpi;
            m_d2dContext->SetDpi(m_dpi, m_dpi);
        }
        void SetLogicalSize(const D2D1_SIZE_F &size) { m_logicalSize = size; }
        D2D1_SIZE_F GetLogicalSize()const { return m_logicalSize; }

    private:
        Microsoft::WRL::ComPtr<ID3D11Device3>			m_d3dDevice;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext3>	m_d3dContext;

        // Direct3D レンダリング オブジェクト。3D で必要です。
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView1>	m_d3dRenderTargetView;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView>	m_d3dDepthStencilView;
        D3D11_VIEWPORT									m_screenViewport;

        // Direct2D 描画コンポーネント。
        Microsoft::WRL::ComPtr<ID2D1Factory3>		m_d2dFactory;
        Microsoft::WRL::ComPtr<ID2D1Device2>		m_d2dDevice;
        Microsoft::WRL::ComPtr<ID2D1DeviceContext2>	m_d2dContext;
        Microsoft::WRL::ComPtr<ID2D1Bitmap1>		m_d2dTargetBitmap;

        // DirectWrite 描画コンポーネント。
        Microsoft::WRL::ComPtr<IDWriteFactory3>		m_dwriteFactory;
        Microsoft::WRL::ComPtr<IWICImagingFactory2>	m_wicFactory;

        D3D_FEATURE_LEVEL								m_d3dFeatureLevel;

        float											m_dpi;

        // 方向を表示するために使用する変換。
        D2D1::Matrix3x2F	m_orientationTransform2D;
        DirectX::XMFLOAT4X4	m_orientationTransform3D;

        D2D1_SIZE_F m_logicalSize;
    };

}
