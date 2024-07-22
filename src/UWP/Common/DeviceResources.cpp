#include "pch.h"
#include <algorithm>
#include "DeviceResources.h"
#include "DirectXHelper.h"
#include "Core/Config.h"
#include <UWPHelpers/StorageExtensions.h>
#include <Common/LogManager.h>
#include <Common/System/NativeApp.h>

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
	// High resolution displays can require a lot of GPU and battery power to render.
	// High resolution phones, for example, may suffer from poor battery life if
	// games attempt to render at 60 frames per second at full fidelity.
	// The decision to render at full fidelity across all platforms and form factors
	// should be deliberate.
	static const bool SupportHighResolutions = true;

	// The default thresholds that define a "high resolution" display. If the thresholds
	// are exceeded and SupportHighResolutions is false, the dimensions will be scaled
	// by 50%.
	static const float DpiThreshold = 192.0f;		// 200% of standard desktop display.
	static const float WidthThreshold = 1920.0f;	// 1080p width.
	static const float HeightThreshold = 1080.0f;	// 1080p height.
};

// Constants used to calculate screen rotations
namespace ScreenRotation
{
	// 0-degree Z-rotation
	static const XMFLOAT4X4 Rotation0(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	// 90-degree Z-rotation
	static const XMFLOAT4X4 Rotation90(
		0.0f, 1.0f, 0.0f, 0.0f,
		-1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	// 180-degree Z-rotation
	static const XMFLOAT4X4 Rotation180(
		-1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	// 270-degree Z-rotation
	static const XMFLOAT4X4 Rotation270(
		0.0f, -1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);
};

// Constructor for DeviceResources.
DX::DeviceResources::DeviceResources() :
	m_screenViewport(),
	m_d3dFeatureLevel(D3D_FEATURE_LEVEL_9_1),
	m_d3dRenderTargetSize(),
	m_outputSize(),
	m_logicalSize(),
	m_nativeOrientation(DisplayOrientations::None),
	m_currentOrientation(DisplayOrientations::None),
	m_dpi(-1.0f),
	m_effectiveDpi(-1.0f),
	m_deviceNotify(nullptr)
{
	CreateDeviceIndependentResources();
	CreateDeviceResources(nullptr, 0);
}

// Configures resources that don't depend on the Direct3D device.
void DX::DeviceResources::CreateDeviceIndependentResources()
{
	// Initialize Direct2D resources.
	D2D1_FACTORY_OPTIONS options;
	ZeroMemory(&options, sizeof(D2D1_FACTORY_OPTIONS));

#if defined(_DEBUG)
	// If the project is in a debug build, enable Direct2D debugging via SDK Layers.
	options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

	// Initialize the Direct2D Factory.
	DX::ThrowIfFailed(
		D2D1CreateFactory(
			D2D1_FACTORY_TYPE_SINGLE_THREADED,
			m_d2dFactory.ReleaseAndGetAddressOf()
		)
	);

	// Initialize the DirectWrite Factory.
	DX::ThrowIfFailed(
		DWriteCreateFactory(
			DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(IDWriteFactory3),
			reinterpret_cast<IUnknown**>(m_dwriteFactory.ReleaseAndGetAddressOf())
		)
	);

	// Initialize the Windows Imaging Component (WIC) Factory.
	DX::ThrowIfFailed(
		CoCreateInstance(
			CLSID_WICImagingFactory2,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(m_wicFactory.ReleaseAndGetAddressOf())
		)
	);
}

std::vector <IDXGIAdapter*> vAdapters;
bool forceAutoLang = false;
bool D3DFeatureLevelGlobal = false;
// Configures the Direct3D device, and stores handles to it and the device context.
void DX::DeviceResources::CreateDeviceResources(IDXGIAdapter* vAdapter, int forceAutoLange)
{
	D3DFeatureLevelGlobal = false;
	// This flag adds support for surfaces with a different color channel ordering
	// than the API default. It is required for compatibility with Direct2D.
	UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
	if (DX::SdkLayersAvailable())
	{
		// If the project is in a debug build, enable debugging via SDK Layers with this flag.
		creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
	}
#endif

	// This array defines the set of DirectX hardware feature levels this app will support.
	// Note the ordering should be preserved.
	// Don't forget to declare your application's minimum required feature level in its
	// description.  All applications are assumed to support 9.1 unless otherwise stated.

	std::vector<D3D_FEATURE_LEVEL> featureLevels = {
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
	if (!forceAutoLange) {
		if (g_Config.sShaderLanguage == "Level 9.1")
		{
			featureLevels =
			{
				D3D_FEATURE_LEVEL_9_1
			};
			D3DFeatureLevelGlobal = true;
		}
		else
		if (g_Config.sShaderLanguage == "Level 9.3")
		{
			featureLevels =
			{
				D3D_FEATURE_LEVEL_9_3,
				D3D_FEATURE_LEVEL_9_2,
				D3D_FEATURE_LEVEL_9_1
			};
			D3DFeatureLevelGlobal = true;
		}
		else
			if (g_Config.sShaderLanguage == "Level 10")
			{
				featureLevels =
				{
					D3D_FEATURE_LEVEL_10_1,
					D3D_FEATURE_LEVEL_10_0,
				};
			}
			else
				if (g_Config.sShaderLanguage == "Level 11")
				{
					featureLevels =
					{
						D3D_FEATURE_LEVEL_11_1,
						D3D_FEATURE_LEVEL_11_0,
					};
				}
				else
					if (g_Config.sShaderLanguage == "Level 12")
					{
						featureLevels =
						{
							D3D_FEATURE_LEVEL_12_1,
							D3D_FEATURE_LEVEL_12_0,
						};
					}
	}

	// Create the Direct3D 11 API device object and a corresponding context.
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;

	auto type = (vAdapter != nullptr ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE);

	HRESULT hr = D3D11CreateDevice(
		vAdapter,					// Specify nullptr to use the default adapter.
		type,						// Create a device using the hardware graphics driver.
		0,							// Should be 0 unless the driver is D3D_DRIVER_TYPE_SOFTWARE.
		creationFlags,				// Set debug and Direct2D compatibility flags.
		featureLevels.data(),		// List of feature levels this app can support.
		featureLevels.size(),		// Size of the list above.
		D3D11_SDK_VERSION,			// Always set this to D3D11_SDK_VERSION for Windows Store apps.
		&device,					// Returns the Direct3D device created.
		&m_d3dFeatureLevel,			// Returns feature level of device created.
		&context					// Returns the device immediate context.
	);

	if (FAILED(hr))
	{
		ERROR_LOG(G3D, "Cannot create D3D device, fall to D3D_DRIVER_TYPE_WARP.");
		// If the initialization fails, fall back to the WARP device.
		// For more information on WARP, see: 
		// http://go.microsoft.com/fwlink/?LinkId=286690
		bool createWarp = false;
		if (vAdapter != nullptr) {
			INFO_LOG(G3D, "Trying with vAdapter first.");
			auto hr = D3D11CreateDevice(
				vAdapter,
				D3D_DRIVER_TYPE_WARP, // Create a WARP device instead of a hardware device.
				0,
				creationFlags,
				featureLevels.data(),
				featureLevels.size(),
				D3D11_SDK_VERSION,
				&device,
				&m_d3dFeatureLevel,
				&context
			);

			if (FAILED(hr))
			{
				createWarp = true;
				ERROR_LOG(G3D, "Failed to create warp with vAdapter, fallback to default warp.");
			}
			else {
				INFO_LOG(G3D, "Warp with vAdapter created.");
			}
		}
		else {
			INFO_LOG(G3D, "No custom adapter detected.");
			createWarp = true;
		}

		if (createWarp) {
			hr =
				D3D11CreateDevice(
					nullptr,
					D3D_DRIVER_TYPE_WARP, // Create a WARP device instead of a hardware device.
					0,
					creationFlags,
					featureLevels.data(),
					featureLevels.size(),
					D3D11_SDK_VERSION,
					&device,
					&m_d3dFeatureLevel,
					&context
				);
			INFO_LOG(G3D, "Default warp created.");
		}
	}
	else if (vAdapter == nullptr) {
		auto currentAdapter = g_Config.sD3D11Device;
		try {
			ComPtr<IDXGIDevice3> dxgi_device;
			DX::ThrowIfFailed(
				device.As(&dxgi_device)
			);
			//if (vAdapters.empty()) 
			{
				if (!vAdapters.empty()) {
					for (IDXGIAdapter* vAdapter : vAdapters) {
						vAdapter->Release();
						vAdapter = nullptr;
					}
					vAdapters.clear();
				}
				Microsoft::WRL::ComPtr<IDXGIAdapter> device_adapter;
				dxgi_device->GetAdapter(&device_adapter);

				Microsoft::WRL::ComPtr<IDXGIFactory4> device_factory;
				device_adapter->GetParent(IID_PPV_ARGS(&device_factory));

				UINT i = 0;
				IDXGIAdapter* pAdapter;
				while (device_factory->EnumAdapters(i, &pAdapter) != DXGI_ERROR_NOT_FOUND)
				{
					vAdapters.push_back(pAdapter);
					++i;
				}
				device_factory->Release();
			}
		}
		catch (...) {
		}

		if (!vAdapters.empty() && !currentAdapter.empty()) {
			INFO_LOG(G3D, "Found custom adapter: %s., checking adapters list", currentAdapter.c_str());
			for (IDXGIAdapter* adapter : vAdapters) {
				DXGI_ADAPTER_DESC vAdapterDesc;
				adapter->GetDesc(&vAdapterDesc);
				auto wdescs = std::wstring(vAdapterDesc.Description);
				auto sdescs = convert(wdescs);
				INFO_LOG(G3D, "Checking match with: %s.", sdescs.c_str());
				if (sdescs == currentAdapter) {
					INFO_LOG(G3D, "Switching to: %s.", currentAdapter.c_str());
					CreateDeviceResources(adapter, forceAutoLange);
					return;
				}
			}
			INFO_LOG(G3D, "No matches found in the adapters list, selecting default adapter.");
		}
		else {
			if (vAdapters.empty()) {
				INFO_LOG(G3D, "No adapters in vAdapters list, selecting default adapter.");
			}
			else {
				INFO_LOG(G3D, "No custom adapter detected, selecting default adapter.");
			}
		}
	}
	else if (vAdapter != nullptr) {
		INFO_LOG(G3D, "D3D device created with custom adapter");
	}

	if (FAILED(hr)) {
		ERROR_LOG(G3D, "D3D device failed with error: %d", hr);
		INFO_LOG(G3D, "Trying with auto configs");
		if (!forceAutoLange) {
			g_Config.bBackwardCompatibility = true;
			CreateDeviceResources(vAdapter, 1);
			return;
		}
		else {
			ERROR_LOG(G3D, "D3D device failed with error: %d", hr);
			DX::ThrowIfFailed(hr);
		}
		return;
	}
	else {
		if (forceAutoLange) {
			g_Config.bBackwardCompatibility = true;
			g_Config.sShaderLanguage = "Auto";
			forceAutoLang = true;
		}
	}

	if (m_d3dFeatureLevel == D3D_FEATURE_LEVEL_9_3 || m_d3dFeatureLevel == D3D_FEATURE_LEVEL_9_2 || m_d3dFeatureLevel == D3D_FEATURE_LEVEL_9_1) {
		D3DFeatureLevelGlobal = true;
		g_Config.bFogState = false;
		g_Config.bforceFloatShader = true;
	}

	// Store pointers to the Direct3D 11.3 API device and immediate context.
	DX::ThrowIfFailed(
		device.As(&m_d3dDevice)
	);

	DX::ThrowIfFailed(
		context.As(&m_d3dContext)
	);

	DX::ThrowIfFailed(
		m_d3dDevice.As(&m_dxgiDevice)
	);

	DX::ThrowIfFailed(
		m_dxgiDevice->GetAdapter(&m_dxgiAdapter)
	);

	DX::ThrowIfFailed(
		m_dxgiAdapter->GetParent(IID_PPV_ARGS(&m_dxgiFactory))
	);

	if (!deviceLost) {
		// Create the Direct2D device object and a corresponding context.
		DX::ThrowIfFailed(
			m_d2dFactory->CreateDevice(m_dxgiDevice.Get(), &m_d2dDevice)
		);

		DX::ThrowIfFailed(
			m_d2dDevice->CreateDeviceContext(
				D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
				&m_d2dContext
			)
		);
	}
}

// These resources need to be recreated every time the window size is changed.
void DX::DeviceResources::CreateWindowSizeDependentResources(bool deviceLost)
{
	auto coreWindow = CoreWindow::GetForCurrentThread();

	DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

#if !defined(_M_ARM) && !defined(BUILD14393)
	if (Windows::System::Profile::AnalyticsInfo::VersionInfo->DeviceFamily == L"Windows.Xbox")
	{
		const auto hdi = Windows::Graphics::Display::Core::HdmiDisplayInformation::GetForCurrentView();
		if (hdi)
		{
			try
			{
				const auto dm = hdi->GetCurrentDisplayMode();
				const float hdmi_width = (float)dm->ResolutionWidthInRawPixels;
				const float hdmi_height = (float)dm->ResolutionHeightInRawPixels;
				// If we're running on Xbox, use the HDMI mode instead of the CoreWindow size.
				// In UWP, the CoreWindow is always 1920x1080, even when running at 4K.

				m_logicalSize = Windows::Foundation::Size(hdmi_width, hdmi_height);
				m_dpi = currentDisplayInformation->LogicalDpi * 1.5f;
			}
			catch (const Platform::Exception^)
			{
				m_logicalSize = Windows::Foundation::Size(coreWindow->Bounds.Width, coreWindow->Bounds.Height);
				m_dpi = currentDisplayInformation->LogicalDpi;
			}
		}
	}
	else
#endif
	{
		m_logicalSize = Windows::Foundation::Size(coreWindow->Bounds.Width, coreWindow->Bounds.Height);
		m_dpi = currentDisplayInformation->LogicalDpi;
	}
	m_nativeOrientation = currentDisplayInformation->NativeOrientation;
	m_currentOrientation = currentDisplayInformation->CurrentOrientation;

	m_d2dContext->SetDpi(m_dpi, m_dpi);

	// Clear the previous window size specific context.
	ID3D11RenderTargetView* nullViews[] = { nullptr };
	m_d3dContext->OMSetRenderTargets(ARRAYSIZE(nullViews), nullViews, nullptr);
	m_d3dRenderTargetView = nullptr;
	m_d2dContext->SetTarget(nullptr);
	m_d2dTargetBitmap = nullptr;
	m_d3dContext->Flush1(D3D11_CONTEXT_TYPE_ALL, nullptr);

	UpdateRenderTargetSize();

	// The width and height of the swap chain must be based on the window's
	// natively-oriented width and height. If the window is not in the native
	// orientation, the dimensions must be reversed.
	DXGI_MODE_ROTATION displayRotation = ComputeDisplayRotation();

	bool swapDimensions = displayRotation == DXGI_MODE_ROTATION_ROTATE90 || displayRotation == DXGI_MODE_ROTATION_ROTATE270;
	m_d3dRenderTargetSize.Width = swapDimensions ? m_outputSize.Height : m_outputSize.Width;
	m_d3dRenderTargetSize.Height = swapDimensions ? m_outputSize.Width : m_outputSize.Height;
	UINT flags = 0;
	switch (g_Config.bSwapFlags) {
	case 1:
		flags = DXGI_SWAP_CHAIN_FLAG::DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
		break;
	default:
		flags = 0;
		break;
	}
	if (m_swapChain != nullptr && !deviceLost)
	{
		// If the swap chain already exists, resize it.
		HRESULT hr = m_swapChain->ResizeBuffers(
			2, // Double-buffered swap chain.
			lround(m_d3dRenderTargetSize.Width),
			lround(m_d3dRenderTargetSize.Height),
			DXGI_FORMAT_B8G8R8A8_UNORM,
			flags
		);

		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
		{
			// If the device was removed for any reason, a new device and swap chain will need to be created.
			HandleDeviceLost();

			// Everything is set up now. Do not continue execution of this method. HandleDeviceLost will reenter this method 
			// and correctly set up the new device.
			return;
		}
		else
		{
			DX::ThrowIfFailed(hr);
		}
	}
	else
	{
		// Otherwise, create a new one using the same adapter as the existing Direct3D device.
		DXGI_SCALING scaling = DisplayMetrics::SupportHighResolutions ? DXGI_SCALING_NONE : DXGI_SCALING_STRETCH;
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };

		swapChainDesc.Width = lround(m_d3dRenderTargetSize.Width);		// Match the size of the window.
		swapChainDesc.Height = lround(m_d3dRenderTargetSize.Height);
		swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;				// This is the most common swap chain format.
		swapChainDesc.Stereo = false;
		swapChainDesc.SampleDesc.Count = 1;								// Don't use multi-sampling.
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 2;									// Use double-buffering to minimize latency.
		switch (g_Config.bSwapEffect)
		{
		case 0:
			swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;	// All Windows Store apps must use this SwapEffect.
			break;

		case 1:
			swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;	// All Windows Store apps must use this SwapEffect.
			break;
		}

		swapChainDesc.Flags = flags;

		swapChainDesc.Scaling = scaling;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

		// This sequence obtains the DXGI factory that was used to create the Direct3D device above.
		ComPtr<IDXGISwapChain1> swapChain;
		auto swapChainHR = m_dxgiFactory->CreateSwapChainForCoreWindow(
			m_d3dDevice.Get(),
			reinterpret_cast<IUnknown*>(coreWindow),
			&swapChainDesc,
			nullptr,
			&swapChain
		);
		WARN_LOG(G3D, "CreateSwapChainForCoreWindow: %d", swapChainHR);
		//DX::ThrowIfFailed(swapChainHR);
		if (SUCCEEDED(swapChainHR)) {
			swapChain.As(&m_swapChain);
		}

		// Ensure that DXGI does not queue more than one frame at a time. This both reduces latency and
		// ensures that the application will only render after each VSync, minimizing power consumption.
		DX::ThrowIfFailed(
			m_dxgiDevice->SetMaximumFrameLatency(1)
		);
	}

	// Set the proper orientation for the swap chain, and generate 2D and
	// 3D matrix transformations for rendering to the rotated swap chain.
	// Note the rotation angle for the 2D and 3D transforms are different.
	// This is due to the difference in coordinate spaces.  Additionally,
	// the 3D matrix is specified explicitly to avoid rounding errors.

	switch (displayRotation)
	{
	case DXGI_MODE_ROTATION_IDENTITY:
		m_orientationTransform2D = Matrix3x2F::Identity();
		m_orientationTransform3D = ScreenRotation::Rotation0;
		break;

	case DXGI_MODE_ROTATION_ROTATE90:
		m_orientationTransform2D =
			Matrix3x2F::Rotation(90.0f) *
			Matrix3x2F::Translation(m_logicalSize.Height, 0.0f);
		m_orientationTransform3D = ScreenRotation::Rotation270;
		break;

	case DXGI_MODE_ROTATION_ROTATE180:
		m_orientationTransform2D =
			Matrix3x2F::Rotation(180.0f) *
			Matrix3x2F::Translation(m_logicalSize.Width, m_logicalSize.Height);
		m_orientationTransform3D = ScreenRotation::Rotation180;
		break;

	case DXGI_MODE_ROTATION_ROTATE270:
		m_orientationTransform2D =
			Matrix3x2F::Rotation(270.0f) *
			Matrix3x2F::Translation(0.0f, m_logicalSize.Width);
		m_orientationTransform3D = ScreenRotation::Rotation90;
		break;

	default:
		throw ref new FailureException();
	}

	DX::ThrowIfFailed(
		m_swapChain->SetRotation(displayRotation)
	);

	// Create a render target view of the swap chain back buffer.
	ComPtr<ID3D11Texture2D1> backBuffer;
	DX::ThrowIfFailed(
		m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))
	);

	DX::ThrowIfFailed(
		m_d3dDevice->CreateRenderTargetView1(
			backBuffer.Get(),
			nullptr,
			&m_d3dRenderTargetView
		)
	);

	// Set the 3D rendering viewport to target the entire window.
	m_screenViewport = CD3D11_VIEWPORT(
		0.0f,
		0.0f,
		m_d3dRenderTargetSize.Width,
		m_d3dRenderTargetSize.Height
	);

	m_d3dContext->RSSetViewports(1, &m_screenViewport);

	// Create a Direct2D target bitmap associated with the
	// swap chain back buffer and set it as the current target.
	D2D1_BITMAP_PROPERTIES1 bitmapProperties =
		D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
			m_dpi,
			m_dpi
		);

