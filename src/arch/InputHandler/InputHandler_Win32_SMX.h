#ifndef INPUT_HANDLER_WIN32_SMX_H
#define INPUT_HANDLER_WIN32_SMX_H

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

 #endif
