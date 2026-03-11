#include <map>
#include <string>
#include <vector>

#include "IniFile.h"
#include "InputFilter.h"
#include "InputMapper.h"
#include "RageInput.h"
#include "RageInputDevice.h"
#include "RageUtil.h"
#include "ScreenEdit.h"
#include "SpecialFiles.h"

namespace {
std::map<std::string, EditButton> name_to_edit_button;
}

void ScreenEdit::InitEditMappings() {
  // Created courtesy of query replace regex.
  name_to_edit_button["COLUMN_0"] = EDIT_BUTTON_COLUMN_0;
  name_to_edit_button["COLUMN_1"] = EDIT_BUTTON_COLUMN_1;
  name_to_edit_button["COLUMN_2"] = EDIT_BUTTON_COLUMN_2;
  name_to_edit_button["COLUMN_3"] = EDIT_BUTTON_COLUMN_3;
  name_to_edit_button["COLUMN_4"] = EDIT_BUTTON_COLUMN_4;
  name_to_edit_button["COLUMN_5"] = EDIT_BUTTON_COLUMN_5;
  name_to_edit_button["COLUMN_6"] = EDIT_BUTTON_COLUMN_6;
  name_to_edit_button["COLUMN_7"] = EDIT_BUTTON_COLUMN_7;
  name_to_edit_button["COLUMN_8"] = EDIT_BUTTON_COLUMN_8;
  name_to_edit_button["COLUMN_9"] = EDIT_BUTTON_COLUMN_9;

  name_to_edit_button["RIGHT_SIDE"] = EDIT_BUTTON_RIGHT_SIDE;
  name_to_edit_button["LAY_ROLL"] = EDIT_BUTTON_LAY_ROLL;
  name_to_edit_button["LAY_TAP_ATTACK"] = EDIT_BUTTON_LAY_TAP_ATTACK;
  name_to_edit_button["REMOVE_NOTE"] = EDIT_BUTTON_REMOVE_NOTE;

  name_to_edit_button["CYCLE_TAP_LEFT"] = EDIT_BUTTON_CYCLE_TAP_LEFT;
  name_to_edit_button["CYCLE_TAP_RIGHT"] = EDIT_BUTTON_CYCLE_TAP_RIGHT;

  name_to_edit_button["CYCLE_SEGMENT_LEFT"] = EDIT_BUTTON_CYCLE_SEGMENT_LEFT;
  name_to_edit_button["CYCLE_SEGMENT_RIGHT"] = EDIT_BUTTON_CYCLE_SEGMENT_RIGHT;

  name_to_edit_button["SCROLL_UP_LINE"] = EDIT_BUTTON_SCROLL_UP_LINE;
  name_to_edit_button["SCROLL_UP_PAGE"] = EDIT_BUTTON_SCROLL_UP_PAGE;
  name_to_edit_button["SCROLL_UP_TS"] = EDIT_BUTTON_SCROLL_UP_TS;
  name_to_edit_button["SCROLL_DOWN_LINE"] = EDIT_BUTTON_SCROLL_DOWN_LINE;
  name_to_edit_button["SCROLL_DOWN_PAGE"] = EDIT_BUTTON_SCROLL_DOWN_PAGE;
  name_to_edit_button["SCROLL_DOWN_TS"] = EDIT_BUTTON_SCROLL_DOWN_TS;
  name_to_edit_button["SCROLL_NEXT_MEASURE"] = EDIT_BUTTON_SCROLL_NEXT_MEASURE;
  name_to_edit_button["SCROLL_PREV_MEASURE"] = EDIT_BUTTON_SCROLL_PREV_MEASURE;
  name_to_edit_button["SCROLL_HOME"] = EDIT_BUTTON_SCROLL_HOME;
  name_to_edit_button["SCROLL_END"] = EDIT_BUTTON_SCROLL_END;
  name_to_edit_button["SCROLL_NEXT"] = EDIT_BUTTON_SCROLL_NEXT;
  name_to_edit_button["SCROLL_PREV"] = EDIT_BUTTON_SCROLL_PREV;

  name_to_edit_button["SEGMENT_NEXT"] = EDIT_BUTTON_SEGMENT_NEXT;
  name_to_edit_button["SEGMENT_PREV"] = EDIT_BUTTON_SEGMENT_PREV;

  name_to_edit_button["SCROLL_SELECT"] = EDIT_BUTTON_SCROLL_SELECT;

  name_to_edit_button["LAY_SELECT"] = EDIT_BUTTON_LAY_SELECT;

  name_to_edit_button["SCROLL_SPEED_UP"] = EDIT_BUTTON_SCROLL_SPEED_UP;
  name_to_edit_button["SCROLL_SPEED_DOWN"] = EDIT_BUTTON_SCROLL_SPEED_DOWN;

  name_to_edit_button["SNAP_NEXT"] = EDIT_BUTTON_SNAP_NEXT;
  name_to_edit_button["SNAP_PREV"] = EDIT_BUTTON_SNAP_PREV;

  name_to_edit_button["OPEN_EDIT_MENU"] = EDIT_BUTTON_OPEN_EDIT_MENU;
  name_to_edit_button["OPEN_TIMING_MENU"] = EDIT_BUTTON_OPEN_TIMING_MENU;
  name_to_edit_button["OPEN_ALTER_MENU"] = EDIT_BUTTON_OPEN_ALTER_MENU;
  name_to_edit_button["OPEN_AREA_MENU"] = EDIT_BUTTON_OPEN_AREA_MENU;
  name_to_edit_button["OPEN_BGCHANGE_LAYER1_MENU"] =
      EDIT_BUTTON_OPEN_BGCHANGE_LAYER1_MENU;
  name_to_edit_button["OPEN_BGCHANGE_LAYER2_MENU"] =
      EDIT_BUTTON_OPEN_BGCHANGE_LAYER2_MENU;
  name_to_edit_button["OPEN_COURSE_MENU"] = EDIT_BUTTON_OPEN_COURSE_MENU;
  name_to_edit_button["OPEN_COURSE_ATTACK_MENU"] =
      EDIT_BUTTON_OPEN_COURSE_ATTACK_MENU;

  name_to_edit_button["OPEN_STEP_ATTACK_MENU"] =
      EDIT_BUTTON_OPEN_STEP_ATTACK_MENU;
  name_to_edit_button["ADD_STEP_MODS"] = EDIT_BUTTON_ADD_STEP_MODS;

  name_to_edit_button["OPEN_INPUT_HELP"] = EDIT_BUTTON_OPEN_INPUT_HELP;

  name_to_edit_button["BAKE_RANDOM_FROM_SONG_GROUP"] =
      EDIT_BUTTON_BAKE_RANDOM_FROM_SONG_GROUP;
  name_to_edit_button["BAKE_RANDOM_FROM_SONG_GROUP_AND_GENRE"] =
      EDIT_BUTTON_BAKE_RANDOM_FROM_SONG_GROUP_AND_GENRE;

  name_to_edit_button["PLAY_FROM_START"] = EDIT_BUTTON_PLAY_FROM_START;
  name_to_edit_button["PLAY_FROM_CURSOR"] = EDIT_BUTTON_PLAY_FROM_CURSOR;
  name_to_edit_button["PLAY_SELECTION"] = EDIT_BUTTON_PLAY_SELECTION;
  name_to_edit_button["RECORD_FROM_CURSOR"] = EDIT_BUTTON_RECORD_FROM_CURSOR;
  name_to_edit_button["RECORD_SELECTION"] = EDIT_BUTTON_RECORD_SELECTION;

  name_to_edit_button["RECORD_HOLD_RESET"] = EDIT_BUTTON_RECORD_HOLD_RESET;
  name_to_edit_button["RECORD_HOLD_OFF"] = EDIT_BUTTON_RECORD_HOLD_OFF;
  name_to_edit_button["RECORD_HOLD_MORE"] = EDIT_BUTTON_RECORD_HOLD_MORE;
  name_to_edit_button["RECORD_HOLD_LESS"] = EDIT_BUTTON_RECORD_HOLD_LESS;

  name_to_edit_button["RETURN_TO_EDIT"] = EDIT_BUTTON_RETURN_TO_EDIT;

  name_to_edit_button["INSERT"] = EDIT_BUTTON_INSERT;
  name_to_edit_button["DELETE"] = EDIT_BUTTON_DELETE;
  name_to_edit_button["INSERT_SHIFT_PAUSES"] = EDIT_BUTTON_INSERT_SHIFT_PAUSES;
  name_to_edit_button["DELETE_SHIFT_PAUSES"] = EDIT_BUTTON_DELETE_SHIFT_PAUSES;

  name_to_edit_button["OPEN_NEXT_STEPS"] = EDIT_BUTTON_OPEN_NEXT_STEPS;
  name_to_edit_button["OPEN_PREV_STEPS"] = EDIT_BUTTON_OPEN_PREV_STEPS;
  name_to_edit_button["PLAY_SAMPLE_MUSIC"] = EDIT_BUTTON_PLAY_SAMPLE_MUSIC;

  name_to_edit_button["BPM_UP"] = EDIT_BUTTON_BPM_UP;
  name_to_edit_button["BPM_DOWN"] = EDIT_BUTTON_BPM_DOWN;
  name_to_edit_button["STOP_UP"] = EDIT_BUTTON_STOP_UP;
  name_to_edit_button["STOP_DOWN"] = EDIT_BUTTON_STOP_DOWN;

  name_to_edit_button["DELAY_UP"] = EDIT_BUTTON_DELAY_UP;
  name_to_edit_button["DELAY_DOWN"] = EDIT_BUTTON_DELAY_DOWN;

  name_to_edit_button["OFFSET_UP"] = EDIT_BUTTON_OFFSET_UP;
  name_to_edit_button["OFFSET_DOWN"] = EDIT_BUTTON_OFFSET_DOWN;
  name_to_edit_button["SAMPLE_START_UP"] = EDIT_BUTTON_SAMPLE_START_UP;
  name_to_edit_button["SAMPLE_START_DOWN"] = EDIT_BUTTON_SAMPLE_START_DOWN;
  name_to_edit_button["SAMPLE_LENGTH_UP"] = EDIT_BUTTON_SAMPLE_LENGTH_UP;
  name_to_edit_button["SAMPLE_LENGTH_DOWN"] = EDIT_BUTTON_SAMPLE_LENGTH_DOWN;

  name_to_edit_button["ADJUST_FINE"] = EDIT_BUTTON_ADJUST_FINE;

  name_to_edit_button["SAVE"] = EDIT_BUTTON_SAVE;

  name_to_edit_button["UNDO"] = EDIT_BUTTON_UNDO;

  name_to_edit_button["ADD_COURSE_MODS"] = EDIT_BUTTON_ADD_COURSE_MODS;

  name_to_edit_button["SWITCH_PLAYERS"] = EDIT_BUTTON_SWITCH_PLAYERS;

  name_to_edit_button["SWITCH_TIMINGS"] = EDIT_BUTTON_SWITCH_TIMINGS;

  m_EditMappingsDeviceInput.Clear();

  // Common mappings:

  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SCROLL_UP_LINE][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_UP);
  m_EditMappingsMenuButton.button[EDIT_BUTTON_SCROLL_UP_LINE][0] =
      GAME_BUTTON_UP;
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SCROLL_DOWN_LINE][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_DOWN);
  m_EditMappingsMenuButton.button[EDIT_BUTTON_SCROLL_DOWN_LINE][0] =
      GAME_BUTTON_DOWN;

  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SCROLL_UP_PAGE][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_PGUP);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SCROLL_UP_PAGE][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_SEMICOLON);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SCROLL_DOWN_PAGE][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_PGDN);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SCROLL_DOWN_PAGE][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_SQUOTE);

