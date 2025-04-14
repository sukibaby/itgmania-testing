#include "global.h"
#include "InputHandler_Win32_SMX.h"
#include "RageLog.h"

#include <windows.h>

/*

remove SHOWVAR before merging

*/
#include <iostream>
#define SHOWVAR(var) std::cout << "SMX: " << #var << " = " << var << " (Type: " << typeid(var).name() << ")" << std::endl;

// Typedefs for the SMX SDK functions
typedef VOID(__stdcall* SMX_Start_t)(SMXUpdateCallback UpdateCallback, void* pUser);
typedef VOID(__stdcall* SMX_GetInfo_t)(int pad, struct SMXInfo* info);
typedef uint16_t(__stdcall* SMX_GetInputState_t)(int pad);
typedef VOID(__stdcall* SMX_SetLogCallback_t)(SMXLogCallback callback);
typedef VOID(__stdcall* SMX_Stop_t)();

REGISTER_INPUT_HANDLER_CLASS2(SMX, Win32_SMX);

// Static variables
static SMX_Start_t pSMX_Start = nullptr;
static SMX_GetInfo_t pSMX_GetInfo = nullptr;
static SMX_GetInputState_t pSMX_GetInputState = nullptr;
static SMX_SetLogCallback_t pSMX_SetLogCallback = nullptr;
static SMX_Stop_t pSMX_Stop = nullptr;

static HINSTANCE hSMXdll = nullptr;

static bool __smxdll_loaded = false;
static bool __is_dll_valid = false;
static bool __detected_pad = false;
static bool __sdk_started = false;
static bool __smx_scanning_complete = false;

static CRITICAL_SECTION smxCriticalSection;

namespace {
	static void SmxCallback(int pad, SMXUpdateCallbackReason reason, void* pUser) {
		if (reason == SMXUpdateCallback_Updated) {
			__smx_scanning_complete = true;
		}
		static_cast<InputHandler_Win32_SMX*>(pUser)->ProcessInputEvent(pad);
	}

	static void LogCallback(const char* log) {
		LOG->Info("SMX SDK Log: %s", log);
	}

	class CriticalSectionGuard {
	public:
		explicit CriticalSectionGuard(CRITICAL_SECTION& cs) : m_cs(cs) {
			EnterCriticalSection(&m_cs);
		}
		~CriticalSectionGuard() {
			LeaveCriticalSection(&m_cs);
		}
	private:
		CRITICAL_SECTION& m_cs;
	};
}  // namespace

int smx_filter(unsigned int, struct _EXCEPTION_POINTERS*) {
	return EXCEPTION_EXECUTE_HANDLER;
}

bool MapFunctions() {
	__try {
		pSMX_Start = reinterpret_cast<SMX_Start_t>(GetProcAddress(hSMXdll, "SMX_Start"));
		pSMX_GetInfo = reinterpret_cast<SMX_GetInfo_t>(GetProcAddress(hSMXdll, "SMX_GetInfo"));
		pSMX_GetInputState = reinterpret_cast<SMX_GetInputState_t>(GetProcAddress(hSMXdll, "SMX_GetInputState"));
		pSMX_SetLogCallback = reinterpret_cast<SMX_SetLogCallback_t>(GetProcAddress(hSMXdll, "SMX_SetLogCallback"));
		pSMX_Stop = reinterpret_cast<SMX_Stop_t>(GetProcAddress(hSMXdll, "SMX_Stop"));
	}
	__except (smx_filter(GetExceptionCode(), GetExceptionInformation())) {
		FreeLibrary(hSMXdll);
		return false;
	}

	__smxdll_loaded = true;
	return true;
}

void InitializeSMXCriticalSection() {
	static bool WasCriticalSectionInit = false;
	if (!WasCriticalSectionInit) {
		InitializeCriticalSection(&smxCriticalSection);
		WasCriticalSectionInit = true;
	}
	SHOWVAR(WasCriticalSectionInit);
}

bool InputHandler_Win32_SMX_Is_SMX_DLL_Available() {
	static bool dll_load_attempted = false;
	SHOWVAR(dll_load_attempted);

	if (dll_load_attempted) {
		return __smxdll_loaded;
	}

	hSMXdll = LoadLibrary("SMX.dll");
	dll_load_attempted = true;

	if (!hSMXdll) {
		return false;
	}
	SHOWVAR(__smxdll_loaded);
	return (__smxdll_loaded = MapFunctions());
}

void InputHandler_Win32_SMX_Register_Pad() {
	if (!__detected_pad) {
		__detected_pad = true;
	}
}

void InputHandler_Win32_SMX::LaunchSDK() {
	if (!__sdk_started) {
		int pad_status = IsPadConnected();
		SHOWVAR(pad_status);
		switch (pad_status) {
		case SMX_SUCCESS:
		case SMX_AMBIGUOUS:
			break;
		case SMX_FAILURE:
			LOG->Info("SMX: Stage detection failed.");
			break;
		default:
			LOG->Info("SMX: Failed to detect pad or load SMX SDK.");
			break;
		}
	}
	else {
		SMX_Stop();
		SMX_Start();
		SHOWVAR(__sdk_started);
		if (__sdk_started) {
			LOG->Info("SMX: SMX SDK restarted successfully.");
		}
		else {
			LOG->Info("SMX: Failed to restart SMX SDK.");
		}
	}
}

