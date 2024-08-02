// UWP UI HELPER
// Copyright (c) 2023 Bashar Astifan.
// Email: bashar@astifan.online
// Telegram: @basharastifan
// GitHub: https://github.com/basharast/UWP2Win32

// Functions:
// SendKeyToTextEdit(vKey, flags, state) [state->KEY_DOWN or KEY_UP]
// ShowInputKeyboard()
// HideInputKeyboard()
// IsCapsLockOn()
// IsShiftOnHold()
// IsCtrlOnHold()

#include "UIHelpers.h"
#include "NKCodeFromWindowsSystem.h"
#include "StorageExtensions.h"
#include "Core/System.h"
#include "Common/System/NativeApp.h"
#include "Core/Config.h"
#include "Core/TiltEventProcessor.h"
#include "Common/Log.h"

using namespace Windows::System;
using namespace Windows::Foundation;
using namespace Windows::UI::Core;
using namespace Windows::UI::ViewManagement;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Data::Xml::Dom;
using namespace Windows::UI::Notifications;
using namespace Windows::Devices::Sensors;
using namespace Windows::Graphics::Display;

#pragma region  Input
// Input Management
UI::TextEdit* globalTextEdit = nullptr; // TextEdit to be invoked by keydown event

void SendKeyToTextEdit(int vKey, int flags, int state) {
	if (globalTextEdit != nullptr) {

		auto virtualKey = (VirtualKey)vKey;

		int keyCode;

		auto keyText = convert(virtualKey.ToString());
		if (keyText.size() > 1) {
			// Try to parse some known codes
			std::map<int, std::string> keysMap = {
				{ 188, "," },
				{ 190, "." },
				{ 191, "/" },
				{ 219, "[" },
				{ 221, "]" },
				{ 220, "\\" },
				{ 186, ";" },
				{ 222, "'" },
				{ 189, "-" },
				{ 187, "=" },
				{ 192, "`" },
				{ (int)VirtualKey::Divide, "/" },
				{ (int)VirtualKey::Multiply, "*" },
				{ (int)VirtualKey::Separator, "," },
				{ (int)VirtualKey::Add, "+" },
				{ (int)VirtualKey::Subtract, "-" },
				{ (int)VirtualKey::Decimal, "." },
				{ (int)VirtualKey::Space, " " },
			};

			if (IsShiftOnHold2()) {
				// Reinitial map with new values when shift on hold
				keysMap = {
					{ 188, "<" },
					{ 190, ">" },
					{ 191, "?" },
					{ 219, "{" },
					{ 221, "}" },
					{ 220, "|" },
					{ 186, ":" },
					{ 222, "\"" },
					{ 189, "_" },
					{ 187, "+" },
					{ 192, "~" },
					{ (int)VirtualKey::Number0, ")" },
					{ (int)VirtualKey::Number1, "!" },
					{ (int)VirtualKey::Number2, "@" },
					{ (int)VirtualKey::Number3, "#" },
					{ (int)VirtualKey::Number4, "$" },
					{ (int)VirtualKey::Number5, "%" },
					{ (int)VirtualKey::Number6, "^" },
					{ (int)VirtualKey::Number7, "&" },
					{ (int)VirtualKey::Number8, "*" },
					{ (int)VirtualKey::Number9, "(" },
					{ (int)VirtualKey::Divide, "/" },
					{ (int)VirtualKey::Multiply, "*" },
					{ (int)VirtualKey::Separator, "," },
					{ (int)VirtualKey::Add, "+" },
					{ (int)VirtualKey::Subtract, "-" },
					{ (int)VirtualKey::Decimal, "." },
					{ (int)VirtualKey::Space, " " },
				};
			}
			auto keyIter = keysMap.find(vKey);
			if (keyIter != keysMap.end()) {
				keyText = keyIter->second;
			}
		}

		replace(keyText, "NumberPad", "");
		replace(keyText, "Number", "");
		if (keyText.size() == 1) {
			flags |= KEY_CHAR;
			auto capsLockState = CoreApplication::MainView->CoreWindow->GetKeyState(VirtualKey::CapitalLock);
			if (!IsCapsLockOn2()) {
				// Transform text to lowercase
				tolower(keyText);
			}
			if (IsShiftOnHold2()) {
				if (!IsCapsLockOn2()) {
					// Transform text to uppercase
					toupper(keyText);
				}
				else {
					// Transform text to lowercase
					tolower(keyText);
				}
			}
		}

		auto iter = virtualKeyCodeToNKCode.find(virtualKey);
		if (iter != virtualKeyCodeToNKCode.end()) {
			keyCode = iter->second;

			std::list<int> nonCharList = { NKCODE_CTRL_LEFT , NKCODE_CTRL_RIGHT ,NKCODE_DPAD_LEFT ,NKCODE_DPAD_RIGHT ,NKCODE_MOVE_HOME ,NKCODE_PAGE_UP ,NKCODE_MOVE_END ,NKCODE_PAGE_DOWN ,NKCODE_FORWARD_DEL ,NKCODE_DEL ,NKCODE_ENTER ,NKCODE_NUMPAD_ENTER ,NKCODE_BACK ,NKCODE_ESCAPE };
			bool found = findInList(nonCharList, keyCode);
			if (found) {
				// Ignore, it will be handled by 'NativeKey()'
				return;
			}
		}
		else {
			keyCode = keyText[0];
		}

		auto oldView = UI::GetFocusedView();
		UI::SetFocusedView(globalTextEdit);

		KeyInput fakeKey{};
		fakeKey.deviceId = DEVICE_ID_KEYBOARD;
		fakeKey.flags = state | flags;
		fakeKey.keyCode = (InputKeyCode)keyCode;

		// Pass char as is
		//fakeKey.keyChar = (char*)keyText.c_str();
		globalTextEdit->Key(fakeKey);
		UI::SetFocusedView(oldView);
	}
}
#pragma endregion