#if defined(MACOSX)
  // On macOS, Control+Up/Down are reserved for Mission Control
  // Use different keys instead
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SCROLL_SPEED_UP][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_UP);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_SCROLL_SPEED_UP][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_LALT);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_SCROLL_SPEED_UP][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_RALT);

  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SCROLL_SPEED_DOWN][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_DOWN);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_SCROLL_SPEED_DOWN][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_LALT);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_SCROLL_SPEED_DOWN][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_RALT);
#else
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SCROLL_SPEED_UP][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_UP);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_SCROLL_SPEED_UP][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_LCTRL);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_SCROLL_SPEED_UP][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_RCTRL);

  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SCROLL_SPEED_DOWN][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_DOWN);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_SCROLL_SPEED_DOWN][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_LCTRL);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_SCROLL_SPEED_DOWN][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_RCTRL);
#endif

  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SCROLL_SELECT][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_LSHIFT);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SCROLL_SELECT][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_RSHIFT);

  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SNAP_NEXT][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_LEFT);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SNAP_PREV][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_RIGHT);

#if defined(MACOSX)
  // On macOS, Control+PgUp/PgDn are reserved for Mission Control
  // Use Alt instead of Ctrl to avoid conflicts
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_SCROLL_UP_TS][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_LALT);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_SCROLL_UP_TS][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_RALT);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SCROLL_UP_TS][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_PGUP);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SCROLL_UP_TS][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_SEMICOLON);

  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_SCROLL_DOWN_TS][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_LALT);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_SCROLL_DOWN_TS][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_RALT);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SCROLL_DOWN_TS][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_PGDN);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SCROLL_DOWN_TS][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_SQUOTE);
