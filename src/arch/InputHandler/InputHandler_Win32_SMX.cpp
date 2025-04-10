#include "global.h"
#include "InputHandler_Win32_SMX.h"
#include "RageLog.h"
#include <windows.h>

static const int SMX_PANEL_COUNT = 9;

typedef VOID(__stdcall* SMX_Start_t)(
    SMXUpdateCallback UpdateCallback, void *pUser
);
static SMX_Start_t pSMX_Start = nullptr;

typedef VOID(__stdcall* SMX_GetInfo_t)(
	int pad, struct SMXInfo *info
);
static SMX_GetInfo_t pSMX_GetInfo = nullptr;

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
		pSMX_GetInfo = (SMX_GetInfo_t)GetProcAddress(hSMXdll, "SMX_GetInfo");
		pSMX_GetInputState = (SMX_GetInputState_t)GetProcAddress(hSMXdll, "SMX_GetInputState");
		pSMX_Stop = (SMX_Stop_t)GetProcAddress(hSMXdll, "SMX_Stop");
	}
	__except (smx_filter(GetExceptionCode(), GetExceptionInformation()))
	{
		LOG->Warn("SMX.dll is not valid. The SMX driver will not be used.");
		FreeLibrary(hSMXdll);
		return false;
	}

	_smxdll_loaded = true;
	return true;
}

// The only way to know for-sure if the DLL is available is to actually load it, so this method attempts to load the DLL the first time it is run.
bool InputHandler_Win32_SMX_Is_SMX_DLL_Available()
{
	if (_smxdll_attempted_load) {
		return _smxdll_loaded;
	}
	_smxdll_attempted_load = true;

	hSMXdll = LoadLibrary("SMX.dll");

	if (hSMXdll == nullptr) {
		LOG->Warn("SMX.dll not found. The SMX driver will not be used.");
		_smxdll_loaded = false;
		return false;
	}
	_smxdll_loaded = MapFunctions();
	return _smxdll_loaded;
}

static bool __detected_pad = false;
static InputHandler_Win32_SMX *__current_instance = nullptr;
void InputHandler_Win32_SMX_Register_Pad() {
	if (__detected_pad) {
		return;
	}
	__detected_pad = true;

	// Start the SMX SDK if a pad is being registered for the first time after the driver instance had already been initialized.
	if (__current_instance != nullptr) {
		__current_instance->SMX_Start();
	}
}

InputHandler_Win32_SMX::InputHandler_Win32_SMX() {
	std::fill(std::begin(m_padInputStates), std::end(m_padInputStates), 0);

	// Only start the SMX SDK if a pad has been detected already.
	// The SDK can also be started on `__current_instance` if a pad gets registered after this point.
    if (__detected_pad) {
        SMX_Start();
		return; // No need to hold `__current_instance` if SMX_Start has been called.
    }

	// Hold onto this instance so static methods can call `SMX_Start` if need be
	if (__current_instance == nullptr) {
		__current_instance = this;
	}
}

InputHandler_Win32_SMX::~InputHandler_Win32_SMX() {
	SMX_Stop();
	__current_instance = nullptr;
}

void InputHandler_Win32_SMX::GetDevicesAndDescriptions(std::vector<InputDeviceInfo>& vDevicesOut)
{
	// Only register this as a device if SMX.dll is available, and if a pad is connected.
	if (InputHandler_Win32_SMX_Is_SMX_DLL_Available() && IsPadConnected()) {
		vDevicesOut.push_back(InputDeviceInfo(InputDevice(DEVICE_SMX), "SMX"));
	}
}

void InputHandler_Win32_SMX::ProcessInputEvent(int pad) {
	/*
	The SMX SDK always sends callback events over the same thread, so thread safety is not a worry.
	*/

	// Fast-fail if somehow the pad is outside of range
    if (pad < 0 || pad >= SMX_PAD_COUNT) {
        return;
    }

	// Get new input state and compare with the old
    uint16_t newInputState = SMX_GetInputState(pad);
    uint16_t changedInputs = newInputState ^ m_padInputStates[pad];

	// If inputs weren't updated, report the end of an empty poll and return
    if (changedInputs == 0) {
        InputHandler::UpdateTimer();
        return;
    }

	// SMX events come in for multiple pads, but get translated into one device from SM's POV.
	// Ensure P2's buttons begin from the correct `JOY_BUTTON`.
    DeviceButton beginButton = enum_add2(JOY_BUTTON_1, pad * SMX_PANEL_COUNT);

	// Process inputs
    for (int i = 0; i < SMX_PANEL_COUNT; i++) {
		bool didButtonStateChange = (changedInputs & (1 << i)) != 0;
        if (didButtonStateChange) {
            bool pressed = newInputState & (1 << i); // Get pressed state
            DeviceInput di(DEVICE_SMX, enum_add2(beginButton, i), pressed); // Make input event with pressed state for this button
			di.ts.Touch(); // Touch the timestamp timer to indicate the input happened *now*
            ButtonPressed(di); // Report the input event
        }
    }

	// Save the new input state for later, and report the end of a poll.
    m_padInputStates[pad] = newInputState;
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

bool InputHandler_Win32_SMX::IsPadConnected() {
	if (!__detected_pad) {
		return false;
	}

	if (!InputHandler_Win32_SMX_Is_SMX_DLL_Available()) {
		return false;
	}

	for (int i = 0; i < SMX_PAD_COUNT; i++) {
		struct SMXInfo info;
		SMX_GetInfo(i, &info);

		if (info.m_bConnected) {
			return true;
		}
	}

	return false;
}

static bool Is_SMX_Started = false;
void InputHandler_Win32_SMX::SMX_Start() {
	if (Is_SMX_Started) {
		return;
	}

    if (pSMX_Start != nullptr) {
        pSMX_Start(&SmxCallback, this);
		Is_SMX_Started = true;
    }
}

void InputHandler_Win32_SMX::SMX_GetInfo(int pad, struct SMXInfo *info) {
	if (Is_SMX_Started && pSMX_GetInfo != nullptr) {
		pSMX_GetInfo(pad, info);
	}
	else {
		info->m_bConnected = false;
	}
}

uint16_t InputHandler_Win32_SMX::SMX_GetInputState(int pad) {
    if (Is_SMX_Started && pSMX_GetInputState != nullptr) {
        return pSMX_GetInputState(pad);
    }
	
	return 0;
}

void InputHandler_Win32_SMX::SMX_Stop() {
	if (!Is_SMX_Started) {
		return;
	}

	if (pSMX_Stop != nullptr) {
		pSMX_Stop();
		Is_SMX_Started = false;
	}
}
