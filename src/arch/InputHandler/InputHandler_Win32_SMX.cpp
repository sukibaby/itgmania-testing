#include "global.h"
#include "InputHandler_Win32_SMX.h"
#include "windows.h"

static const int SMX_PANEL_COUNT = 9;

typedef VOID(__stdcall* SMX_Start_t)(
    SMXUpdateCallback UpdateCallback, void *pUser
);
static SMX_Start_t pSMX_Start = nullptr;

typedef uint16_t(__stdcall* SMX_GetInputState_t)(
    int pad
);
static SMX_GetInputState_t pSMX_GetInputState = nullptr;

typedef VOID(__stdcall* SMX_Stop_t)();
static SMX_Stop_t pSMX_Stop = nullptr;

static HINSTANCE hSMXdll = nullptr;

REGISTER_INPUT_HANDLER_CLASS2(SMX, Win32_SMX);

namespace {
	static void SmxCallback(int pad, SMXUpdateCallbackReason reason, void* pUser) {
		InputHandler_Win32_SMX* inputHandler = static_cast<InputHandler_Win32_SMX*>(pUser);
		inputHandler->ProcessInputEvent(pad);
	};
}

int smx_filter(unsigned int, struct _EXCEPTION_POINTERS*)
{
	return EXCEPTION_EXECUTE_HANDLER;
}

static bool _smxdll_loaded = false;
static bool _smxdll_attempted_load = false;

bool MapFunctions()
{
	__try
	{
		pSMX_Start = (SMX_Start_t)GetProcAddress(hSMXdll, "SMX_Start");
		pSMX_GetInputState = (SMX_GetInputState_t)GetProcAddress(hSMXdll, "SMX_GetInputState");
		pSMX_Stop = (SMX_Stop_t)GetProcAddress(hSMXdll, "SMX_Stop");
	}
	__except (smx_filter(GetExceptionCode(), GetExceptionInformation()))
	{
		FreeLibrary(hSMXdll);
		return false;
	}

	_smxdll_loaded = true;
	return true;
}

bool InputHandler_Win32_SMX_Is_SMX_DLL_Available()
{
	if (_smxdll_attempted_load) {
		return _smxdll_loaded;
	}
	_smxdll_attempted_load = true;

	hSMXdll = LoadLibrary("SMX.dll");

	if (hSMXdll == nullptr)
	{
		return false;
	}

	return MapFunctions();
}

InputHandler_Win32_SMX::InputHandler_Win32_SMX() {
	std::fill(std::begin(m_padInputStates), std::end(m_padInputStates), 0);

    if (InputHandler_Win32_SMX_Is_SMX_DLL_Available()) {
        SMX_Start(&SmxCallback, this);
    }
}

InputHandler_Win32_SMX::~InputHandler_Win32_SMX() {
	SMX_Stop();
}

void InputHandler_Win32_SMX::GetDevicesAndDescriptions(std::vector<InputDeviceInfo>& vDevicesOut)
{
	if (InputHandler_Win32_SMX_Is_SMX_DLL_Available()) {
		vDevicesOut.push_back(InputDeviceInfo(InputDevice(DEVICE_SMX), "SMX"));
	}
}

void InputHandler_Win32_SMX::ProcessInputEvent(int pad) {
    if (pad >= SMX_PAD_COUNT) {
        return;
    }

    uint16_t inputState = SMX_GetInputState(pad);
    uint16_t changedInputs = inputState ^ m_padInputStates[pad];

    if (changedInputs == 0) {
        InputHandler::UpdateTimer();
        return;
    }

    DeviceButton startButton = enum_add2(JOY_BUTTON_1, pad * SMX_PANEL_COUNT);

    for (int i = 0; i < SMX_PANEL_COUNT; i++) {
        if ((changedInputs & (1 << i)) != 0) {
            bool pressed = inputState & (1 << i);
            DeviceInput di(DEVICE_SMX, enum_add2(startButton, i), pressed);
			di.ts.Touch(); // SMX surfaces an input immediately when it occurs, so don't adjust for error
            ButtonPressed(di);
        }
    }

    m_padInputStates[pad] = inputState;
    InputHandler::UpdateTimer();
}

RString InputHandler_Win32_SMX::GetDeviceSpecificInputString(const DeviceInput &di)
{
    int zeroIndexedButton = di.button - JOY_BUTTON_1;
    int pad = (zeroIndexedButton / SMX_PANEL_COUNT) + 1;
    int padRemovedButton = zeroIndexedButton % SMX_PANEL_COUNT;

    RString buttonString;
    switch (padRemovedButton) {
        case 0: buttonString = "up left"; break;
        case 1: buttonString = "up"; break;
        case 2: buttonString = "up right"; break;
        case 3: buttonString = "left"; break;
        case 4: buttonString = "center"; break;
        case 5: buttonString = "right"; break;
        case 6: buttonString = "down left"; break;
        case 7: buttonString = "down"; break;
        case 8: buttonString = "down right"; break;
        default: buttonString = "unknown"; break;
    }

    return ssprintf("SMX P%d, %s", pad, buttonString.c_str());
}

void InputHandler_Win32_SMX::SMX_Start(SMXUpdateCallback UpdateCallback, void *pUser) {
    if (pSMX_Start != nullptr) {
        pSMX_Start(UpdateCallback, pUser);
    }
}

uint16_t InputHandler_Win32_SMX::SMX_GetInputState(int pad) {
    if (pSMX_GetInputState != nullptr) {
        return pSMX_GetInputState(pad);
    }
	
	return 0;
}

void InputHandler_Win32_SMX::SMX_Stop() {
	if (pSMX_Stop != nullptr) {
		pSMX_Stop();
	}
}

/*
 * (c) 2025 Caleb Lee
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