#else
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_SCROLL_UP_TS][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_LCTRL);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_SCROLL_UP_TS][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_RCTRL);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SCROLL_UP_TS][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_PGUP);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SCROLL_UP_TS][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_SEMICOLON);

  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_SCROLL_DOWN_TS][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_LCTRL);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_SCROLL_DOWN_TS][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_RCTRL);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SCROLL_DOWN_TS][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_PGDN);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SCROLL_DOWN_TS][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_SQUOTE);
#endif

  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SCROLL_HOME][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_HOME);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SCROLL_END][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_END);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SCROLL_NEXT][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_PERIOD);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SCROLL_PREV][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_COMMA);

  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SEGMENT_NEXT][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_PERIOD);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_SEGMENT_NEXT][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_LCTRL);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_SEGMENT_NEXT][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_RCTRL);

  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SEGMENT_PREV][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_COMMA);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_SEGMENT_PREV][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_LCTRL);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_SEGMENT_PREV][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_RCTRL);

  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SCROLL_SELECT][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_LSHIFT);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SCROLL_SELECT][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_RSHIFT);

  m_EditMappingsDeviceInput.button[EDIT_BUTTON_LAY_SELECT][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_SPACE);

  m_EditMappingsDeviceInput.button[EDIT_BUTTON_PLAY_FROM_START][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_Cp);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_PLAY_FROM_START][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_LCTRL);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_PLAY_FROM_START][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_RCTRL);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_PLAY_FROM_CURSOR][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_Cp);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_PLAY_FROM_CURSOR][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_LSHIFT);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_PLAY_FROM_CURSOR][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_RSHIFT);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_PLAY_SELECTION][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_Cp);

  // EditMode-specific mappings, for overriding options that don't make sense in
  // one mode vs. another.
  switch (EDIT_MODE.GetValue()) {
    case EditMode_Practice:
      // F1 = Show help popup
      m_EditMappingsDeviceInput.button[EDIT_BUTTON_OPEN_INPUT_HELP][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_F1);

      // Esc = Show Edit Menu
      m_EditMappingsDeviceInput.button[EDIT_BUTTON_OPEN_EDIT_MENU][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_ESC);
      m_EditMappingsMenuButton.button[EDIT_BUTTON_OPEN_EDIT_MENU][0] =
          GAME_BUTTON_START;
      m_EditMappingsMenuButton.button[EDIT_BUTTON_OPEN_EDIT_MENU][1] =
          GAME_BUTTON_BACK;

      // Escape, Enter = exit play/record
      m_PlayMappingsDeviceInput.button[EDIT_BUTTON_RETURN_TO_EDIT][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_ENTER);
      m_PlayMappingsDeviceInput.button[EDIT_BUTTON_RETURN_TO_EDIT][1] =
          DeviceInput(DEVICE_KEYBOARD, KEY_ESC);
      m_PlayMappingsMenuButton.button[EDIT_BUTTON_RETURN_TO_EDIT][1] =
          GAME_BUTTON_BACK;
      return;
    case EditMode_CourseMods:
      // Left/Right = Snap to Next/Prev
      m_EditMappingsDeviceInput.button[EDIT_BUTTON_SNAP_NEXT][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_LEFT);
      m_EditMappingsDeviceInput.button[EDIT_BUTTON_SNAP_PREV][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_RIGHT);

      // v = course playback menu
      // m_EditMappingsDeviceInput.button[EDIT_BUTTON_OPEN_COURSE_ATTACK_MENU][0]
      // = DeviceInput(DEVICE_KEYBOARD, KEY_Cv);
      m_EditMappingsDeviceInput.button[EDIT_BUTTON_ADD_COURSE_MODS][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_Co);
      m_EditMappingsDeviceInput.button[EDIT_BUTTON_OPEN_COURSE_MENU][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_Cv);

      // F1 = Show help popup
      m_EditMappingsDeviceInput.button[EDIT_BUTTON_OPEN_INPUT_HELP][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_F1);

      // Esc = Show Edit Menu
      m_EditMappingsDeviceInput.button[EDIT_BUTTON_OPEN_EDIT_MENU][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_ESC);
      m_EditMappingsMenuButton.button[EDIT_BUTTON_OPEN_EDIT_MENU][0] =
          GAME_BUTTON_START;
      m_EditMappingsMenuButton.button[EDIT_BUTTON_OPEN_EDIT_MENU][1] =
          GAME_BUTTON_BACK;

      // Escape, Enter = exit play/record
      m_PlayMappingsDeviceInput.button[EDIT_BUTTON_RETURN_TO_EDIT][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_ENTER);
      m_PlayMappingsDeviceInput.button[EDIT_BUTTON_RETURN_TO_EDIT][1] =
          DeviceInput(DEVICE_KEYBOARD, KEY_ESC);
      m_PlayMappingsMenuButton.button[EDIT_BUTTON_RETURN_TO_EDIT][0] =
          GAME_BUTTON_START;
      return;
    case EditMode_Full:
      // F4 = Show timing menu
      m_EditMappingsDeviceInput.button[EDIT_BUTTON_OPEN_TIMING_MENU][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_F4);

      /* Don't allow F5/F6 in home mode. It breaks the "delay creation until
       * first save" logic. */
      m_EditMappingsDeviceInput.button[EDIT_BUTTON_OPEN_PREV_STEPS][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_F5);
      m_EditMappingsDeviceInput.button[EDIT_BUTTON_OPEN_NEXT_STEPS][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_F6);
      m_EditMappingsDeviceInput.button[EDIT_BUTTON_BPM_DOWN][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_F7);
      m_EditMappingsDeviceInput.button[EDIT_BUTTON_BPM_UP][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_F8);
      m_EditMappingsDeviceInput.button[EDIT_BUTTON_STOP_DOWN][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_F9);
      m_EditMappingsDeviceInput.button[EDIT_BUTTON_STOP_UP][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_F10);

      m_EditMappingsDeviceInput.button[EDIT_BUTTON_DELAY_DOWN][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_F9);
      m_EditMappingsDeviceInput.hold[EDIT_BUTTON_DELAY_DOWN][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_LSHIFT);
      m_EditMappingsDeviceInput.hold[EDIT_BUTTON_DELAY_DOWN][1] =
          DeviceInput(DEVICE_KEYBOARD, KEY_RSHIFT);
      m_EditMappingsDeviceInput.button[EDIT_BUTTON_DELAY_UP][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_F10);
      m_EditMappingsDeviceInput.hold[EDIT_BUTTON_DELAY_UP][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_LSHIFT);
      m_EditMappingsDeviceInput.hold[EDIT_BUTTON_DELAY_UP][1] =
          DeviceInput(DEVICE_KEYBOARD, KEY_RSHIFT);

      m_EditMappingsDeviceInput.button[EDIT_BUTTON_OFFSET_DOWN][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_F11);
      m_EditMappingsDeviceInput.button[EDIT_BUTTON_OFFSET_UP][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_F12);

      m_EditMappingsDeviceInput.button[EDIT_BUTTON_SAMPLE_START_UP][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_RBRACKET);
      m_EditMappingsDeviceInput.button[EDIT_BUTTON_SAMPLE_START_DOWN][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_LBRACKET);
      m_EditMappingsDeviceInput.button[EDIT_BUTTON_SAMPLE_LENGTH_UP][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_RBRACKET);
      m_EditMappingsDeviceInput.hold[EDIT_BUTTON_SAMPLE_LENGTH_UP][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_LSHIFT);
      m_EditMappingsDeviceInput.hold[EDIT_BUTTON_SAMPLE_LENGTH_UP][1] =
          DeviceInput(DEVICE_KEYBOARD, KEY_RSHIFT);
      m_EditMappingsDeviceInput.button[EDIT_BUTTON_SAMPLE_LENGTH_DOWN][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_LBRACKET);
      m_EditMappingsDeviceInput.hold[EDIT_BUTTON_SAMPLE_LENGTH_DOWN][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_LSHIFT);
      m_EditMappingsDeviceInput.hold[EDIT_BUTTON_SAMPLE_LENGTH_DOWN][1] =
          DeviceInput(DEVICE_KEYBOARD, KEY_RSHIFT);

      m_EditMappingsDeviceInput.button[EDIT_BUTTON_PLAY_SAMPLE_MUSIC][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_Cl);

      m_EditMappingsDeviceInput
          .button[EDIT_BUTTON_OPEN_BGCHANGE_LAYER1_MENU][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_Cb);
      m_EditMappingsDeviceInput
          .button[EDIT_BUTTON_OPEN_BGCHANGE_LAYER2_MENU][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_Cb);
      m_EditMappingsDeviceInput.hold[EDIT_BUTTON_OPEN_BGCHANGE_LAYER2_MENU][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_LSHIFT);
      m_EditMappingsDeviceInput.hold[EDIT_BUTTON_OPEN_BGCHANGE_LAYER2_MENU][1] =
          DeviceInput(DEVICE_KEYBOARD, KEY_RSHIFT);
      // m_EditMappingsDeviceInput.button[EDIT_BUTTON_ADD_STEP_MODS][0] =
      // DeviceInput(DEVICE_KEYBOARD, KEY_Cc);
      // m_EditMappingsDeviceInput.button[EDIT_BUTTON_OPEN_STEP_ATTACK_MENU][0]
      // = DeviceInput(DEVICE_KEYBOARD, KEY_Cv);

      m_EditMappingsDeviceInput.button[EDIT_BUTTON_INSERT_SHIFT_PAUSES][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_INSERT);
      m_EditMappingsDeviceInput.hold[EDIT_BUTTON_INSERT_SHIFT_PAUSES][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_LCTRL);
      m_EditMappingsDeviceInput.hold[EDIT_BUTTON_INSERT_SHIFT_PAUSES][1] =
          DeviceInput(DEVICE_KEYBOARD, KEY_RCTRL);
      m_EditMappingsDeviceInput.button[EDIT_BUTTON_DELETE_SHIFT_PAUSES][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_DEL);
      m_EditMappingsDeviceInput.hold[EDIT_BUTTON_DELETE_SHIFT_PAUSES][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_LCTRL);
      m_EditMappingsDeviceInput.hold[EDIT_BUTTON_DELETE_SHIFT_PAUSES][1] =
          DeviceInput(DEVICE_KEYBOARD, KEY_RCTRL);

      m_EditMappingsDeviceInput.button[EDIT_BUTTON_RECORD_HOLD_LESS][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_Cq);
      m_EditMappingsDeviceInput.button[EDIT_BUTTON_RECORD_HOLD_MORE][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_Cw);
      m_EditMappingsDeviceInput.button[EDIT_BUTTON_RECORD_HOLD_RESET][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_Ce);
      m_EditMappingsDeviceInput.button[EDIT_BUTTON_RECORD_HOLD_OFF][0] =
          DeviceInput(DEVICE_KEYBOARD, KEY_Cr);
      break;
    default:
      break;
  }

  m_EditMappingsDeviceInput.button[EDIT_BUTTON_COLUMN_0][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_C1);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_COLUMN_1][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_C2);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_COLUMN_2][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_C3);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_COLUMN_3][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_C4);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_COLUMN_4][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_C5);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_COLUMN_5][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_C6);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_COLUMN_6][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_C7);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_COLUMN_7][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_C8);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_COLUMN_8][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_C9);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_COLUMN_9][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_C0);

  m_EditMappingsDeviceInput.button[EDIT_BUTTON_RIGHT_SIDE][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_LALT);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_RIGHT_SIDE][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_RALT);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_LAY_ROLL][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_LSHIFT);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_LAY_ROLL][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_RSHIFT);

  m_EditMappingsDeviceInput.button[EDIT_BUTTON_CYCLE_TAP_LEFT][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_Cn);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_CYCLE_TAP_RIGHT][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_Cm);

  m_EditMappingsDeviceInput.button[EDIT_BUTTON_CYCLE_SEGMENT_LEFT][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_Cn);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_CYCLE_SEGMENT_LEFT][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_LCTRL);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_CYCLE_SEGMENT_LEFT][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_RCTRL);

  m_EditMappingsDeviceInput.button[EDIT_BUTTON_CYCLE_SEGMENT_RIGHT][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_Cm);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_CYCLE_SEGMENT_RIGHT][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_LCTRL);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_CYCLE_SEGMENT_RIGHT][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_RCTRL);

  m_EditMappingsDeviceInput.button[EDIT_BUTTON_OPEN_EDIT_MENU][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_ESC);
  m_EditMappingsMenuButton.button[EDIT_BUTTON_OPEN_EDIT_MENU][0] =
      GAME_BUTTON_START;
  m_EditMappingsMenuButton.button[EDIT_BUTTON_OPEN_EDIT_MENU][1] =
      GAME_BUTTON_BACK;
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_OPEN_AREA_MENU][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_ENTER);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_OPEN_ALTER_MENU][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_Ca);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_OPEN_INPUT_HELP][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_F1);

  m_EditMappingsDeviceInput.button[EDIT_BUTTON_BAKE_RANDOM_FROM_SONG_GROUP][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_Cb);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_BAKE_RANDOM_FROM_SONG_GROUP][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_LALT);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_BAKE_RANDOM_FROM_SONG_GROUP][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_RALT);
  m_EditMappingsDeviceInput
      .button[EDIT_BUTTON_BAKE_RANDOM_FROM_SONG_GROUP_AND_GENRE][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_Cb);
  m_EditMappingsDeviceInput
      .hold[EDIT_BUTTON_BAKE_RANDOM_FROM_SONG_GROUP_AND_GENRE][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_LCTRL);
  m_EditMappingsDeviceInput
      .hold[EDIT_BUTTON_BAKE_RANDOM_FROM_SONG_GROUP_AND_GENRE][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_RCTRL);

  m_EditMappingsDeviceInput.button[EDIT_BUTTON_RECORD_SELECTION][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_Cr);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_RECORD_SELECTION][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_LCTRL);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_RECORD_SELECTION][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_RCTRL);

  m_EditMappingsDeviceInput.button[EDIT_BUTTON_INSERT][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_INSERT);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_INSERT][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_BACKSLASH);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_DELETE][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_DEL);

  m_EditMappingsDeviceInput.button[EDIT_BUTTON_ADJUST_FINE][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_RALT);
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_ADJUST_FINE][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_LALT);

  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SAVE][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_Cs);