InputHandler_Win32_SMX::InputHandler_Win32_SMX() {
	LOG->Trace("Checking if a SMX.dll exists in the Program directory...");
	std::fill(std::begin(m_padInputStates), std::end(m_padInputStates), 0);

	if (!InputHandler_Win32_SMX_Is_SMX_DLL_Available()) {
		return;
	}

	InitializeSMXCriticalSection();

	{
		CriticalSectionGuard guard(smxCriticalSection);
		SMX_SetLogCallback();
	}

	LaunchSDK();
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

	SHOWVAR(zeroIndexedButton);
	SHOWVAR(pad);
	SHOWVAR(padRemovedButton);

    static const char* buttonStrings[SMX_PANEL_COUNT] =
    {
		"UpLeft", "Up", "Up Right", "Left", "Center",
		"Right", "DownLeft", "Down", "DownRight"
    };

    const char* buttonString = (padRemovedButton >= 0 && padRemovedButton < SMX_PANEL_COUNT) ? buttonStrings[padRemovedButton] : "unknown";

    return ssprintf("SMX P%d %s", pad, buttonString);
}

void WaitForScanningToComplete() {
	constexpr int max_wait_time_ms = 5000;
	constexpr int poll_interval_ms = 100;
	int elapsed_time = 0;

	while (!__smx_scanning_complete && elapsed_time < max_wait_time_ms) {
		Sleep(poll_interval_ms);
		elapsed_time += poll_interval_ms;
		SHOWVAR(elapsed_time);
		SHOWVAR(__smx_scanning_complete);
	}

	if (!__smx_scanning_complete) {
		LOG->Warn("SMX: Scanning did not complete within the expected time.");
	}
}

int InputHandler_Win32_SMX::GetStageStatus() {
	CriticalSectionGuard guard(smxCriticalSection);

	struct SMXInfo info;
	int connected_stages = 0;

	// We need around 2 seconds to scan for pads to prevent this
	// from failing before the SDK can finish checking for pads.
	WaitForScanningToComplete();

	for (int stage = 0; stage < SMX_PAD_COUNT; ++stage) {
		SMX_GetInfo(stage, &info);

		if (info.m_bConnected) {
			++connected_stages;
		}
		SHOWVAR(stage);
		SHOWVAR(connected_stages);
	}

	if (connected_stages > 0) {
		LOG->Info("SMX: %d stage(s) are connected.", connected_stages);
		return SMX_SUCCESS;
	}

	LOG->Warn("SMX: No stages were detected.");
	return SMX_FAILURE; // could happen if the scan times out
}

int InputHandler_Win32_SMX::InitializeSMX() {
	CriticalSectionGuard guard(smxCriticalSection);
	SHOWVAR(__smxdll_loaded);
	SHOWVAR(__sdk_started);
	if (__sdk_started) {
		LOG->Info("SMX: StepManiaX SDK already started.");
		return SMX_AMBIGUOUS;
	}

	SMX_Start();
	SHOWVAR(__sdk_started);
	if (__sdk_started) {
		LOG->Info("SMX: StepManiaX SDK started.");
		return SMX_SUCCESS;
	}
	LOG->Warn("SMX: Attempted to start StepManiaX SDK, but it doesn't seem to be running.");
	return SMX_FAILURE;
}

int InputHandler_Win32_SMX::IsPadConnected() {
	CriticalSectionGuard guard(smxCriticalSection);

	SHOWVAR(__detected_pad);
	if (!__detected_pad) {
		LOG->Warn("SMX: No pad detected (__detected_pad is false).");
		return SMX_FAILURE;
	}

	bool smx_dll_available = InputHandler_Win32_SMX_Is_SMX_DLL_Available();
	SHOWVAR(smx_dll_available);
	if (!smx_dll_available) {
		LOG->Warn("SMX: SMX DLL is not available.");
		return SMX_FAILURE;
	}

	int initialization_status = InitializeSMX();
	SHOWVAR(initialization_status);
	if (initialization_status != SMX_SUCCESS) {
		LOG->Warn("SMX: Failed to initialize SMX.");
		return SMX_FAILURE;
	}

	int stage_status = GetStageStatus();
	SHOWVAR(stage_status);
	if (stage_status == SMX_SUCCESS) {
		LOG->Info("SMX: Pad is connected and ready.");
	}
	else {
		LOG->Warn("SMX: Pad is not connected.");
	}

	return stage_status;
}

void InputHandler_Win32_SMX::SMX_Start() {
	CriticalSectionGuard guard(smxCriticalSection);
	LOG->Trace("SMX: SMX_Start().");
	SHOWVAR(__sdk_started);
	if (__sdk_started) {
		LOG->Trace("SMX: SMX SDK was asked to start up, but it was already started.");
		return;
	}

    if (pSMX_Start != nullptr) {
        pSMX_Start(&SmxCallback, this);
		__sdk_started = true;
    }
}

void InputHandler_Win32_SMX::SMX_GetInfo(int pad, struct SMXInfo* info) {
	CriticalSectionGuard guard(smxCriticalSection);

	if (pSMX_GetInfo == nullptr) {
		LOG->Warn("SMX: SMX_GetInfo is null. The SMX driver will not be used.");
		info->m_bConnected = false;
		return;
	}

	pSMX_GetInfo(pad, info);
}

uint16_t InputHandler_Win32_SMX::SMX_GetInputState(int pad) {
    if (__sdk_started && pSMX_GetInputState != nullptr) {
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
	CriticalSectionGuard guard(smxCriticalSection);
	LOG->Trace("SMX: SMX_Stop().");
	SHOWVAR(__sdk_started);
	if (!__sdk_started) {
		return;
	}

	if (pSMX_Stop != nullptr) {
		pSMX_Stop();
		__sdk_started = false;
	}
}
