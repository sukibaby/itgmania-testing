//
//  InputHandler_NSEvent.hpp
//  StepMania
//
//  Created by heshuimu on 12/22/19.
//

#ifndef INPUTHANDLER_NSEVENT_HPP
#define INPUTHANDLER_NSEVENT_HPP

#include "InputHandler.h"
#include "archutils/Darwin/CocoaEventDispatcher.h"

class InputHandler_NSEvent : public InputHandler {
 public:
  InputHandler_NSEvent();
  ~InputHandler_NSEvent();

  void GetDevicesAndDescriptions(std::vector<InputDeviceInfo>& vDevicesOut);

 protected:
  void HandleEvent(EVENT_TYPE e);
  void InitKeyCodeMap();

 private:
  DeviceButton m_NSKeyCodeMap[0x100];
  unsigned m_ResponderID;
};
#endif
