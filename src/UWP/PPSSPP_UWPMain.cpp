#include "pch.h"
#include "PPSSPP_UWPMain.h"

#include <mutex>
#include <list>
#include <memory>

#include "Common/File/FileUtil.h"
#include "Common/Net/HTTPClient.h"
#include "Common/Net/Resolve.h"
#include "Common/GPU/thin3d_create.h"

#include "Common/Common.h"
#include "Common/Input/InputState.h"
#include "Common/File/VFS/VFS.h"
#include "Common/File/VFS/DirectoryReader.h"
#include "Common/Thread/ThreadUtil.h"
#include "Common/Data/Encoding/Utf8.h"
#include "Common/DirectXHelper.h"
#include "Common/File/FileUtil.h"
#include "Common/Log.h"
#include "Common/LogManager.h"
#include "Common/TimeUtil.h"
#include "Common/StringUtils.h"
#include "Common/System/Display.h"
#include "Common/System/NativeApp.h"
#include "Common/System/Request.h"

#include "Core/System.h"
#include "Core/Loaders.h"
#include "Core/Config.h"

#include "Windows/InputDevice.h"
#include "Windows/XinputDevice.h"
#include "NKCodeFromWindowsSystem.h"
#include "XAudioSoundStream.h"
#include "UWPUtil.h"
#include "App.h"

// UWP Storage helper includes
#include "UWPHelpers/StorageManager.h"
#include "UWPHelpers/StorageAsync.h"
#include "UWPHelpers/LaunchItem.h"
#include <UWPHelpers/UIHelpers.h>
#include <UWPHelpers/StorageWin32.h>
#include <Common/OSVersion.h>
#include <mutex>

using namespace UWP;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;
using namespace Windows::System::Threading;
using namespace Windows::ApplicationModel::DataTransfer;
using namespace Windows::Devices::Enumeration;
using namespace Concurrency;
using namespace Windows::Graphics::Display;

// UGLY!
PPSSPP_UWPMain* g_main;
extern WindowsAudioBackend* winAudioBackend;
std::string langRegion;
std::list<std::unique_ptr<InputDevice>> g_input;

bool mainMenuVisible = false;
extern float scaleAmount;
extern DisplayOrientations currentOrientation;
// TODO: Use Microsoft::WRL::ComPtr<> for D3D11 objects?
// TODO: See https://github.com/Microsoft/Windows-universal-samples/tree/master/Samples/WindowsAudioSession for WASAPI with UWP
// TODO: Low latency input: https://github.com/Microsoft/Windows-universal-samples/tree/master/Samples/LowLatencyInput/cpp

extern std::vector <IDXGIAdapter*> vAdapters;
extern bool appStarted;
extern bool appActivated;
extern IActivatedEventArgs^ argsEx;
// Loads and initializes application assets when the application is loaded.
PPSSPP_UWPMain::PPSSPP_UWPMain(App^ app, const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	app_(app),
	m_deviceResources(deviceResources)
{

	while (!appStarted) {
		CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
	}
	
	DisplayInformation^ displayInfo = DisplayInformation::GetForCurrentView();
	currentOrientation = displayInfo->CurrentOrientation;

	LinkAccelerometer();
	g_main = this;

	net::Init();

	// Register to be notified if the Device is lost or recreated
	m_deviceResources->RegisterDeviceNotify(this);

	// create_task(KnownFolders::GetFolderForUserAsync(nullptr, KnownFolderId::RemovableDevices)).then([this](StorageFolder ^));

	// TODO: Change the timer settings if you want something other than the default variable timestep mode.
	// e.g. for 60 FPS fixed timestep update logic, call:
	/*
	m_timer.SetFixedTimeStep(true);
	m_timer.SetTargetElapsedSeconds(1.0 / 60);
	*/

	ctx_.reset(new UWPGraphicsContext(deviceResources, vAdapters));

	const Path& exePath = File::GetExeDirectory();
	g_VFS.Register("", new DirectoryReader(exePath / "Content"));
	g_VFS.Register("", new DirectoryReader(exePath));

	wchar_t lcCountry[256];

	if (0 != GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SNAME, lcCountry, 256)) {
		langRegion = ConvertWStringToUTF8(lcCountry);
		for (size_t i = 0; i < langRegion.size(); i++) {
			if (langRegion[i] == '-')
				langRegion[i] = '_';
		}
	}
	else {
		langRegion = "en_US";
	}
	CompleteLoading();
}

