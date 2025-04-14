#ifndef INPUT_HANDLER_WIN32_SMX_H
#define INPUT_HANDLER_WIN32_SMX_H

#include "InputHandler.h"

// Constants
constexpr int SMX_FAILURE = -1;
constexpr int SMX_AMBIGUOUS = 0;
constexpr int SMX_SUCCESS = 1;
constexpr int SMX_PAD_COUNT = 2;
constexpr int SMX_PANEL_COUNT = 9;

// Initialization and Setup
bool InputHandler_Win32_SMX_Is_SMX_DLL_Available();
void InputHandler_Win32_SMX_Register_Pad();

class InputHandler_Win32_SMX: public InputHandler
{
public:
	// Constructor and Destructor
	InputHandler_Win32_SMX();
	~InputHandler_Win32_SMX();

	// Core Functionality
	void SMX_Start();
	void ProcessInputEvent(int pad);
	int InitializeSMX();
	int IsPadConnected();
	int GetStageStatus();
	bool IsSMXReady();

	// Utility and Helper Functions
	void GetDevicesAndDescriptions(std::vector<InputDeviceInfo>& vDevicesOut);
	RString GetDeviceSpecificInputString(const DeviceInput& di);
	void SMX_SetLogCallback();
private:
	// Internal Logic
	void SMX_GetInfo(int pad, struct SMXInfo* info);
	uint16_t SMX_GetInputState(int pad);
	void SMX_Stop();
	void SMX_Restart();

	// Member Variables
	uint16_t m_padInputStates[SMX_PAD_COUNT];
};

enum SMXUpdateCallbackReason {
	SMXUpdateCallback_Updated,
	SMXUpdateCallback_FactoryResetCommandComplete
};
typedef void SMXUpdateCallback(int pad, SMXUpdateCallbackReason reason, void* pUser);

struct SMXInfo {
	bool m_bConnected = false;
	char m_Serial[33];
	uint16_t m_iFirmwareVersion;
};
typedef void SMXLogCallback(const char* log);

#endif
