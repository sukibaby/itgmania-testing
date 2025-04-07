#ifndef INPUT_HANDLER_WIN32_SMX_H
#define INPUT_HANDLER_WIN32_SMX_H
#endif

#include "InputHandler.h"

static bool _smxdll_loaded = false;
static const int SMX_PAD_COUNT = 2;

bool Attempt_SMX_DLL_Load();

enum SMXUpdateCallbackReason {
	SMXUpdateCallback_Updated,
	SMXUpdateCallback_FactoryResetCommandComplete
};
typedef void SMXUpdateCallback(int pad, SMXUpdateCallbackReason reason, void* pUser);

class InputHandler_Win32_SMX: public InputHandler
{
public:
	InputHandler_Win32_SMX();
	void GetDevicesAndDescriptions( std::vector<InputDeviceInfo>& vDevicesOut );
	RString GetDeviceSpecificInputString( const DeviceInput &di );
	void ProcessPoll( int pad );

private:
	uint16_t m_padInputStates[SMX_PAD_COUNT];

	void SMX_Start( SMXUpdateCallback UpdateCallback, void *pUser );
    uint16_t SMX_GetInputState( int pad );
};