#pragma region Input Keyboard

void ShowInputKeyboard() {
	InputPane::GetForCurrentView()->TryShow();
}

void HideInputKeyboard() {
	auto state = InputPane::GetForCurrentView()->TryHide();
	if (state) {
		globalTextEdit = nullptr;
	}
}

#pragma endregion

#pragma region Keys Status
bool IsCapsLockOn2() {
	auto capsLockState = CoreApplication::MainView->CoreWindow->GetKeyState(VirtualKey::CapitalLock);
	return (capsLockState == CoreVirtualKeyStates::Locked);
}
bool IsShiftOnHold2() {
	auto shiftState = CoreApplication::MainView->CoreWindow->GetKeyState(VirtualKey::Shift);
	return (shiftState == CoreVirtualKeyStates::Down);
}
bool IsCtrlOnHold2() {
	auto ctrlState = CoreApplication::MainView->CoreWindow->GetKeyState(VirtualKey::Control);
	return (ctrlState == CoreVirtualKeyStates::Down);
}
#pragma endregion

#pragma region Notifications
void ShowToastNotification(std::string title, std::string message) {
	ToastNotifier^ toastNotifier = ToastNotificationManager::CreateToastNotifier();
	XmlDocument^ toastXml = ToastNotificationManager::GetTemplateContent(ToastTemplateType::ToastText02);
	XmlNodeList^ toastNodeList = toastXml->GetElementsByTagName("text");
	toastNodeList->Item(0)->AppendChild(toastXml->CreateTextNode(convert(title)));
	toastNodeList->Item(1)->AppendChild(toastXml->CreateTextNode(convert(message)));
	IXmlNode^ toastNode = toastXml->SelectSingleNode("/toast");
	XmlElement^ audio = toastXml->CreateElement("audio");
	audio->SetAttribute("src", "ms-winsoundevent:Notification.SMS");
	ToastNotification^ toast = ref new ToastNotification(toastXml);
	toastNotifier->Show(toast);
}
#pragma endregion

#pragma region Sensors
Accelerometer^ _accelerometer;