#if defined(MACOSX)
  /* use cmd */
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_SAVE][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_LMETA);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_SAVE][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_RMETA);
#else
  /* use ctrl */
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_SAVE][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_LCTRL);
  m_EditMappingsDeviceInput.hold[EDIT_BUTTON_SAVE][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_RCTRL);
#endif

  m_EditMappingsDeviceInput.button[EDIT_BUTTON_UNDO][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_Cu);

  // Switch players, if it makes sense to do so.
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SWITCH_PLAYERS][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_SLASH);

  // Allow song and step timing to be swapped.
  m_EditMappingsDeviceInput.button[EDIT_BUTTON_SWITCH_TIMINGS][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_Ct);

  m_PlayMappingsDeviceInput.button[EDIT_BUTTON_RETURN_TO_EDIT][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_ENTER);
  m_PlayMappingsDeviceInput.button[EDIT_BUTTON_RETURN_TO_EDIT][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_ESC);
  m_PlayMappingsMenuButton.button[EDIT_BUTTON_RETURN_TO_EDIT][0] =
      GAME_BUTTON_BACK;
  m_PlayMappingsMenuButton.button[EDIT_BUTTON_RETURN_TO_EDIT][1] =
      GAME_BUTTON_START;

  m_RecordMappingsDeviceInput.button[EDIT_BUTTON_LAY_ROLL][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_LSHIFT);
  m_RecordMappingsDeviceInput.button[EDIT_BUTTON_LAY_ROLL][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_RSHIFT);
  m_RecordMappingsDeviceInput.button[EDIT_BUTTON_REMOVE_NOTE][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_LALT);
  m_RecordMappingsDeviceInput.button[EDIT_BUTTON_REMOVE_NOTE][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_RALT);
  m_RecordMappingsDeviceInput.button[EDIT_BUTTON_RETURN_TO_EDIT][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_ESC);
  m_RecordMappingsDeviceInput.button[EDIT_BUTTON_RETURN_TO_EDIT][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_ENTER);
  m_RecordMappingsMenuButton.button[EDIT_BUTTON_RETURN_TO_EDIT][0] =
      GAME_BUTTON_BACK;
  m_RecordMappingsMenuButton.button[EDIT_BUTTON_RETURN_TO_EDIT][1] =
      GAME_BUTTON_START;

  m_RecordPausedMappingsDeviceInput.button[EDIT_BUTTON_PLAY_SELECTION][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_Cp);
  m_RecordPausedMappingsDeviceInput.button[EDIT_BUTTON_RECORD_SELECTION][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_Cr);
  m_RecordPausedMappingsDeviceInput.hold[EDIT_BUTTON_RECORD_SELECTION][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_LCTRL);
  m_RecordPausedMappingsDeviceInput.hold[EDIT_BUTTON_RECORD_SELECTION][1] =
      DeviceInput(DEVICE_KEYBOARD, KEY_RCTRL);
  m_RecordPausedMappingsDeviceInput.button[EDIT_BUTTON_RECORD_FROM_CURSOR][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_Cr);
  m_RecordPausedMappingsDeviceInput.button[EDIT_BUTTON_RETURN_TO_EDIT][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_ESC);
  m_RecordPausedMappingsMenuButton.button[EDIT_BUTTON_RETURN_TO_EDIT][1] =
      GAME_BUTTON_BACK;
  m_RecordPausedMappingsDeviceInput.button[EDIT_BUTTON_UNDO][0] =
      DeviceInput(DEVICE_KEYBOARD, KEY_Cu);

  IniFile mapping_ini;
  // Only use the mappings file if it exists.  It's meant to be optional, and
  // only used in rare cases like someone having critical keys broken. -Kyz
  if (mapping_ini.ReadFile(SpecialFiles::EDIT_MODE_KEYMAPS_PATH)) {
    LoadKeymapSectionIntoMappingsMember(
        mapping_ini.GetChild("Edit"), m_EditMappingsDeviceInput);
    LoadKeymapSectionIntoMappingsMember(
        mapping_ini.GetChild("Play"), m_PlayMappingsDeviceInput);
    LoadKeymapSectionIntoMappingsMember(
        mapping_ini.GetChild("Record"), m_RecordMappingsDeviceInput);
    LoadKeymapSectionIntoMappingsMember(
        mapping_ini.GetChild("RecordPaused"),
        m_RecordPausedMappingsDeviceInput);
  }
}