	ComPtr<IDXGISurface2> dxgiBackBuffer;
	DX::ThrowIfFailed(
		m_swapChain->GetBuffer(0, IID_PPV_ARGS(&dxgiBackBuffer))
	);

	DX::ThrowIfFailed(
		m_d2dContext->CreateBitmapFromDxgiSurface(
			dxgiBackBuffer.Get(),
			&bitmapProperties,
			&m_d2dTargetBitmap
		)
	);

	m_d2dContext->SetTarget(m_d2dTargetBitmap.Get());
	m_d2dContext->SetDpi(m_effectiveDpi, m_effectiveDpi);

	// Grayscale text anti-aliasing is recommended for all Windows Store apps.
	m_d2dContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
}

// Determine the dimensions of the render target and whether it will be scaled down.
void DX::DeviceResources::UpdateRenderTargetSize()
{
	m_effectiveDpi = m_dpi;
	if (Windows::System::Profile::AnalyticsInfo::VersionInfo->DeviceFamily == L"Windows.Xbox")
	{
		m_effectiveDpi = 96.0f / static_cast<float>(m_logicalSize.Height) * 1080.0f;
	}
	else
	{
		// To improve battery life on high resolution devices, render to a smaller render target
		// and allow the GPU to scale the output when it is presented.
		if (!DisplayMetrics::SupportHighResolutions && m_dpi >= DisplayMetrics::DpiThreshold)
		{
			float width = DX::ConvertDipsToPixels(m_logicalSize.Width, m_dpi);
			float height = DX::ConvertDipsToPixels(m_logicalSize.Height, m_dpi);

			// When the device is in portrait orientation, height > width. Compare the
			// larger dimension against the width threshold and the smaller dimension
			// against the height threshold.
			if (std::max(width, height) > DisplayMetrics::WidthThreshold && std::min(width, height) > DisplayMetrics::HeightThreshold)
			{
				// To scale the app we change the effective DPI. Logical size does not change.
				//m_effectiveDpi /= 2.0f;
			}
		}
	}
	// Calculate the necessary render target size in pixels.
	m_outputSize.Width = DX::ConvertDipsToPixels(m_logicalSize.Width, m_effectiveDpi);
	m_outputSize.Height = DX::ConvertDipsToPixels(m_logicalSize.Height, m_effectiveDpi);

	// Prevent zero size DirectX content from being created.
	m_outputSize.Width = std::max(m_outputSize.Width, 1.0f);
	m_outputSize.Height = std::max(m_outputSize.Height, 1.0f);
}