volatile bool ppssppShutingDown = false;
volatile bool deviceRestoreInProgress = false;
void PPSSPP_UWPMain::deviceLostMonitor() {
	concurrency::create_task([&] {
		while (!ppssppShutingDown) {
			/*if (g_Config.bDetectDeviceLose) {
				if (mainMenuVisible) {
					if (!deviceRestoreInProgress) {
						deviceRestoreInProgress = true;
						try {
							m_deviceResources->Present();
						}
						catch (Platform::Exception^ exception_)
						{
							ERROR_LOG(G3D, convertToChar(exception_->Message));
						}
						deviceRestoreInProgress = false;
						std::this_thread::sleep_for(std::chrono::milliseconds(100));
					}
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
			else {
				std::this_thread::sleep_for(std::chrono::milliseconds(1500));
			}*/
			std::this_thread::sleep_for(std::chrono::milliseconds(5000));
			Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(
				CoreDispatcherPriority::Normal,
				ref new Windows::UI::Core::DispatchedHandler([&]()
					{
						m_deviceResources->HandleDeviceLost();
					}));
			break;
		}
	});
}

void PPSSPP_UWPMain::CompleteLoading() {
	const char* argv[2] = { "fake", nullptr };
	int argc = 1;

	std::string cacheFolder = ConvertWStringToUTF8(ApplicationData::Current->TemporaryFolder->Path->Data());

	// 'PPSSPP_UWPMain' is getting called before 'OnActivated'
	// this make detecting launch items on startup hard
	// I think 'Init' process must be moved out and invoked from 'OnActivated' one time only
	// currently launchItem will work fine but we cannot skip logo screen
	// we should pass file path to 'argv' using 'GetLaunchItemPath(args)' 
	// instead of depending on 'boot_filename' (LaunchItem.cpp)
	NativeInit(argc, argv, "", "", cacheFolder.c_str());

#ifdef _M_ARM
	System_Notify(SystemNotification::ROTATE_UPDATED);
#endif

	NativeInitGraphics(ctx_.get());
	System_NotifyUIState("resize");

	int width = m_deviceResources->GetScreenViewport().Width;
	int height = m_deviceResources->GetScreenViewport().Height;

	ctx_->GetDrawContext()->HandleEvent(Draw::Event::GOT_BACKBUFFER, width, height, m_deviceResources->GetBackBufferRenderTargetView());

	// add first XInput device to respond
	g_input.push_back(std::make_unique<XinputDevice>());

	InputDevice::BeginPolling();
	//deviceLostMonitor();
}


PPSSPP_UWPMain::~PPSSPP_UWPMain() {
	ppssppShutingDown = true;
	InputDevice::StopPolling();
	ctx_->GetDrawContext()->HandleEvent(Draw::Event::LOST_BACKBUFFER, 0, 0, nullptr);
	NativeShutdownGraphics();
	NativeShutdown();
	g_VFS.Clear();

	// Deregister device notification
	m_deviceResources->RegisterDeviceNotify(nullptr);
	net::Shutdown();
}

// Updates application state when the window size changes (e.g. device orientation change)
void PPSSPP_UWPMain::CreateWindowSizeDependentResources() {
	ctx_->GetDrawContext()->HandleEvent(Draw::Event::LOST_BACKBUFFER, 0, 0, nullptr);

	System_NotifyUIState("resize");

	int width = m_deviceResources->GetScreenViewport().Width;
	int height = m_deviceResources->GetScreenViewport().Height;
	ctx_->GetDrawContext()->HandleEvent(Draw::Event::GOT_BACKBUFFER, width, height, m_deviceResources->GetBackBufferRenderTargetView());
}

bool startupDone = false;
bool updateScreen = true;
bool recreateView = false;
bool resizeBufferRequested = false;
static bool hasSetThreadName = false;

