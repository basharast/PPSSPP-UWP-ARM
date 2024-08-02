#include "ppsspp_config.h"

#include "pch.h"
#include "App.h"

#include <mutex>

#include "Common/Input/InputState.h"
#include "Common/System/NativeApp.h"
#include "Common/System/System.h"
#include "Core/System.h"
#include "Core/Config.h"
#include "Core/Core.h"

#include <ppltasks.h>

#include "UWPHelpers/LaunchItem.h"
#include "UWPHelpers/StorageManager.h"
#include <Common/LogManager.h>
#include <Common/File/FileUtil.h>
#include <Common/OSVersion.h>
#include <GPU/GPUState.h>

using namespace UWP;

using namespace concurrency;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input;
using namespace Windows::System;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;
using namespace Windows::Storage;

bool m_windowClosed;
bool m_windowVisible;

// The main function is only used to initialize our IFrameworkView class.
[Platform::MTAThread]
int main(Platform::Array<Platform::String^>^) {
	auto direct3DApplicationSource = ref new Direct3DApplicationSource();
	CoreApplication::Run(direct3DApplicationSource);
	return 0;
}

IFrameworkView^ Direct3DApplicationSource::CreateView() {
	return ref new App();
}

void updateMemStickLocation() {
	Path memstickDirFile = g_Config.internalDataDirectory / "memstick_dir.txt";
	if (File::Exists(memstickDirFile)) {
		INFO_LOG(SYSTEM, "Reading '%s' to find memstick dir.", memstickDirFile.c_str());
		std::string memstickDir;
		if (File::ReadFileToString(true, memstickDirFile, memstickDir)) {
			Path memstickPath(memstickDir);
			if (!memstickPath.empty() && File::Exists(memstickPath)) {
				g_Config.memStickDirectory = memstickPath;
#if defined(_M_ARM) || defined(BUILD14393)
				SetWorkingFolder(g_Config.memStickDirectory.ToString());
#endif
				g_Config.SetSearchPath(GetSysDirectory(DIRECTORY_SYSTEM));
				g_Config.Reload({ "none","none" });
				INFO_LOG(SYSTEM, "Memstick Directory from memstick_dir.txt: '%s'", g_Config.memStickDirectory.c_str());
			}
			else {
				ERROR_LOG(SYSTEM, "Couldn't read directory '%s' specified by memstick_dir.txt.", memstickDir.c_str());
			}
		}
	}
}

#ifdef __cplusplus
extern "C" {
#endif
	bool build14393 = false;
#ifdef __cplusplus
}
#endif
bool appStarted = false;
App::App()
{
	m_windowClosed = false;
	m_windowVisible = true;
	if (!appStarted) {
#if _M_ARM || defined(BUILD14393)
		concurrency::create_task([&] {
#endif
			std::wstring internalDataFolderW = ApplicationData::Current->LocalFolder->Path->Data();
			g_Config.internalDataDirectory = Path(internalDataFolderW);
			g_Config.memStickDirectory = g_Config.internalDataDirectory;

			// On Win32 it makes more sense to initialize the system directories here
			// because the next place it was called was in the EmuThread, and it's too late by then.
			InitSysDirectories();

			LogManager::Init(&g_Config.bEnableLogging);

			// Load config up here, because those changes below would be overwritten
			// if it's not loaded here first.
			g_Config.SetSearchPath(GetSysDirectory(DIRECTORY_SYSTEM));
			g_Config.Load();

			if (g_Config.bFirstRun) {
				g_Config.memStickDirectory.clear();
			}
			else {
				updateMemStickLocation();
				InitSysDirectories();
			}

			bool debugLogLevel = false;

			g_Config.iGPUBackend = (int)GPUBackend::DIRECT3D11;

			if (debugLogLevel) {
				LogManager::GetInstance()->SetAllLogLevels(LogTypes::LDEBUG);
			}
#if _M_ARM || defined(BUILD14393)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
#endif
			// Set log file location
			if (g_Config.bEnableLogging) {
				LogManager::GetInstance()->ChangeFileLog(GetLogFile().c_str());
			}

#if defined(BUILD14393)
			g_Config.bBackwardCompatibility = true;
			build14393 = true;
#else
			if (IsLegacyWindows()) {
				g_Config.bBackwardCompatibility = true;
				build14393 = true;
			}
#endif
			appStarted = true;
#if _M_ARM || defined(BUILD14393)
			return 0;
			});
#endif
#if defined(BUILD14393)
		g_Config.bBackwardCompatibility = true;
#else
		if (IsLegacyWindows()) {
			g_Config.bBackwardCompatibility = true;
			build14393 = true;
		}
#endif

		// At this point we have access to the device.
		// We can create the device-dependent resources.
#if _M_ARM || defined(BUILD14393)
		std::this_thread::sleep_for(std::chrono::milliseconds(1500));
#endif
		m_deviceResources = std::make_shared<DX::DeviceResources>();
	}
}