// This method is called in the event handler for the SizeChanged event.
void DX::DeviceResources::SetLogicalSize(Windows::Foundation::Size logicalSize)
{
	if (m_logicalSize != logicalSize)
	{
		m_logicalSize = logicalSize;
		CreateWindowSizeDependentResources();
	}
}

// This method is called in the event handler for the DpiChanged event.
void DX::DeviceResources::SetDpi(float dpi)
{
	if (dpi != m_dpi)
	{
		m_dpi = dpi;
		CreateWindowSizeDependentResources();
	}
}

// This method is called in the event handler for the OrientationChanged event.
void DX::DeviceResources::SetCurrentOrientation(DisplayOrientations currentOrientation)
{
	if (m_currentOrientation != currentOrientation)
	{
		m_currentOrientation = currentOrientation;
		CreateWindowSizeDependentResources();
	}
}

// Recreate all device resources and set them back to the current state.
void DX::DeviceResources::HandleDeviceLost()
{
	if (deviceLost) {
		return;
	}
	deviceLost = true;
	if (m_deviceNotify != nullptr)
	{
		m_deviceNotify->OnDeviceLost();
	}

	m_d3dContext->Flush();

	/*m_d2dDevice->Release();
	m_d2dDevice = nullptr;
	m_d2dFactory->Release();
	m_d2dFactory = nullptr;
	m_dwriteFactory->Release();
	m_dwriteFactory = nullptr;
	m_wicFactory->Release();
	m_wicFactory = nullptr;*/

	if (!vAdapters.empty()) {
		for (IDXGIAdapter* vAdapter : vAdapters) {
			vAdapter->Release();
			vAdapter = nullptr;
		}
		vAdapters.clear();
	}

	m_dxgiFactory->Release();
	m_dxgiFactory = nullptr;
	m_dxgiAdapter->Release();
	m_dxgiAdapter = nullptr;
	m_dxgiDevice->Release();
	m_dxgiDevice = nullptr;
	m_d3dDevice->Release();
	m_d3dDevice = nullptr;

	m_swapChain->Release();
	m_swapChain = nullptr;

	//CreateDeviceIndependentResources();
	CreateDeviceResources(nullptr, 0);
	CreateWindowSizeDependentResources(true);

	if (m_deviceNotify != nullptr)
	{
		m_deviceNotify->OnDeviceRestored();
	}
	deviceLost = false;
}