void PPSSPP_UWPMain::updateScreenState() {
	auto context = m_deviceResources->GetD3DDeviceContext();

	switch (m_deviceResources->ComputeDisplayRotation()) {
	case DXGI_MODE_ROTATION_IDENTITY: g_display.rotation = DisplayRotation::ROTATE_0; break;
	case DXGI_MODE_ROTATION_ROTATE90: g_display.rotation = DisplayRotation::ROTATE_90; break;
	case DXGI_MODE_ROTATION_ROTATE180: g_display.rotation = DisplayRotation::ROTATE_180; break;
	case DXGI_MODE_ROTATION_ROTATE270: g_display.rotation = DisplayRotation::ROTATE_270; break;
	}
	// Not super elegant but hey.
	memcpy(&g_display.rot_matrix, &m_deviceResources->GetOrientationTransform3D(), sizeof(float) * 16);

	// Reset the viewport to target the whole screen.
	auto viewport = m_deviceResources->GetScreenViewport();

	g_display.pixel_xres = lround(viewport.Width);
	g_display.pixel_yres = lround(viewport.Height);

	if (g_display.rotation == DisplayRotation::ROTATE_90 || g_display.rotation == DisplayRotation::ROTATE_270) {
		// We need to swap our width/height.
		std::swap(g_display.pixel_xres, g_display.pixel_yres);
	}

	g_display.dpi = m_deviceResources->GetActualDpi();

	// Boost DPI a bit to look better.
	if (g_Config.bDPIBoost > 0) {
		g_display.dpi *= g_Config.bDPIBoost / (136.0f);
	}

	g_display.dpi_scale_x = (96.0f / g_display.dpi);
	g_display.dpi_scale_y = (96.0f / g_display.dpi);

	g_display.pixel_in_dps_x = 1.0f / g_display.dpi_scale_x;
	g_display.pixel_in_dps_y = 1.0f / g_display.dpi_scale_y;

	g_display.dp_xres = g_display.pixel_xres * g_display.dpi_scale_x;
	g_display.dp_yres = g_display.pixel_yres * g_display.dpi_scale_y;

	context->RSSetViewports(1, &viewport);
}
// Renders the current frame according to the current application state.
// Returns true if the frame was rendered and is ready to be displayed.
volatile int skip = 0;
volatile bool release = false;

bool isThreadRenderRunning = false;
void PPSSPP_UWPMain::PPSSPPFrame() {
	if (resizeBufferRequested) {
		resizeBufferRequested = false;
		scaleAmount = g_Config.bQualityControl;
		m_deviceResources->SetQuality(scaleAmount);
		CreateWindowSizeDependentResources();
		updateScreen = true;
	}
	if (!mainMenuVisible) {
		System_NotifyUIState("resize");
	}

	if (updateScreen) {
		PPSSPP_UWPMain::updateScreenState();
		concurrency::create_task([&] {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			System_PostUIMessage("resetdpad", "");
			});
		updateScreen = !mainMenuVisible;
	}

	if (recreateView) {
		recreateView = false;
		concurrency::create_task([&] {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			System_PostUIMessage("createall", "");
			});
	}

	if (!hasSetThreadName) {
		SetCurrentThreadName("UWPRenderThread");
		hasSetThreadName = true;
	}

	if (!m_deviceResources->deviceLost) {
		NativeFrame(ctx_.get());
	}
}
extern bool m_windowClosed;
extern bool m_windowVisible;
void PPSSPP_UWPMain::threadPPSSPPFrameRender() {
	/*concurrency::create_task([&] {
		isThreadRenderRunning = true;

		while (!m_windowClosed) {
			if (m_windowVisible) {
				PPSSPPFrame();
			}
		}

		isThreadRenderRunning = false;
		});*/
}
bool PPSSPP_UWPMain::Render() {
	if (!isThreadRenderRunning) {
		PPSSPPFrame();
	}

	return true;
}

// Notifies renderers that device resources need to be released.
void PPSSPP_UWPMain::OnDeviceLost() {
	WARN_LOG(G3D, "Device lost: LOST_DEVICE");
	ctx_->GetDrawContext()->HandleEvent(Draw::Event::LOST_DEVICE, 0, 0, nullptr);
}

