#include "global.h"
#include "InputHandler_Win32_SMX.h"
#include "RageLog.h"

#include <windows.h>

// Typedefs for the SMX SDK functions
typedef VOID(__stdcall* SMX_Start_t)(
    SMXUpdateCallback UpdateCallback, void *pUser
);

typedef VOID(__stdcall* SMX_GetInfo_t)(
	int pad, struct SMXInfo *info
);

typedef uint16_t(__stdcall* SMX_GetInputState_t)(
    int pad
);

typedef VOID(__stdcall* SMX_SetLogCallback_t)(
	SMXLogCallback callback
);

typedef VOID(__stdcall* SMX_Stop_t)();

REGISTER_INPUT_HANDLER_CLASS2(SMX, Win32_SMX);

// Constants
constexpr int SMX_PANEL_COUNT = 9;
constexpr int SMX_SUCCESS = 1;
constexpr int SMX_AMBIGUOUS = 0;
constexpr int SMX_FAILURE = -1;

// Static variables
static SMX_Start_t pSMX_Start = nullptr;
static SMX_GetInfo_t pSMX_GetInfo = nullptr;
static SMX_GetInputState_t pSMX_GetInputState = nullptr;
static SMX_SetLogCallback_t pSMX_SetLogCallback = nullptr;
static SMX_Stop_t pSMX_Stop = nullptr;

static HINSTANCE hSMXdll = nullptr;

static bool _smxdll_loaded = false;
static bool __detected_pad = false;
static bool Is_SMX_Started = false;

namespace {
	static void SmxCallback(int pad, SMXUpdateCallbackReason reason, void* pUser) {
		InputHandler_Win32_SMX* inputHandler = static_cast<InputHandler_Win32_SMX*>(pUser);
		inputHandler->ProcessInputEvent(pad);
	};

	static void LogCallback(const char *log) {
		LOG->Info("SMX SDK Log: %s", log);
	};
}

int smx_filter(unsigned int, struct _EXCEPTION_POINTERS*)
{
	return EXCEPTION_EXECUTE_HANDLER;
}