// Register our DeviceNotify to be informed on device lost and creation.
void DX::DeviceResources::RegisterDeviceNotify(DX::IDeviceNotify* deviceNotify)
{
	m_deviceNotify = deviceNotify;
}

// Call this method when the app suspends. It provides a hint to the driver that the app 
// is entering an idle state and that temporary buffers can be reclaimed for use by other apps.
void DX::DeviceResources::Trim()
{
	m_dxgiDevice->Trim();
}

// Present the contents of the swap chain to the screen.
void DX::DeviceResources::Present()
{
	// The first argument instructs DXGI to block until VSync, putting the application
	// to sleep until the next VSync. This ensures we don't waste any cycles rendering
	// frames that will never be displayed to the screen.
	HRESULT hr = m_swapChain->Present(1, DXGI_PRESENT_TEST);

	// If the device was removed either by a disconnection or a driver upgrade, we 
	// must recreate all device resources.
	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
	{
		auto lostReason = m_d3dDevice->GetDeviceRemovedReason();
		WARN_LOG(G3D, "Device lost, reason: %d", lostReason);
		HandleDeviceLost();
	}
	else
	{
		DX::ThrowIfFailed(hr);
	}
}

// This method determines the rotation between the display device's native orientation and the
// current display orientation.
DXGI_MODE_ROTATION DX::DeviceResources::ComputeDisplayRotation()
{
	DXGI_MODE_ROTATION rotation = DXGI_MODE_ROTATION_UNSPECIFIED;

	// Note: NativeOrientation can only be Landscape or Portrait even though
	// the DisplayOrientations enum has other values.
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