// Notifies renderers that device resources may now be recreated.
void PPSSPP_UWPMain::OnDeviceRestored() {
	WARN_LOG(G3D, "Device restored: GOT_DEVICE");
	CreateWindowSizeDependentResources();
	ctx_->GetDrawContext()->HandleEvent(Draw::Event::GOT_DEVICE, 0, 0, nullptr);
}

void PPSSPP_UWPMain::OnKeyDown(int scanCode, Windows::System::VirtualKey virtualKey, int repeatCount) {
	auto iter = virtualKeyCodeToNKCode.find(virtualKey);
	if (iter != virtualKeyCodeToNKCode.end()) {
		KeyInput key{};
		key.deviceId = DEVICE_ID_KEYBOARD;
		key.keyCode = (InputKeyCode)iter->second;
		key.flags = KEY_DOWN | (repeatCount > 1 ? KEY_IS_REPEAT : 0);
		NativeKey(key);
	}
#if !defined(NO_UI_HELPER) && !defined(__LIBRETRO__)
	//Here we can capture keyboard keys and send it to the target TextEdit
	SendKeyToTextEdit((int)virtualKey, (repeatCount > 1 ? KEY_IS_REPEAT : 0), KEY_DOWN);
#endif
}

void PPSSPP_UWPMain::OnKeyUp(int scanCode, Windows::System::VirtualKey virtualKey) {
	auto iter = virtualKeyCodeToNKCode.find(virtualKey);
	if (iter != virtualKeyCodeToNKCode.end()) {
		KeyInput key{};
		key.deviceId = DEVICE_ID_KEYBOARD;
		key.keyCode = (InputKeyCode)iter->second;
		key.flags = KEY_UP;
		NativeKey(key);
	}
}

void PPSSPP_UWPMain::OnMouseWheel(float delta) {
	int key = NKCODE_EXT_MOUSEWHEEL_UP;
	if (delta < 0) {
		key = NKCODE_EXT_MOUSEWHEEL_DOWN;
	}
	else if (delta == 0) {
		return;
	}

	KeyInput keyInput{};
	keyInput.keyCode = (InputKeyCode)key;
	keyInput.deviceId = DEVICE_ID_MOUSE;
	keyInput.flags = KEY_DOWN | KEY_UP;
	NativeKey(keyInput);
}

bool PPSSPP_UWPMain::OnHardwareButton(HardwareButton button) {
	KeyInput keyInput{};
	keyInput.deviceId = DEVICE_ID_KEYBOARD;
	keyInput.flags = KEY_DOWN | KEY_UP;
	switch (button) {
	case HardwareButton::BACK:
		keyInput.keyCode = NKCODE_BACK;
		return NativeKey(keyInput);
	default:
		return false;
	}
}

void PPSSPP_UWPMain::OnTouchEvent(int touchEvent, int touchId, float x, float y, double timestamp) {
	// We get the coordinate in Windows' device independent pixels already. So let's undo that,
	// and then apply our own "dpi".
	float dpiFactor_x = m_deviceResources->GetActualDpi() / 96.0f;
	float dpiFactor_y = dpiFactor_x;
	dpiFactor_x /= g_display.pixel_in_dps_x;
	dpiFactor_y /= g_display.pixel_in_dps_y;

	TouchInput input{};
	input.id = touchId;
	input.x = x * dpiFactor_x;
	input.y = y * dpiFactor_y;
	input.flags = touchEvent;
	input.timestamp = timestamp;
	NativeTouch(input);

	KeyInput key{};
	key.deviceId = DEVICE_ID_MOUSE;
	if (touchEvent & TOUCH_DOWN) {
		key.keyCode = NKCODE_EXT_MOUSEBUTTON_1;
		key.flags = KEY_DOWN;
		NativeKey(key);
	}
	if (touchEvent & TOUCH_UP) {
		key.keyCode = NKCODE_EXT_MOUSEBUTTON_1;
		key.flags = KEY_UP;
		NativeKey(key);
	}
}

void PPSSPP_UWPMain::OnSuspend() {
	// TODO
}