bool MapFunctions()
{
	__try
	{
		pSMX_Start = (SMX_Start_t)GetProcAddress(hSMXdll, "SMX_Start");
		pSMX_GetInfo = (SMX_GetInfo_t)GetProcAddress(hSMXdll, "SMX_GetInfo");
		pSMX_GetInputState = (SMX_GetInputState_t)GetProcAddress(hSMXdll, "SMX_GetInputState");
		pSMX_SetLogCallback = (SMX_SetLogCallback_t)GetProcAddress(hSMXdll, "SMX_SetLogCallback");
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
	static bool dll_load_attempted = false;
	if (dll_load_attempted) {
		LOG->Trace("SMX: SMX DLL already loaded, skipping load attempt.");
		return _smxdll_loaded;
	}

	hSMXdll = LoadLibrary("SMX.dll");
	dll_load_attempted = true;

	if (hSMXdll == nullptr) {
		LOG->Warn("SMX.dll not found. The SMX driver will not be used.");
		_smxdll_loaded = false;
		return false;
	}
	else {
		LOG->Trace("SMX.dll loaded successfully! :D");
	}
	_smxdll_loaded = MapFunctions();
	return _smxdll_loaded;
}

void InputHandler_Win32_SMX_Register_Pad() {
	if (__detected_pad) {
		return;
	}
	__detected_pad = true;
}

InputHandler_Win32_SMX::InputHandler_Win32_SMX() {
	LOG->Trace("SMX: InputHandler_Win32_SMX constructor called. Attempting to load SMX.dll...");
	std::fill(std::begin(m_padInputStates), std::end(m_padInputStates), 0);
	SMX_SetLogCallback();

	int connection_status = IsPadConnected();
	switch (connection_status) {
	case SMX_SUCCESS:
		LOG->Info("SMX: Pad is connected and ready.");
		break;
	case SMX_AMBIGUOUS:
		LOG->Warn("SMX: SMX started, but info.m_bConnected returned 0. Are pads connected?");
		break;
	case SMX_FAILURE:
		LOG->Warn("SMX: Failed to detect pad or load SMX DLL.");
		if (Is_SMX_Started) {
			SMX_Stop();
			LOG->Info("SMX: Stopping SMX SDK.");
		}
		break;
	default:
		LOG->Warn("SMX: Unknown connection status: %d", connection_status);
		break;
	}
}

InputHandler_Win32_SMX::~InputHandler_Win32_SMX() {
	SMX_Stop();
}

void InputHandler_Win32_SMX::GetDevicesAndDescriptions(std::vector<InputDeviceInfo>& vDevicesOut)
{
	vDevicesOut.push_back(InputDeviceInfo(InputDevice(DEVICE_SMX), "SMX"));
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

    static const char* buttonStrings[SMX_PANEL_COUNT] =
    {
		"UpLeft", "Up", "Up Right", "Left", "Center",
		"Right", "DownLeft", "Down", "DownRight"
    };

    const char* buttonString = (padRemovedButton >= 0 && padRemovedButton < SMX_PANEL_COUNT) ? buttonStrings[padRemovedButton] : "unknown";

    return ssprintf("SMX P%d %s", pad, buttonString);
}

int InputHandler_Win32_SMX::GetStageStatus() {
	// Not sure if this works. Seems like we are failing to return true
	// here when we have a valid pad connected that the game will map.
	// As a result this function is returning false despite the pad being
	// connected. The game will be able to poll the pad anyway since
	// SMX_Start() was called. So we might not be populating m_bConnected properly.
	// Check stage 0
	struct SMXInfo info;
	int connected_stages = 0;

	// Check stages 0 and 1
	for (int stage = 0; stage <= 1; ++stage) {
		SMX_GetInfo(stage, &info);
		LOG->Trace("SMX: Stage %d connection status: %d", stage, info.m_bConnected);

		if (info.m_bConnected) {
			LOG->Trace("SMX: Stage %d is connected.", stage);
			++connected_stages;
		}
	}

	if (connected_stages > 0) {
		LOG->Info("SMX: %d stage(s) are connected.", connected_stages);
		return SMX_SUCCESS;
	}

	LOG->Warn("SMX: No stages were detected.");
	return SMX_AMBIGUOUS; // doesn't necessarily mean the pad is not connected. everything might be ok but we failed to get m_bConnected
}

int InputHandler_Win32_SMX::InitializeSMX() {
	if (Is_SMX_Started) {
		LOG->Info("SMX: StepManiaX SDK already started.");
		return SMX_AMBIGUOUS;
	}

	SMX_Start();
	if (Is_SMX_Started) {
		LOG->Info("SMX: StepManiaX SDK started.");
		return SMX_SUCCESS;
	}
	LOG->Warn("SMX: Attempted to start StepManiaX SDK, but it doesn't seem to be running.");
	return SMX_FAILURE;
}

int InputHandler_Win32_SMX::IsPadConnected() {
	if (!__detected_pad) {
		LOG->Warn("SMX: No pad detected (__detected_pad is false).");
		return SMX_FAILURE;
	}

	if (!InputHandler_Win32_SMX_Is_SMX_DLL_Available()) {
		LOG->Warn("SMX: SMX DLL is not available.");
		return SMX_FAILURE;
	}

	if (InitializeSMX() == SMX_SUCCESS) {
		int stage_status = GetStageStatus();
		if (stage_status == SMX_SUCCESS) {
			LOG->Info("SMX: Pad is connected and ready.");
		}
		return stage_status;
	}

	return SMX_FAILURE;
}

void InputHandler_Win32_SMX::SMX_Start() {
	if (Is_SMX_Started) {
		LOG->Trace("SMX: SMX SDK was asked to start up, but it was already started.");
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
	
	return SMX_AMBIGUOUS;
}

void InputHandler_Win32_SMX::SMX_SetLogCallback() {
	if (pSMX_SetLogCallback != nullptr) {
		pSMX_SetLogCallback(&LogCallback);
	}
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
