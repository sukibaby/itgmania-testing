#include "global.h"
#include "InputHandler_Win32_SMX.h"
#include "RageLog.h"

#include <windows.h>

// Setup
typedef VOID(__stdcall* SMX_Start_t)(SMXUpdateCallback UpdateCallback, void* pUser);
typedef VOID(__stdcall* SMX_GetInfo_t)(int pad, struct SMXInfo* info);
typedef uint16_t(__stdcall* SMX_GetInputState_t)(int pad);
typedef VOID(__stdcall* SMX_SetLogCallback_t)(SMXLogCallback callback);
typedef VOID(__stdcall* SMX_Stop_t)();

REGISTER_INPUT_HANDLER_CLASS2(SMX, Win32_SMX);

// Setup
static SMX_Start_t pSMX_Start = nullptr;
static SMX_GetInfo_t pSMX_GetInfo = nullptr;
static SMX_GetInputState_t pSMX_GetInputState = nullptr;
static SMX_SetLogCallback_t pSMX_SetLogCallback = nullptr;
static SMX_Stop_t pSMX_Stop = nullptr;

static HINSTANCE hSMXdll = nullptr;

static bool _smxdll_loaded = false;
static bool __detected_pad = false;
static bool Is_SMX_Started = false;
static bool smx_scanning_complete = false;

static CRITICAL_SECTION smxCriticalSection;

namespace {
	static void SmxCallback(int pad, SMXUpdateCallbackReason reason, void* pUser) {
		if (reason == SMXUpdateCallback_Updated) {
			smx_scanning_complete = true;
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

void InitializeSMXCriticalSection() {
	static bool smx_cs_init = false;
	if (!smx_cs_init) {
		InitializeCriticalSection(&smxCriticalSection);
		smx_cs_init = true;
	}
}

void WaitForScanningToComplete() {
	constexpr int max_wait_time_ms = 5000;
	constexpr int poll_interval_ms = 100;
	int elapsed_time = 0;

	while (!smx_scanning_complete && elapsed_time < max_wait_time_ms) {
		Sleep(poll_interval_ms);
		elapsed_time += poll_interval_ms;
	}

	if (!smx_scanning_complete) {
		LOG->Trace("SMX: Stage detection failed.");
	}
}

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
		LOG->Warn("SMX.dll could not be loaded successfully.");
		FreeLibrary(hSMXdll);
		return false;
	}

	_smxdll_loaded = true;
	return true;
}

bool InputHandler_Win32_SMX_Is_SMX_DLL_Available() {
	static bool dll_load_attempted = false;
	if (dll_load_attempted) {
		return _smxdll_loaded;
	}

	hSMXdll = LoadLibrary("SMX.dll");
	dll_load_attempted = true;

	if (hSMXdll) {
		LOG->Trace("SMX: SMX.dll successfully loaded.");
		return false;
	}
	else {
		LOG->Trace("SMX: SMX.dll not found.");
	}

	return (_smxdll_loaded = MapFunctions());
}

void InputHandler_Win32_SMX_Register_Pad() {
	if (!__detected_pad) {
		__detected_pad = true;
	}
}

bool InputHandler_Win32_SMX::IsSMXReady() {
	return __detected_pad && InputHandler_Win32_SMX_Is_SMX_DLL_Available();
}

void InputHandler_Win32_SMX::SMX_Restart() {
	LOG->Info("SMX: Restarting SMX SDK...");
	SMX_Stop();
	SMX_Start();

	if (Is_SMX_Started) {
		LOG->Info("SMX: SMX SDK restarted successfully.");
	}
	else {
		LOG->Warn("SMX: Failed to restart SMX SDK.");
	}
}

void InputHandler_Win32_SMX::SMX_Start() {
	CriticalSectionGuard guard(smxCriticalSection);

	if (Is_SMX_Started) {
		return;
	}

	if (pSMX_Start != nullptr) {
		pSMX_Start(&SmxCallback, this);
		Is_SMX_Started = true;
	}
}

void InputHandler_Win32_SMX::SMX_GetInfo(int pad, struct SMXInfo* info) {
	CriticalSectionGuard guard(smxCriticalSection);

	if (pSMX_GetInfo == nullptr) {
		info->m_bConnected = false;
		return;
	}

	pSMX_GetInfo(pad, info);
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
	CriticalSectionGuard guard(smxCriticalSection);

	if (!Is_SMX_Started) {
		return;
	}

	if (pSMX_Stop != nullptr) {
		pSMX_Stop();
		Is_SMX_Started = false;
	}
}

InputHandler_Win32_SMX::InputHandler_Win32_SMX() {
	// Initialize the pad states
	std::fill(std::begin(m_padInputStates), std::end(m_padInputStates), 0);

	// Check if the DLL is available
	if (!InputHandler_Win32_SMX_Is_SMX_DLL_Available()) {
		return;
	}

	// Initialize the SMX SDK
	if (InitializeSMX() != SMX_SUCCESS) {
		LOG->Warn("SMX: StepManiaX SDK initialization failed.");
		return;
	}

	// Check for connected pads
	if (IsPadConnected() != SMX_SUCCESS) {
		LOG->Warn("SMX: No pads connected.");
		return;
	}

	LOG->Info("SMX: SMX SDK initialized successfully.");
}


InputHandler_Win32_SMX::~InputHandler_Win32_SMX() {
	SMX_Stop();
}

int InputHandler_Win32_SMX::GetStageStatus() {
	CriticalSectionGuard guard(smxCriticalSection);

	int connected_stages = 0;
	struct SMXInfo info;

	// We need 1-2 seconds after requesting info to scan for pads,
	// to prevent this function from continuing before the SDK finishes
	// scanning for valid stages.
	WaitForScanningToComplete();

	for (int stage = 0; stage < SMX_PAD_COUNT; ++stage) {
		SMX_GetInfo(stage, &info);
		LOG->Trace("SMX: Stage %d: %s", stage, info.m_bConnected ? "Connected" : "Disconnected");
		if (info.m_bConnected) {
			++connected_stages;
		}
	}

	return (connected_stages > 0) ? SMX_SUCCESS : SMX_FAILURE;
}

int InputHandler_Win32_SMX::InitializeSMX() {
	// Check if the DLL is available
	if (!CheckDLLAvailability()) {
		LOG->Warn("SMX: DLL not available.");
		return SMX_FAILURE;
	}

	// Initialize the critical section (for mutexing/atomicity)
	InitializeSMXCriticalSection();

	// Set up logging
	{
		CriticalSectionGuard guard(smxCriticalSection);
		SMX_SetLogCallback();
	}

	// Start the SMX SDK
	SMX_Start();

	// Wait for scanning to complete
	WaitForScanningToComplete();

	// Validate if the SMX SDK started successfully
	if (!Is_SMX_Started) {
		LOG->Warn("SMX: Failed to start SMX SDK.");
		return SMX_FAILURE;
	}

	LOG->Info("SMX: SMX SDK initialized successfully.");
	return SMX_SUCCESS;
}

int InputHandler_Win32_SMX::IsPadConnected() {
	int stage_status = GetStageStatus();
	if (stage_status == SMX_FAILURE) {
		LOG->Warn("SMX: Stage status check failed.");
		return SMX_FAILURE;
	}

	if (stage_status != SMX_SUCCESS && stage_status != SMX_AMBIGUOUS) {
		LOG->Warn("SMX: Unknown stage status: %d", stage_status);
		return SMX_FAILURE;
	}

	return stage_status;
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

RString InputHandler_Win32_SMX::GetDeviceSpecificInputString(const DeviceInput& di)
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