UWPGraphicsContext::UWPGraphicsContext(std::shared_ptr<DX::DeviceResources> resources, std::vector <IDXGIAdapter*> vAdapters) {
	std::vector<std::string> adapterNames;
	if (!vAdapters.empty()) {
		for (IDXGIAdapter* vAdapter : vAdapters) {
			DXGI_ADAPTER_DESC vAdapterDesc;
			vAdapter->GetDesc(&vAdapterDesc);
			auto wdescs = std::wstring(vAdapterDesc.Description);
			adapterNames.push_back(convert(wdescs));
		}
	}

	draw_ = Draw::T3DCreateD3D11Context(
		resources->GetD3DDevice(), resources->GetD3DDeviceContext(), resources->GetD3DDevice(), resources->GetD3DDeviceContext(), resources->GetSwapChain(), resources->GetDeviceFeatureLevel(), 0, adapterNames, g_Config.iInflightFrames);
	bool success = draw_->CreatePresets();
	_assert_(success);
}

void UWPGraphicsContext::Shutdown() {
	delete draw_;
}

std::string System_GetProperty(SystemProperty prop) {
	static bool hasCheckedGPUDriverVersion = false;
	switch (prop) {
	case SYSPROP_NAME:
		return GetWindowsVersion();
	case SYSPROP_LANGREGION:
		return langRegion;
	case SYSPROP_CLIPBOARD_TEXT:
		/* TODO: Need to either change this API or do this on a thread in an ugly fashion.
		DataPackageView ^view = Clipboard::GetContent();
		if (view) {
			string text = await view->GetTextAsync();
		}
		*/
		return "";
	case SYSPROP_GPUDRIVER_VERSION:
		return "";
	case SYSPROP_BUILD_VERSION:
		return PPSSPP_GIT_VERSION;
	default:
		return "";
	}
}

std::vector<std::string> System_GetPropertyStringVec(SystemProperty prop) {
	std::vector<std::string> result;
	switch (prop) {
	case SYSPROP_TEMP_DIRS:
	{
		std::wstring tempPath(MAX_PATH, '\0');
		size_t sz = GetTempPath((DWORD)tempPath.size(), &tempPath[0]);
		if (sz >= tempPath.size()) {
			tempPath.resize(sz);
			sz = GetTempPath((DWORD)tempPath.size(), &tempPath[0]);
		}
		// Need to resize off the null terminator either way.
		tempPath.resize(sz);
		result.push_back(ConvertWStringToUTF8(tempPath));
		return result;
	}

	default:
		return result;
	}
}

int System_GetPropertyInt(SystemProperty prop) {
	switch (prop) {
	case SYSPROP_AUDIO_SAMPLE_RATE:
		return winAudioBackend ? winAudioBackend->GetSampleRate() : -1;
	case SYSPROP_AUDIO_FRAMES_PER_BUFFER:
		return g_Config.bAudioFramesPerBuffer;
	case SYSPROP_DEVICE_TYPE:
	{
		auto ver = Windows::System::Profile::AnalyticsInfo::VersionInfo;
		if (ver->DeviceFamily == "Windows.Mobile") {
			return DEVICE_TYPE_MOBILE;
		}
		else if (ver->DeviceFamily == "Windows.Xbox") {
			return DEVICE_TYPE_TV;
		}
		else {
			return DEVICE_TYPE_DESKTOP;
		}
	}
	case SYSPROP_DISPLAY_XRES:
	{
		CoreWindow^ corewindow = CoreWindow::GetForCurrentThread();
		if (corewindow) {
			return  (int)lround(corewindow->Bounds.Width);
		}
	}
	case SYSPROP_DISPLAY_YRES:
	{
		CoreWindow^ corewindow = CoreWindow::GetForCurrentThread();
		if (corewindow) {
			return (int)lround(corewindow->Bounds.Height);
		}
	}
	case SYSPROP_DISPLAY_COUNT:
		return g_Config.bMonitorsCount;
	default:
		return -1;
	}
}

std::vector<float> refreshrates = { 30.0f, 60.0f, 120.0f, 240.0f };
float System_GetPropertyFloat(SystemProperty prop) {
	switch (prop) {
	case SYSPROP_DISPLAY_REFRESH_RATE: {
		if (g_Config.bRenderSkip3) {
			return 30.0f;
		}
		else {
			return refreshrates[g_Config.iRefreshRate2];
		}
	}
	case SYSPROP_DISPLAY_SAFE_INSET_LEFT:
	case SYSPROP_DISPLAY_SAFE_INSET_RIGHT:
	case SYSPROP_DISPLAY_SAFE_INSET_TOP:
	case SYSPROP_DISPLAY_SAFE_INSET_BOTTOM:
		return 0.0f;
	default:
		return -1;
	}
}