void ScreenEdit::LoadKeymapSectionIntoMappingsMember(
    const XNode* section, MapEditToDI& mappings) {
  if (section == nullptr) {
    return;
  }  // Not an error, sections are optional. -Kyz
  FOREACH_CONST_Attr(section, attr) {
    std::map<std::string, EditButton>::iterator name_entry =
        name_to_edit_button.find(attr->first);
    if (name_entry != name_to_edit_button.end()) {
      std::string joined_names;
      attr->second->GetValue(joined_names);
      std::vector<std::string> key_names;
      split(joined_names, DEVICE_INPUT_SEPARATOR, key_names, false);
      for (size_t k = 0; k < key_names.size() && k < NUM_EDIT_TO_DEVICE_SLOTS;
           ++k) {
        DeviceInput devi;
        devi.FromString(key_names[k]);
        if (devi.IsValid()) {
          mappings.button[name_entry->second][k] = devi;
        }
      }
    }
  }
}

/* Given a DeviceInput that was just depressed, return an active edit function.
 */
EditButton ScreenEdit::DeviceToEdit(const DeviceInput& DeviceI) const {
  ASSERT(DeviceI.IsValid());

  const MapEditToDI* pCurrentMap = GetCurrentDeviceInputMap();

  /* First, search to see if a key that requires a modifier is pressed. */
  FOREACH_EditButton(e) {
    for (int slot = 0; slot < NUM_EDIT_TO_DEVICE_SLOTS; ++slot) {
      if (pCurrentMap->button[e][slot] == DeviceI &&
          pCurrentMap->hold[e][0].IsValid()) {
        /* The button maps to this function, and has one or more shift modifiers
         * attached. */
        for (int holdslot = 0; holdslot < NUM_EDIT_TO_DEVICE_SLOTS;
             ++holdslot) {
          DeviceInput hDI = pCurrentMap->hold[e][holdslot];
          if (hDI.IsValid() && INPUTFILTER->IsBeingPressed(hDI)) {
            return e;
          }
        }
      }
    }
  }

  /* No shifted keys matched.  See if any unshifted inputs are bound to this
   * key. */
  FOREACH_EditButton(e) {
    for (int slot = 0; slot < NUM_EDIT_TO_DEVICE_SLOTS; ++slot) {
      if (pCurrentMap->button[e][slot] == DeviceI &&
          !pCurrentMap->hold[e][0].IsValid()) {
        /* The button maps to this function. */
        return e;
      }
    }
  }

  return EditButton_Invalid;
}