bool ReadingInProgress = false;
int AccSkipper = 0;
extern DisplayOrientations currentOrientation;
void OnReadingChanged(Windows::Devices::Sensors::Accelerometer^ sender, Windows::Devices::Sensors::AccelerometerReadingChangedEventArgs^ e)
{
	if (e == nullptr || !g_Config.bSensorsMove || ReadingInProgress || g_Config.iTiltInputType == TILT_NULL || (!g_Config.bSensorsMoveX && !g_Config.bSensorsMoveY && !g_Config.bSensorsMoveZ)) {
		return;
	}
	ReadingInProgress = true;

	try {
		AccelerometerReading^ reading = e->Reading;

		double xAxisAcc = 0;
		double yAxisAcc = 0;
		double zAxisAcc = 0;
		xAxisAcc = reading->AccelerationX;
		yAxisAcc = reading->AccelerationY;
		zAxisAcc = reading->AccelerationZ;

		switch (currentOrientation)
		{
		case DisplayOrientations::Portrait:
			xAxisAcc = reading->AccelerationX;
			yAxisAcc = reading->AccelerationY;
			zAxisAcc = reading->AccelerationZ;
			break;
		case DisplayOrientations::LandscapeFlipped:
			xAxisAcc = reading->AccelerationY;
			yAxisAcc = (-1 * reading->AccelerationX);
			zAxisAcc = reading->AccelerationZ;
			break;
		case DisplayOrientations::PortraitFlipped:
			xAxisAcc = (-1 * reading->AccelerationX);
			yAxisAcc = (-1 * reading->AccelerationY);
			zAxisAcc = reading->AccelerationZ;
			break;
		case DisplayOrientations::Landscape:
			xAxisAcc = (-1 * reading->AccelerationY);
			yAxisAcc = reading->AccelerationX;
			zAxisAcc = reading->AccelerationZ;
			break;
		}

		//X
		AxisInput axis_x;
		axis_x.deviceId = DEVICE_ID_ACCELEROMETER;
		axis_x.axisId = JOYSTICK_AXIS_ACCELEROMETER_X;
		axis_x.value = g_Config.bSensorsMoveX ? (float)xAxisAcc : 0;

		//Y
		AxisInput axis_y;
		axis_y.deviceId = DEVICE_ID_ACCELEROMETER;
		axis_y.axisId = JOYSTICK_AXIS_ACCELEROMETER_Y;
		axis_y.value = g_Config.bSensorsMoveY ? (float)yAxisAcc : 0;

		//Z
		AxisInput axis_z;
		axis_z.deviceId = DEVICE_ID_ACCELEROMETER;
		axis_z.axisId = JOYSTICK_AXIS_ACCELEROMETER_Z;
		axis_z.value = g_Config.bSensorsMoveZ ? (float)zAxisAcc : 0;

		float x = 0.f, y = 0.f, z = 0.f;

		if (g_Config.bSensorsMoveX) {
			x = axis_x.value;
		}
		if (g_Config.bSensorsMoveY) {
			y = axis_y.value;
		}
		if (g_Config.bSensorsMoveZ) {
			z = axis_z.value;
		}

		switch (currentOrientation)
		{
		case DisplayOrientations::Landscape:
		case DisplayOrientations::LandscapeFlipped:
			if (g_Config.bSensorsMoveX) {
				x = axis_y.value;
			}
			if (g_Config.bSensorsMoveY) {
				y = axis_x.value;
			}
			break;
		}
		
		NativeAccelerometer(x, y, z);
		ReadingInProgress = false;
	}
	catch (...) {
		ReadingInProgress = false;
	}

}

bool AccelerometerReady = false;
void LinkAccelerometer() {
	_accelerometer = Accelerometer::GetDefault();

	if (_accelerometer != nullptr && !AccelerometerReady)
	{
		// Establish the report interval
		UINT minReportInterval = _accelerometer->MinimumReportInterval;
		UINT reportInterval = minReportInterval > 16 ? minReportInterval : 16;
		_accelerometer->ReportInterval = reportInterval;

		// Assign an event handler for the reading-changed event
		_accelerometer->ReadingChanged += ref new Windows::Foundation::TypedEventHandler<Windows::Devices::Sensors::Accelerometer^, Windows::Devices::Sensors::AccelerometerReadingChangedEventArgs^>(&OnReadingChanged);
		AccelerometerReady = true;
	}
}
bool GetAccelerometerState() {
	return AccelerometerReady;
}
#pragma endregion