void System_Toast(const char* str) {}

bool System_GetPropertyBool(SystemProperty prop) {
	switch (prop) {
	case SYSPROP_HAS_OPEN_DIRECTORY:
	{
		auto ver = Windows::System::Profile::AnalyticsInfo::VersionInfo;
		return ver->DeviceFamily != "Windows.Xbox";
	}
	case SYSPROP_HAS_FILE_BROWSER:
		return true;
	case SYSPROP_HAS_FOLDER_BROWSER:
		return true;
	case SYSPROP_HAS_IMAGE_BROWSER:
		return true;  // we just use the file browser
	case SYSPROP_HAS_BACK_BUTTON:
		return true;
	case SYSPROP_APP_GOLD:
#ifdef GOLD
		return true;
#else
		return false;
#endif
	case SYSPROP_CAN_JIT:
		return true;
	case SYSPROP_HAS_KEYBOARD:
		return false;
		//case SYSPROP_ANDROID_SCOPED_STORAGE:
		//{
		//	auto messageDialog = ref new Windows::UI::Popups::MessageDialog("No internet connection has been found.");

		//	// Add commands and set their callbacks; both buttons use the same callback function instead of inline event handlers
		//	messageDialog->Commands->Append(ref new Windows::UI::Popups::UICommand(
		//		"Ok",
		//		nullptr));
		//	Windows::UI::Popups::IUICommand^ result;
		//	ExecuteTask(result, messageDialog->ShowAsync());
		//	return true;
		//}
	default:
		return false;
	}
}

void System_Notify(SystemNotification notification) {
	switch (notification) {
	case SystemNotification::POLL_CONTROLLERS:
	{
		for (const auto& device : g_input)
		{
			if (device->UpdateState() == InputDevice::UPDATESTATE_SKIP_PAD)
				break;
		}
		break;
	}
	case SystemNotification::ROTATE_UPDATED:
	{
#if defined(_M_ARM)
		auto displayOrientation = DisplayOrientations::None;
		switch (g_Config.iScreenRotation)
		{
		case 1:
			displayOrientation = DisplayOrientations::Landscape;
			break;
		case 2:
			displayOrientation = DisplayOrientations::Portrait;
			break;
		case 3:
			displayOrientation = DisplayOrientations::LandscapeFlipped;
			break;
		case 4:
			displayOrientation = DisplayOrientations::PortraitFlipped;
			break;
		default:
			break;
		}
		DisplayInformation^ displayInformation = DisplayInformation::GetForCurrentView();
		displayInformation->AutoRotationPreferences = displayOrientation;
#endif
		updateScreen = true;
		break;
	}
	default:
		break;
	}
}