/* Given a DeviceInput that was just depressed, return an active edit function.
 */
EditButton ScreenEdit::MenuButtonToEditButton(GameButton MenuI) const {
  const MapEditButtonToMenuButton* pCurrentMap = GetCurrentMenuButtonMap();

  FOREACH_EditButton(e) {
    for (int slot = 0; slot < NUM_EDIT_TO_MENU_SLOTS; ++slot) {
      if (pCurrentMap->button[e][slot] == MenuI) {
        /* The button maps to this function. */
        return e;
      }
    }
  }

  return EditButton_Invalid;
}

/* If DeviceI was just pressed, return true if button is triggered.  (More than
 * one function may be mapped to a key.) */
bool ScreenEdit::EditPressed(EditButton button, const DeviceInput& DeviceI) {
  ASSERT(DeviceI.IsValid());

  const MapEditToDI* pCurrentMap = GetCurrentDeviceInputMap();

  /* First, search to see if a key that requires a modifier is pressed. */
  bool bPrimaryButtonPressed = false;
  for (int slot = 0; slot < NUM_EDIT_TO_DEVICE_SLOTS; ++slot) {
    if (pCurrentMap->button[button][slot] == DeviceI) {
      bPrimaryButtonPressed = true;
    }
  }

  if (!bPrimaryButtonPressed) {
    return false;
  }

  /* The button maps to this function.  Does the function has one or more shift
   * modifiers attached? */
  if (!pCurrentMap->hold[button][0].IsValid()) {
    return true;
  }

  for (int holdslot = 0; holdslot < NUM_EDIT_TO_DEVICE_SLOTS; ++holdslot) {
    DeviceInput hDI = pCurrentMap->hold[button][holdslot];
    if (INPUTFILTER->IsBeingPressed(hDI)) {
      return true;
    }
  }

  /* No shifted keys matched. */
  return false;
}