// The first method called when the IFrameworkView is being created.
void App::Initialize(CoreApplicationView^ applicationView) {
	// Register event handlers for app lifecycle. This example includes Activated, so that we
	// can make the CoreWindow active and start rendering on the window.
	applicationView->Activated +=
		ref new TypedEventHandler<CoreApplicationView^, IActivatedEventArgs^>(this, &App::OnActivated);

	CoreApplication::Suspending +=
		ref new EventHandler<SuspendingEventArgs^>(this, &App::OnSuspending);

	CoreApplication::Resuming +=
		ref new EventHandler<Platform::Object^>(this, &App::OnResuming);
}

// Called when the CoreWindow object is created (or re-created).
void App::SetWindow(CoreWindow^ window) {
	window->SizeChanged +=
		ref new TypedEventHandler<CoreWindow^, WindowSizeChangedEventArgs^>(this, &App::OnWindowSizeChanged);

	window->VisibilityChanged +=
		ref new TypedEventHandler<CoreWindow^, VisibilityChangedEventArgs^>(this, &App::OnVisibilityChanged);

	window->Closed +=
		ref new TypedEventHandler<CoreWindow^, CoreWindowEventArgs^>(this, &App::OnWindowClosed);

	DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

	currentDisplayInformation->DpiChanged +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &App::OnDpiChanged);

	currentDisplayInformation->OrientationChanged +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &App::OnOrientationChanged);

	DisplayInformation::DisplayContentsInvalidated +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &App::OnDisplayContentsInvalidated);

	window->KeyDown += ref new TypedEventHandler<CoreWindow^, KeyEventArgs^>(this, &App::OnKeyDown);
	window->KeyUp += ref new TypedEventHandler<CoreWindow^, KeyEventArgs^>(this, &App::OnKeyUp);

	window->PointerMoved += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &App::OnPointerMoved);
	window->PointerEntered += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &App::OnPointerEntered);
	window->PointerExited += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &App::OnPointerExited);
	window->PointerPressed += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &App::OnPointerPressed);
	window->PointerReleased += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &App::OnPointerReleased);
	window->PointerCaptureLost += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &App::OnPointerCaptureLost);
	window->PointerWheelChanged += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &App::OnPointerWheelChanged);

	if (Windows::Foundation::Metadata::ApiInformation::IsTypePresent("Windows.Phone.UI.Input.HardwareButtons")) {
		m_hardwareButtons.insert(HardwareButton::BACK);
	}

	if (Windows::System::Profile::AnalyticsInfo::VersionInfo->DeviceFamily == "Windows.Mobile") {
		m_isPhone = true;
	}

	Windows::UI::Core::SystemNavigationManager::GetForCurrentView()->
		BackRequested += ref new Windows::Foundation::EventHandler<
		Windows::UI::Core::BackRequestedEventArgs^>(
			this, &App::App_BackRequested);

	m_deviceResources->CreateWindowSizeDependentResources();
}

bool App::HasBackButton() {
	if (m_hardwareButtons.count(HardwareButton::BACK) != 0)
		return true;
	else
		return false;
}

void App::App_BackRequested(Platform::Object^ sender, Windows::UI::Core::BackRequestedEventArgs^ e) {
	if (m_isPhone) {
		if (g_Config.bBackButtonHandle) {
			e->Handled = m_main->OnHardwareButton(HardwareButton::BACK);
		}
		else {
			e->Handled = true;
		}
	}
	else {
		e->Handled = true;
	}
}

void App::OnKeyDown(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ args) {
	m_main->OnKeyDown(args->KeyStatus.ScanCode, args->VirtualKey, args->KeyStatus.RepeatCount);
}

void App::OnKeyUp(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ args) {
	m_main->OnKeyUp(args->KeyStatus.ScanCode, args->VirtualKey);
}

void App::OnPointerMoved(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args) {
	int pointerId = touchMap_.TouchId(args->CurrentPoint->PointerId);
	if (pointerId < 0)
		return;
	float X = args->CurrentPoint->Position.X;
	float Y = args->CurrentPoint->Position.Y;
	int64_t timestamp = args->CurrentPoint->Timestamp;
	m_main->OnTouchEvent(TOUCH_MOVE, pointerId, X, Y, (double)timestamp);
}

void App::OnPointerEntered(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args) {
}

void App::OnPointerExited(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args) {
}

void App::OnPointerPressed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args) {
	int pointerId = touchMap_.TouchId(args->CurrentPoint->PointerId);
	if (pointerId < 0)
		pointerId = touchMap_.AddNewTouch(args->CurrentPoint->PointerId);

	float X = args->CurrentPoint->Position.X;
	float Y = args->CurrentPoint->Position.Y;
	int64_t timestamp = args->CurrentPoint->Timestamp;
	m_main->OnTouchEvent(TOUCH_DOWN | TOUCH_MOVE, pointerId, X, Y, (double)timestamp);
	if (!m_isPhone) {
		sender->SetPointerCapture();
	}
}

