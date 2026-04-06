#pragma once

void RegisterEmergencyShutdownCallback(void (*pFunc)());
void DoEmergencyShutdown();