bool ScreenEdit::EditToDevice(
    EditButton button, int iSlotNum, DeviceInput& DeviceI) const {
  ASSERT(iSlotNum < NUM_EDIT_TO_DEVICE_SLOTS);
  const MapEditToDI* pCurrentMap = GetCurrentDeviceInputMap();
  if (pCurrentMap->button[button][iSlotNum].IsValid()) {
    DeviceI = pCurrentMap->button[button][iSlotNum];
  } else if (pCurrentMap->hold[button][iSlotNum].IsValid()) {
    DeviceI = pCurrentMap->hold[button][iSlotNum];
  }
  return DeviceI.IsValid();
}

bool ScreenEdit::EditIsBeingPressed(EditButton button) const {
  for (int slot = 0; slot < NUM_EDIT_TO_DEVICE_SLOTS; ++slot) {
    DeviceInput DeviceI;
    if (EditToDevice(button, slot, DeviceI) &&
        INPUTFILTER->IsBeingPressed(DeviceI)) {
      return true;
    }
  }

  return false;
}

const MapEditToDI* ScreenEdit::GetCurrentDeviceInputMap() const {
  switch (m_EditState) {
    DEFAULT_FAIL(m_EditState);
    case STATE_EDITING:
      return &m_EditMappingsDeviceInput;
    case STATE_PLAYING:
      return &m_PlayMappingsDeviceInput;
    case STATE_RECORDING:
      return &m_RecordMappingsDeviceInput;
    case STATE_RECORDING_PAUSED:
      return &m_RecordPausedMappingsDeviceInput;
  }
}

const MapEditButtonToMenuButton* ScreenEdit::GetCurrentMenuButtonMap() const {
  switch (m_EditState) {
    DEFAULT_FAIL(m_EditState);
    case STATE_EDITING:
      return &m_EditMappingsMenuButton;
    case STATE_PLAYING:
      return &m_PlayMappingsMenuButton;
    case STATE_RECORDING:
      return &m_RecordMappingsMenuButton;
    case STATE_RECORDING_PAUSED:
      return &m_RecordPausedMappingsMenuButton;
  }
}
