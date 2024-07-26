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

#include "Common/UI/View.h"
#include "Common/UI/Root.h"

// Input Handler
extern UI::TextEdit* globalTextEdit; // TextEdit to be invoked by keydown event
void SendKeyToTextEdit(int vKey, int flags, int state);
void ShowInputKeyboard();
void HideInputKeyboard();


// Keys Status
bool IsCapsLockOn2();
bool IsShiftOnHold2();
bool IsCtrlOnHold2();
bool GetAccelerometerState();
void LinkAccelerometer();

// Notifications
void ShowToastNotification(std::string title, std::string message);