void App::OnPointerReleased(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args) {
	int pointerId = touchMap_.RemoveTouch(args->CurrentPoint->PointerId);
	if (pointerId < 0)
		return;
	float X = args->CurrentPoint->Position.X;
	float Y = args->CurrentPoint->Position.Y;
	int64_t timestamp = args->CurrentPoint->Timestamp;
	m_main->OnTouchEvent(TOUCH_UP | TOUCH_MOVE, pointerId, X, Y, (double)timestamp);
	if (!m_isPhone) {
		sender->ReleasePointerCapture();
	}
}

void App::OnPointerCaptureLost(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args) {
}

void App::OnPointerWheelChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args) {
	int pointerId = 0;  // irrelevant
	float delta = (float)args->CurrentPoint->GetCurrentPoint(args->CurrentPoint->PointerId)->Properties->MouseWheelDelta;
	m_main->OnMouseWheel(delta);
}

// Initializes scene resources, or loads a previously saved app state.
void App::Load(Platform::String^ entryPoint) {
	if (m_main == nullptr) {
		m_main = std::unique_ptr<PPSSPP_UWPMain>(new PPSSPP_UWPMain(this, m_deviceResources));
	}
}

int base = 1001;
int FPS30 = 30;
int framerate = 60;
int targetFPS = 60;
int targetSpeed = 200;
float framePerSec = 59.9400599f;
double frameMs = 1001.0 / (double)framerate;
double timePerVblank = 1.001f / (float)framerate;
double vblankMs = 0.7315;
double vsyncStartMs = 0.5925;
double vsyncEndMs = 0.7265;
int numSkippedFrames;
extern GPUStateCache gstate_c;
extern void DoFrameIdleTiming();
extern void __DisplayWaitForVblanks(const char* reason, int vblanks, bool callbacks);
extern void __DisplayGetFPS(float* out_vps, float* out_fps, float* out_actual_fps);
extern void DisplayFireActualFlip();
// This method is called after the window becomes active.
void generalCalculations() {
	
}
void setTo30State(int target = FPS30) {
	framerate = target;
	base = 1001 / 2;
	frameMs = (base * 1.0) / (double)framerate;
	framePerSec = 59.9400599f / 2;
	targetFPS = FPS30;

	timePerVblank = ((base * 0.001f) * 2) / (float)framerate;
	vblankMs = 0.7315 / 2;
	vsyncStartMs = 0.5925 / 2;
	vsyncEndMs = 0.7265 / 2;

	g_Config.iRefreshRate2 = 0;
	g_Config.iFpsLimit1 = (targetSpeed * 30) / 100;
	g_Config.iFpsLimit1State = true;
}
void setTo60State(int target = 60) {
	framerate = target;
	base = 1001;
	frameMs = (base * 1.0) / (double)framerate;
	framePerSec = 59.9400599f;
	targetFPS = 60;
	
	timePerVblank = (base * 0.001f) / (float)framerate;
	vblankMs = 0.7315;
	vsyncStartMs = 0.5925;
	vsyncEndMs = 0.7265;

	g_Config.iFpsLimit1 = 0;
	g_Config.iFpsLimit1 = (targetSpeed * 60) / 100;
	g_Config.iFpsLimit1State = false;
}

bool NotInGame = true;
void App::Run() {
	//Limiter
	const std::chrono::milliseconds frameTime(33);
	auto lastFrameTime = std::chrono::high_resolution_clock::now();
	auto now = lastFrameTime;
	std::chrono::nanoseconds deltaTime;
	bool limitWasActive = false;
	bool limitChangedBySpeed = false;
	int fpsTracker = 0;
	m_main->threadPPSSPPFrameRender();
	while (!m_windowClosed) {
		if (m_windowVisible) {
			bool skipRequired = g_Config.bRenderSkip3 || NotInGame;
			if ((skipRequired)) {
				now = std::chrono::high_resolution_clock::now();
				deltaTime = now - lastFrameTime;

				if (g_Config.bRenderSkip3) {
					if (!limitWasActive && !limitChangedBySpeed) {
						setTo30State();
						limitWasActive = true;
					}
				}
				// Call it each full rate
				/*if (fpsTracker > framerate) {
					float vps, fps, actual_fps;
					__DisplayGetFPS(&vps, &fps, &actual_fps);

					if (fps > 0) {
						float speed60Base = vps / (60 / 100.0f);
						if (speed60Base <= 50) {
							// Double the speed
						}
					}
					fpsTracker = 0;
				}
				else {
					fpsTracker++;
				}*/
			}
			else {
				if (limitWasActive) {
					setTo60State();
					limitWasActive = false;
				}
			}
			if ((!skipRequired) || deltaTime >= frameTime) {
				lastFrameTime = now;
				CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
				m_main->Render();
				if (g_Config.bDetectDeviceLose) {
					m_deviceResources->Present();
				}
			}
		}
		else {
			CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessOneAndAllPending);
		}
	}

}

