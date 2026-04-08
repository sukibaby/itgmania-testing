#ifndef EMERGENCYSHUTDOWN_H
#define EMERGENCYSHUTDOWN_H

void RegisterEmergencyShutdownCallback(void (*pFunc)());
void DoEmergencyShutdown();
#endif