extern void setTo30State(int target);
extern int FPS30;
extern bool NotInGame;
bool System_MakeRequest(SystemRequestType type, int requestId, const std::string& param1, const std::string& param2, int param3) {
	switch (type) {

	case SystemRequestType::EXIT_APP:
	{
#if defined(_M_ARM) || defined(BUILD14393)
		Windows::ApplicationModel::Core::CoreApplication::Exit();
#else
		bool state = false;
		ExecuteTask(state, Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->TryConsolidateAsync());
		if (!state) {
			// Notify the user?
		}
#endif
		return true;
	}
	case SystemRequestType::RESTART_APP:
	{
#if defined(_M_ARM) || defined(BUILD14393)
		Windows::ApplicationModel::Core::CoreApplication::Exit();
#else
		Windows::ApplicationModel::Core::AppRestartFailureReason error;
		ExecuteTask(error, Windows::ApplicationModel::Core::CoreApplication::RequestRestartAsync(nullptr));
		if (error != Windows::ApplicationModel::Core::AppRestartFailureReason::RestartPending) {
			// Shutdown
			System_MakeRequest(SystemRequestType::EXIT_APP, requestId, param1, param2, param3);
		}
#endif
		return true;
	}
	case SystemRequestType::BROWSE_FOR_IMAGE:
	{
		std::vector<std::string> supportedExtensions = { ".jpg", ".png" };

		//Call file picker
		ChooseFile(supportedExtensions).then([requestId](std::string filePath) {
			if (filePath.size() > 1) {
				g_requestManager.PostSystemSuccess(requestId, filePath.c_str());
			}
			else {
				g_requestManager.PostSystemFailure(requestId);
			}
			});
		return true;
	}
	case SystemRequestType::BROWSE_FOR_FILE:
	{
		std::vector<std::string> supportedExtensions = {};
		switch ((BrowseFileType)param3) {
		case BrowseFileType::BOOTABLE:
			supportedExtensions = { ".cso", ".bin", ".iso", ".elf", ".pbp", ".zip" };
			break;
		case BrowseFileType::INI:
			supportedExtensions = { ".ini" };
			break;
		case BrowseFileType::DB:
			supportedExtensions = { ".db" };
			break;
		case BrowseFileType::SOUND_EFFECT:
			supportedExtensions = { ".wav" };
			break;
		case BrowseFileType::ANY:
			// 'ChooseFile' will added '*' by default when there are no extensions assigned
			break;
		default:
			ERROR_LOG(FILESYS, "Unexpected BrowseFileType: %d", param3);
			return false;
		}

		//Call file picker
		ChooseFile(supportedExtensions).then([requestId](std::string filePath) {
			if (filePath.size() > 1) {
				g_requestManager.PostSystemSuccess(requestId, filePath.c_str());
			}
			else {
				g_requestManager.PostSystemFailure(requestId);
			}
			});

		return true;
	}
	case SystemRequestType::BROWSE_FOR_FOLDER:
	{
		ChooseFolder().then([requestId](std::string folderPath) {
			if (folderPath.size() > 1) {
				g_requestManager.PostSystemSuccess(requestId, folderPath.c_str());
			}
			else {
				g_requestManager.PostSystemFailure(requestId);
			}
			});
		return true;
	}
	case SystemRequestType::NOTIFY_UI_STATE:
	{
		if (!param1.empty()) {
			if (!strcmp(param1.c_str(), "menu")) {
				mainMenuVisible = true;
				NotInGame = true;
				System_NotifyUIState("resize");
				if (g_Config.bRenderSkip3) {
					setTo30State(FPS30);
				}
				CloseLaunchItem();
#ifdef _M_ARM
				System_Notify(SystemNotification::ROTATE_UPDATED);
#endif
			}
			else if (!strcmp(param1.c_str(), "ingame")) {
#ifdef _M_ARM
				System_Notify(SystemNotification::ROTATE_UPDATED);
#endif
			}
			else if (!strcmp(param1.c_str(), "resize")) {
				Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(
					CoreDispatcherPriority::Normal,
					ref new Windows::UI::Core::DispatchedHandler([]()
						{
							NativeResized();
							System_PostUIMessage("gpu_renderResized", "");
						}));
			}
			else if (!strcmp(param1.c_str(), "skeyboard")) {
				Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(
					CoreDispatcherPriority::Normal,
					ref new Windows::UI::Core::DispatchedHandler([]()
						{
							ShowInputKeyboard();
						}));
			}
			else if (!strcmp(param1.c_str(), "hkeyboard")) {
				Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(
					CoreDispatcherPriority::Normal,
					ref new Windows::UI::Core::DispatchedHandler([]()
						{
							HideInputKeyboard();
						}));
			}
		}
		return true;
	}
	case SystemRequestType::COPY_TO_CLIPBOARD:
	{
		auto dataPackage = ref new DataPackage();
		dataPackage->RequestedOperation = DataPackageOperation::Copy;
#if defined(_M_ARM)
		dataPackage->SetText(convert(param1));
#else
		dataPackage->SetText(ToPlatformString(param1));
#endif
		Clipboard::SetContent(dataPackage);
		return true;
	}
	case SystemRequestType::TOGGLE_FULLSCREEN_STATE:
	{
		auto view = Windows::UI::ViewManagement::ApplicationView::GetForCurrentView();
		bool flag = !view->IsFullScreenMode;
		if (param1 == "0") {
			flag = false;
		}
		else if (param1 == "1") {
			flag = true;
		}
		if (flag) {
			view->TryEnterFullScreenMode();
		}
		else {
			view->ExitFullScreenMode();
		}
		return true;
	}
	default:
		return false;
	}
}

