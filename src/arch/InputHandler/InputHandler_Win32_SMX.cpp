#include "global.h"
#include "InputHandler_Win32_SMX.h"
#define WIN32_LEAN_AND_MEAN
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

static HINSTANCE hSMXdll = nullptr;

REGISTER_INPUT_HANDLER_CLASS2(SMX, Win32_SMX);

namespace {
	static void SmxCallback(int pad, SMXUpdateCallbackReason reason, void* pUser) {
		InputHandler_Win32_SMX* stage_info = static_cast<InputHandler_Win32_SMX*>(pUser);
		stage_info->ProcessPoll(pad);
	};
}

int smx_filter(unsigned int, struct _EXCEPTION_POINTERS*)
{
	return EXCEPTION_EXECUTE_HANDLER;
}

bool MapFunctions()
{
	if (_smxdll_loaded) {
		return true;
	}

	__try
	{
		pSMX_Start = (SMX_Start_t)GetProcAddress(hSMXdll, "SMX_Start");
		pSMX_GetInputState = (SMX_GetInputState_t)GetProcAddress(hSMXdll, "SMX_GetInputState");
	}
	__except (smx_filter(GetExceptionCode(), GetExceptionInformation()))
	{
		MessageBox(nullptr, "Could not map functions in SMX.dll", "ERROR", MB_OK);
		FreeLibrary(hSMXdll);
		return false;
	}

	_smxdll_loaded = true;
	return true;
}

bool Attempt_SMX_DLL_Load()
{
	if (_smxdll_loaded) {
		return true;
	}

	hSMXdll = LoadLibrary("SMX.dll");

	if (hSMXdll == nullptr)
	{
		MessageBox(nullptr, "Could not load SMX.dll. Ensure it is in the Program directory, the proper C++ Visual C++ Redistributable Runtimes dependencies are installed, you have matched your architectures (x86/x64), and any additional dll dependencies of SMX.dll are available.", "ERROR", MB_OK);
		return false;
	}

	return MapFunctions();
}

void InputHandler_Win32_SMX::GetDevicesAndDescriptions( std::vector<InputDeviceInfo>& vDevicesOut )
{
	vDevicesOut.push_back(InputDeviceInfo(InputDevice(DEVICE_SMX), "SMX"));
}

InputHandler_Win32_SMX::InputHandler_Win32_SMX() {
	std::fill(std::begin(m_padInputStates), std::end(m_padInputStates), 0);

    if (Attempt_SMX_DLL_Load()) {
        SMX_Start(&SmxCallback, this);
    }
}

void InputHandler_Win32_SMX::ProcessPoll(int pad) {
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