// Required for IFrameworkView.
// Terminate events do not cause Uninitialize to be called. It will be called if your IFrameworkView
// class is torn down while the app is in the foreground.
void App::Uninitialize() {
}

IActivatedEventArgs^ argsEx;
bool appActivated = false;
// Application lifecycle event handlers.
void App::OnActivated(CoreApplicationView^ applicationView, IActivatedEventArgs^ args) {
	// Run() won't start until the CoreWindow is activated.
	CoreWindow::GetForCurrentThread()->Activate();
	// On mobile, we force-enter fullscreen mode.
	if (m_isPhone)
		g_Config.iForceFullScreen = 1;

	if (g_Config.UseFullScreen())
		Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->TryEnterFullScreenMode();

	DetectLaunchItem(args);
	argsEx = args;
	//Detect if app started or activated by launch item (file, uri)
}

void App::OnSuspending(Platform::Object^ sender, SuspendingEventArgs^ args) {
	// Save app state asynchronously after requesting a deferral. Holding a deferral
	// indicates that the application is busy performing suspending operations. Be
	// aware that a deferral may not be held indefinitely. After about five seconds,
	// the app will be forced to exit.
	SuspendingDeferral^ deferral = args->SuspendingOperation->GetDeferral();
	auto app = this;

	create_task([app, deferral]() {
		g_Config.Save("App::OnSuspending");
		app->m_deviceResources->Trim();
		deferral->Complete();
		});
}

void App::OnResuming(Platform::Object^ sender, Platform::Object^ args) {
	// Restore any data or state that was unloaded on suspend. By default, data
	// and state are persisted when resuming from suspend. Note that this event
	// does not occur if the app was previously terminated.

	// Insert your code here.
}

// Window event handlers.
extern bool updateScreen;
bool resizeInProgress = false;
bool forceResize = false;
void App::OnWindowSizeChanged(CoreWindow^ sender, WindowSizeChangedEventArgs^ args) {
	if (resizeInProgress) {
		return;
	}
	forceResize = true;
	resizeInProgress = true;

	auto view = Windows::UI::ViewManagement::ApplicationView::GetForCurrentView();
	g_Config.bFullScreen = view->IsFullScreenMode;
	g_Config.iForceFullScreen = -1;

	float width = sender->Bounds.Width;
	float height = sender->Bounds.Height;
	float scale = m_deviceResources->GetDpi() / 96.0f;

	m_deviceResources->SetLogicalSize(Size(width, height));
	if (m_main) {
		m_main->CreateWindowSizeDependentResources();
	}

	PSP_CoreParameter().pixelWidth = (int)(width * scale);
	PSP_CoreParameter().pixelHeight = (int)(height * scale);

	updateScreen = true;
	if (UpdateScreenScale((int)width, (int)height)) {
		System_PostUIMessage("gpu_displayResized", "");
	}

	concurrency::create_task([&] {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		resizeInProgress = false;
		});
}

void App::OnVisibilityChanged(CoreWindow^ sender, VisibilityChangedEventArgs^ args) {
	m_windowVisible = args->Visible;
	updateScreen = true;
}

void App::OnWindowClosed(CoreWindow^ sender, CoreWindowEventArgs^ args) {
	m_windowClosed = true;
}

// DisplayInformation event handlers. 

void App::OnDpiChanged(DisplayInformation^ sender, Object^ args) {
	// Note: The value for LogicalDpi retrieved here may not match the effective DPI of the app
	// if it is being scaled for high resolution devices. Once the DPI is set on DeviceResources,
	// you should always retrieve it using the GetDpi method.
	// See DeviceResources.cpp for more details.
	m_deviceResources->SetDpi(sender->LogicalDpi);
	m_main->CreateWindowSizeDependentResources();
	updateScreen = true;
	//CoreWindow^ corewindow = CoreWindow::GetForCurrentThread();
	//App::OnWindowSizeChanged(corewindow, nullptr);
}

DisplayOrientations currentOrientation;
void App::OnOrientationChanged(DisplayInformation^ sender, Object^ args) {
	m_deviceResources->SetCurrentOrientation(sender->CurrentOrientation);
	m_main->CreateWindowSizeDependentResources();
	currentOrientation = sender->CurrentOrientation;
	updateScreen = true;
}

void App::OnDisplayContentsInvalidated(DisplayInformation^ sender, Object^ args) {
	m_deviceResources->Present();
	updateScreen = true;
}