void System_ShowFileInFolder(const char* path) {
	OpenFolder(std::string(path));
}

void System_LaunchUrl(LaunchUrlType urlType, const char* url) {
	auto uri = ref new Windows::Foundation::Uri(ToPlatformString(url));

	create_task(Windows::System::Launcher::LaunchUriAsync(uri)).then([](bool b) {});
}

void System_Vibrate(int length_ms) {
#ifdef _M_ARM
	if (length_ms == -1 || length_ms == -3)
		length_ms = 50;
	else if (length_ms == -2)
		length_ms = 25;
	else
		return;

	auto timeSpan = Windows::Foundation::TimeSpan();
	timeSpan.Duration = length_ms * 10000;
	try {
#if !defined(BUILD14393)
		//Windows::Phone::Devices::Notification::VibrationDevice::GetDefault()->Vibrate(timeSpan);
#endif
	}
	catch (...) {

	}
#endif
}

void System_AskForPermission(SystemPermission permission) {
	// Do nothing
}

PermissionStatus System_GetPermissionStatus(SystemPermission permission) {
	return PERMISSION_STATUS_GRANTED;
}

std::string GetCPUBrandString() {
	Platform::String^ cpu_id = nullptr;
	Platform::String^ cpu_name = nullptr;

	// GUID_DEVICE_PROCESSOR: {97FADB10-4E33-40AE-359C-8BEF029DBDD0}
	Platform::String^ if_filter = L"System.Devices.InterfaceClassGuid:=\"{97FADB10-4E33-40AE-359C-8BEF029DBDD0}\"";

	// Enumerate all CPU DeviceInterfaces, and get DeviceInstanceID of the first one.
	auto if_task = create_task(
		DeviceInformation::FindAllAsync(if_filter)).then([&](DeviceInformationCollection^ collection) {
			if (collection->Size > 0) {
				auto cpu = collection->GetAt(0);
				auto id = cpu->Properties->Lookup(L"System.Devices.DeviceInstanceID");
				cpu_id = dynamic_cast<Platform::String^>(id);
			}
			});

	try {
		if_task.wait();
	}
	catch (const std::exception& e) {
		const char* what = e.what();
		INFO_LOG(SYSTEM, "%s", what);
	}

	if (cpu_id != nullptr) {
		// Get the Device with the same ID as the DeviceInterface
		// Then get the name (description) of that Device
		// We have to do this because the DeviceInterface we get doesn't have a proper description.
		Platform::String^ dev_filter = L"System.Devices.DeviceInstanceID:=\"" + cpu_id + L"\"";

		auto dev_task = create_task(
			DeviceInformation::FindAllAsync(dev_filter, {}, DeviceInformationKind::Device)).then(
				[&](DeviceInformationCollection^ collection) {
					if (collection->Size > 0) {
						cpu_name = collection->GetAt(0)->Name;
					}
				});

		try {
			dev_task.wait();
		}
		catch (const std::exception& e) {
			const char* what = e.what();
			INFO_LOG(SYSTEM, "%s", what);
		}
	}

	if (cpu_name != nullptr) {
		return FromPlatformString(cpu_name);
	}
	else {
		return "Unknown";
	}
}

// Emulation of TlsAlloc for Windows 10. Used by glslang. Doesn't actually seem to work, other than fixing the linking errors?

extern "C" {
	DWORD WINAPI __imp_TlsAlloc() {
		return FlsAlloc(nullptr);
	}
	BOOL WINAPI __imp_TlsFree(DWORD index) {
		return FlsFree(index);
	}
	BOOL WINAPI __imp_TlsSetValue(DWORD dwTlsIndex, LPVOID lpTlsValue) {
		return FlsSetValue(dwTlsIndex, lpTlsValue);
	}
	LPVOID WINAPI __imp_TlsGetValue(DWORD dwTlsIndex) {
		return FlsGetValue(dwTlsIndex);
	}
}
