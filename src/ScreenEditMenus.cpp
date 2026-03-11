#include "ScreenEditMenus.h"

#include "BackgroundUtil.h"
#include "RageUtil.h"
#include "RageUtil/RandomNumbers.h"

namespace {

enum RowCountChoice {
  RCC_4M,
  RCC_2M,
  RCC_1M,
  RCC_2ND,
  RCC_4TH,
  RCC_8TH,
  RCC_12TH,
  RCC_16TH,
  RCC_24TH,
  RCC_32ND,
  RCC_48TH,
  RCC_64TH,
  RCC_192ND,
};


#define RCC_CHOICES                                                       \
    RCC_4TH, "4m", "2m", "1m", "2nd", "4th", "8th", "12th", "16th", "24th", \
            "32nd", "48th", "64th", "192nd"

bool EnabledIfClipboardTimingIsSafe();
bool EnabledIfClipboardTimingIsSafe() {
  TimingData* clipboard_full_timing = ScreenEditClipboardTimingSlot();
  return clipboard_full_timing != nullptr &&
         clipboard_full_timing->IsSafeFullTiming();
}

}  // namespace

static bool EnabledIfSet1SongBGAnimation();
static bool EnabledIfSet1SongMovie();
static bool EnabledIfSet1SongBitmap();
static bool EnabledIfSet1GlobalBGAnimation();
static bool EnabledIfSet1GlobalMovie();
static bool EnabledIfSet1GlobalMovieSongGroup();
static bool EnabledIfSet1GlobalMovieSongGroupAndGenre();
static bool EnabledIfSet2SongBGAnimation();
static bool EnabledIfSet2SongMovie();
static bool EnabledIfSet2SongBitmap();
static bool EnabledIfSet2GlobalBGAnimation();
static bool EnabledIfSet2GlobalMovie();
static bool EnabledIfSet2GlobalMovieSongGroup();
static bool EnabledIfSet2GlobalMovieSongGroupAndGenre();

int GetRowsFromAnswers(int choice, const std::vector<int>& answers) {
  if (answers.empty()) {
    return 192 / 8;
  }
  switch (answers[choice]) {
    case RCC_4M:
      return 192 * 4;
    case RCC_2M:
      return 192 * 2;
    case RCC_1M:
      return 192 * 1;
    case RCC_2ND:
      return 192 / 2;
    case RCC_4TH:
    default:
      return 192 / 4;
    case RCC_8TH:
      return 192 / 8;
    case RCC_12TH:
      return 192 / 12;
    case RCC_16TH:
      return 192 / 16;
    case RCC_24TH:
      return 192 / 24;
    case RCC_32ND:
      return 192 / 32;
    case RCC_48TH:
      return 192 / 48;
    case RCC_64TH:
      return 192 / 64;
    case RCC_192ND:
      return 192 / 192;
  }
}

TimingData*& ScreenEditClipboardTimingSlot() {
  static TimingData* clipboard_full_timing = nullptr;
  return clipboard_full_timing;
}

MenuDef g_EditHelp(
    "ScreenMiniMenuEditHelp"
    // fill this in dynamically
);

MenuDef g_AttackAtTimeMenu(
    "ScreenMiniMenuAttackAtTimeMenu"
    // fill this in dynamically
);

MenuDef g_IndividualAttack(
    "ScreenMiniMenuIndividualAttack"
    // fill this in dynamically
);

MenuDef g_KeysoundTrack(
    "ScreenMiniMenuKeysoundTrack");  // fill this in dynamically

MenuDef g_MainMenu(
    "ScreenMiniMenuMainMenu",
    MenuRowDef(
        ScreenEdit::play_whole_song, "Play whole song", true, EditMode_Practice,
        true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::play_current_beat_to_end, "Play current beat to end", true,
        EditMode_Practice, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::save, "Save", true, EditMode_Home, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::play_selection, "Play selection", true, EditMode_Practice,
        true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::set_selection_start, "Set selection start", true,
        EditMode_Practice, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::set_selection_end, "Set selection end", true,
        EditMode_Practice, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::revert_to_last_save, "Revert to last save", true,
        EditMode_Home, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::revert_from_disk, "Revert from disk", true, EditMode_Full,
        true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::options, "Editor options", true, EditMode_Practice, true,
        true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::edit_song_info, "Edit song info", true, EditMode_Full, true,
        true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::edit_steps_information, "Edit steps information", true,
        EditMode_Practice, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::edit_timing_data, "Edit Timing Data", true, EditMode_Full,
        true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::view_steps_data, "View steps data", true, EditMode_Full,
        true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::play_preview_music, "Play preview music", true,
        EditMode_Full, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::exit, "Exit Edit Mode", true, EditMode_Practice, true, true,
        0, nullptr));

MenuDef g_AlterMenu(
    "ScreenMiniMenuAlterMenu",
    MenuRowDef(
        ScreenEdit::cut, "Cut", true, EditMode_Practice, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::copy, "Copy", true, EditMode_Practice, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::clear, "Clear area", true, EditMode_Practice, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::quantize, "Quantize", true, EditMode_Practice, true, true,
        0, "4th", "8th", "12th", "16th", "24th", "32nd", "48th", "64th",
        "192nd"),
    MenuRowDef(
        ScreenEdit::turn, "Turn", true, EditMode_Practice, true, true, 0,
        "Left", "Right", "Mirror", "LRMirror", "UDMirror", "Backwards",
        "Shuffle", "SuperShuffle", "HyperShuffle"),
    MenuRowDef(
        ScreenEdit::transform, "Transform", true, EditMode_Practice, true, true,
        0, "NoHolds", "NoMines", "Little", "Wide", "Big", "Quick", "Skippy",
        "Mines", "Echo", "Stomp", "Planted", "Floored", "Twister", "NoJumps",
        "NoHands", "NoQuads", "NoStretch"),
    MenuRowDef(
        ScreenEdit::alter, "Alter", true, EditMode_Practice, true, true, 0,
        "Autogen To Fill Width", "Backwards", "Swap Sides",
        "Copy Left To Right", "Copy Right To Left", "Clear Left", "Clear Right",
        "Collapse To One", "Collapse Left", "Shift Left", "Shift Right",
        "Swap Up/Down", "Arbitrary Remap Columns"),
    MenuRowDef(
        ScreenEdit::tempo, "Tempo", true, EditMode_Full, true, true, 0,
        "Compress 2x", "Compress 3->2", "Compress 4->3", "Expand 3->4",
        "Expand 2->3", "Expand 2x"),
    MenuRowDef(
        ScreenEdit::play, "Play selection", true, EditMode_Practice, true, true,
        0, nullptr),
    MenuRowDef(
        ScreenEdit::record, "Record in selection", true, EditMode_Practice,
        true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::preview_designation, "Designate as Music Preview", true,
        EditMode_Full, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::convert_to_pause, "Convert selection to pause", true,
        EditMode_Full, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::convert_to_delay, "Convert selection to delay", true,
        EditMode_Full, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::convert_to_warp, "Convert selection to warp", true,
        EditMode_Full, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::convert_to_fake, "Convert selection to fake", true,
        EditMode_Full, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::convert_to_attack, "Convert selection to attack", true,
        EditMode_Full, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::routine_invert_notes, "Invert notes' player", true,
        EditMode_Full, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::routine_mirror_1_to_2, "Mirror Player 1 to 2", true,
        EditMode_Full, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::routine_mirror_2_to_1, "Mirror Player 2 to 1", true,
        EditMode_Full, true, true, 0, nullptr));

MenuDef g_AreaMenu(
    "ScreenMiniMenuAreaMenu",
    MenuRowDef(
        ScreenEdit::paste_at_current_beat, "Paste at current beat", true,
        EditMode_Practice, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::paste_at_begin_marker, "Paste at begin marker", true,
        EditMode_Practice, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::insert_and_shift, "Insert beat and shift down", true,
        EditMode_Practice, true, true, RCC_CHOICES),
    MenuRowDef(
        ScreenEdit::delete_and_shift, "Delete beat and shift up", true,
        EditMode_Practice, true, true, RCC_CHOICES),
    MenuRowDef(
        ScreenEdit::shift_pauses_forward, "Shift all timing changes down", true,
        EditMode_Full, true, true, RCC_CHOICES),
    MenuRowDef(
        ScreenEdit::shift_pauses_backward, "Shift all timing changes up", true,
        EditMode_Full, true, true, RCC_CHOICES),
    MenuRowDef(
        ScreenEdit::convert_pause_to_beat, "Convert pause to beats", true,
        EditMode_Full, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::convert_delay_to_beat, "Convert delay to beats", true,
        EditMode_Full, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::last_second_at_beat,
        "Designate last second at current beat", true, EditMode_Full, true,
        true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::undo, "Undo", true, EditMode_Practice, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::clear_clipboard, "Clear clipboard", true, EditMode_Practice,
        true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::modify_attacks_at_row, "Modify Attacks at current beat",
        true, EditMode_CourseMods, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::modify_keysounds_at_row, "Modify Keysounds at current beat",
        true, EditMode_Full, true, true, 0, nullptr)

);

MenuDef g_StepsInformation(
    "ScreenMiniMenuStepsInformation",
    MenuRowDef(
        ScreenEdit::difficulty, "Difficulty", true, EditMode_Practice, true,
        true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::meter, "Meter", true, EditMode_Practice, true, false, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::predict_meter, "Predicted Meter", false, EditMode_Full,
        true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::chartname, "Chart Name", true, EditMode_Practice, true,
        true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::description, "Description", true, EditMode_Practice, true,
        true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::chartstyle, "Chart Style", true, EditMode_Practice, true,
        true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::step_credit, "Step Author", true, EditMode_Practice, true,
        true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::step_display_bpm, "Display BPM", true, EditMode_Full, true,
        true, 0, "Actual", "Specified", "Random"),
    MenuRowDef(
        ScreenEdit::step_min_bpm, "Min BPM", true, EditMode_Full, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::step_max_bpm, "Max BPM", true, EditMode_Full, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::step_music, "Music File", true, EditMode_Full, true, true,
        0, nullptr));

MenuDef g_StepsData(
    "ScreenMiniMenuStepsData",
    MenuRowDef(
        ScreenEdit::tap_notes, "Tap Steps", false, EditMode_Full, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::jumps, "Jumps", false, EditMode_Full, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::hands, "Hands", false, EditMode_Full, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::quads, "Quads", false, EditMode_Full, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::holds, "Holds", false, EditMode_Full, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::mines, "Mines", false, EditMode_Full, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::rolls, "Rolls", false, EditMode_Full, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::lifts, "Lifts", false, EditMode_Full, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::fakes, "Fakes", false, EditMode_Full, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::stream, "Stream", false, EditMode_Full, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::voltage, "Voltage", false, EditMode_Full, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::air, "Air", false, EditMode_Full, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::freeze, "Freeze", false, EditMode_Full, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::chaos, "Chaos", false, EditMode_Full, true, true, 0,
        nullptr));

MenuDef g_SongInformation(
    "ScreenMiniMenuSongInformation",
    MenuRowDef(
        ScreenEdit::main_title, "Main title", true, EditMode_Practice, true,
        true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::sub_title, "Sub title", true, EditMode_Practice, true, true,
        0, nullptr),
    MenuRowDef(
        ScreenEdit::artist, "Artist", true, EditMode_Practice, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::genre, "Genre", true, EditMode_Practice, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::credit, "Credit", true, EditMode_Practice, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::preview, "Preview", true, EditMode_Full, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::main_title_transliteration, "Main title transliteration",
        true, EditMode_Practice, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::sub_title_transliteration, "Sub title transliteration",
        true, EditMode_Practice, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::artist_transliteration, "Artist transliteration", true,
        EditMode_Practice, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::last_second_hint, "Last second hint", true, EditMode_Full,
        true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::preview_start, "Preview Start", true, EditMode_Full, true,
        true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::preview_length, "Preview Length", true, EditMode_Full, true,
        true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::display_bpm, "Display BPM", true, EditMode_Full, true, true,
        0, "Actual", "Specified", "Random"),
    MenuRowDef(
        ScreenEdit::min_bpm, "Min BPM", true, EditMode_Full, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::max_bpm, "Max BPM", true, EditMode_Full, true, true, 0,
        nullptr));

MenuDef g_TimingDataInformation(
    "ScreenMiniMenuTimingDataInformation",
    MenuRowDef(
        ScreenEdit::beat_0_offset, "Beat 0 Offset", true, EditMode_Full, true,
        true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::bpm, "Edit BPM change", true, EditMode_Full, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::stop, "Edit stop", true, EditMode_Full, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::delay, "Edit delay", true, EditMode_Full, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::label, "Edit label", true, EditMode_Full, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::tickcount, "Edit tickcount", true, EditMode_Full, true,
        true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::combo, "Edit combo", true, EditMode_Full, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::warp, "Edit warp", true, EditMode_Full, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::speed_percent, "Edit speed (percent)", true, EditMode_Full,
        true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::speed_wait, "Edit speed (wait)", true, EditMode_Full, true,
        true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::speed_mode, "Edit speed (mode)", true, EditMode_Full, true,
        true, 0, "Beats", "Seconds"),
    MenuRowDef(
        ScreenEdit::scroll, "Edit scrolling factor", true, EditMode_Full, true,
        true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::fake, "Edit fake", true, EditMode_Full, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::shift_timing_in_region_down, "Shift timing in region down",
        true, EditMode_Full, true, true, RCC_CHOICES),
    MenuRowDef(
        ScreenEdit::shift_timing_in_region_up, "Shift timing in region up",
        true, EditMode_Full, true, true, RCC_CHOICES),
    MenuRowDef(
        ScreenEdit::copy_timing_in_region, "Copy timing in region", true,
        EditMode_Full, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::clear_timing_in_region, "Clear timing in region", true,
        EditMode_Full, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::paste_timing_from_clip, "Paste timing from clipboard", true,
        EditMode_Full, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::copy_full_timing, "Copy timing data", true, EditMode_Full,
        true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::paste_full_timing, "Paste timing data",
        EnabledIfClipboardTimingIsSafe, EditMode_Full, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::erase_step_timing, "Erase step timing", true, EditMode_Full,
        true, true, 0, nullptr));

MenuDef g_TimingDataChangeInformation(
    "ScreenMiniMenuTimingDataChangeInformation",
    MenuRowDef(
        ScreenEdit::timing_all, "All timing", true, EditMode_Full, true, true,
        0, nullptr),
    MenuRowDef(
        ScreenEdit::timing_bpm, "BPM changes", true, EditMode_Full, true, true,
        0, nullptr),
    MenuRowDef(
        ScreenEdit::timing_stop, "Stops", true, EditMode_Full, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::timing_delay, "Delays", true, EditMode_Full, true, true, 0,
        nullptr),
    // Time signatures disabled because they don't fully work. -Kyz
    // MenuRowDef(ScreenEdit::timing_time_sig,
    //  "Time Signatures",
    //  true, EditMode_Full, true, true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::timing_warp, "Warps", true, EditMode_Full, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::timing_label, "Labels", true, EditMode_Full, true, true, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::timing_tickcount, "Tickcount", true, EditMode_Full, true,
        true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::timing_combo, "Combo segments", true, EditMode_Full, true,
        true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::timing_speed, "Speed segments", true, EditMode_Full, true,
        true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::timing_scroll, "Scroll segments", true, EditMode_Full, true,
        true, 0, nullptr),
    MenuRowDef(
        ScreenEdit::timing_fake, "Fakes", true, EditMode_Full, true, true, 0,
        nullptr));

MenuDef g_BackgroundChange(
    "ScreenMiniMenuBackgroundChange",
    MenuRowDef(
        ScreenEdit::layer, "Layer", false, EditMode_Full, true, false, 0, ""),
    MenuRowDef(
        ScreenEdit::rate, "Rate", true, EditMode_Full, true, false, 10, "0%",
        "10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%",
        "120%", "140%", "160%", "180%", "200%", "220%", "240%", "260%", "280%",
        "300%", "350%", "400%"),
    MenuRowDef(
        ScreenEdit::transition, "Force Transition", true, EditMode_Full, true,
        false, 0, nullptr),
    MenuRowDef(
        ScreenEdit::effect, "Force Effect", true, EditMode_Full, true, false, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::color1, "Force Color 1", true, EditMode_Full, true, false,
        0, "-", "#FFFFFF", "#808080", "#FFFFFF80", "#000000", "#FF0000",
        "#00FF00", "#0000FF", "#FFFF00", "#00FFFF", "#FF00FF"),
    MenuRowDef(
        ScreenEdit::color2, "Force Color 2", true, EditMode_Full, true, false,
        0, "-", "#FFFFFF", "#808080", "#FFFFFF80", "#000000", "#FF0000",
        "#00FF00", "#0000FF", "#FFFF00", "#00FFFF", "#FF00FF"),
    MenuRowDef(
        ScreenEdit::file1_type, "File1 Type", true, EditMode_Full, true, true,
        0, "Song BGAnimation", "Song Movie", "Song Bitmap",
        "Global BGAnimation", "Global Movie", "Global Movie from Song Group",
        "Global Movie from Song Group and Genre", "Dynamic Random",
        "Baked Random", "None"),
    MenuRowDef(
        ScreenEdit::file1_song_bganimation, "File1 Song BGAnimation",
        EnabledIfSet1SongBGAnimation, EditMode_Full, true, false, 0, nullptr),
    MenuRowDef(
        ScreenEdit::file1_song_movie, "File1 Song Movie",
        EnabledIfSet1SongMovie, EditMode_Full, true, false, 0, nullptr),
    MenuRowDef(
        ScreenEdit::file1_song_still, "File1 Song Still",
        EnabledIfSet1SongBitmap, EditMode_Full, true, false, 0, nullptr),
    MenuRowDef(
        ScreenEdit::file1_global_bganimation, "File1 Global BGAnimation",
        EnabledIfSet1GlobalBGAnimation, EditMode_Full, true, false, 0, nullptr),
    MenuRowDef(
        ScreenEdit::file1_global_movie, "File1 Global Movie",
        EnabledIfSet1GlobalMovie, EditMode_Full, true, false, 0, nullptr),
    MenuRowDef(
        ScreenEdit::file1_global_movie_song_group, "File1 Global Movie (Group)",
        EnabledIfSet1GlobalMovieSongGroup, EditMode_Full, true, false, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::file1_global_movie_song_group_and_genre,
        "File1 Global Movie (Group + Genre)",
        EnabledIfSet1GlobalMovieSongGroupAndGenre, EditMode_Full, true, false,
        0, nullptr),
    MenuRowDef(
        ScreenEdit::file2_type, "File2 Type", true, EditMode_Full, true, true,
        0, "Song BGAnimation", "Song Movie", "Song Bitmap",
        "Global BGAnimation", "Global Movie", "Global Movie from Song Group",
        "Global Movie from Song Group and Genre", "Dynamic Random",
        "Baked Random", "None"),
    MenuRowDef(
        ScreenEdit::file2_song_bganimation, "File2 Song BGAnimation",
        EnabledIfSet2SongBGAnimation, EditMode_Full, true, false, 0, nullptr),
    MenuRowDef(
        ScreenEdit::file2_song_movie, "File2 Song Movie",
        EnabledIfSet2SongMovie, EditMode_Full, true, false, 0, nullptr),
    MenuRowDef(
        ScreenEdit::file2_song_still, "File2 Song Still",
        EnabledIfSet2SongBitmap, EditMode_Full, true, false, 0, nullptr),
    MenuRowDef(
        ScreenEdit::file2_global_bganimation, "File2 Global BGAnimation",
        EnabledIfSet2GlobalBGAnimation, EditMode_Full, true, false, 0, nullptr),
    MenuRowDef(
        ScreenEdit::file2_global_movie, "File2 Global Movie",
        EnabledIfSet2GlobalMovie, EditMode_Full, true, false, 0, nullptr),
    MenuRowDef(
        ScreenEdit::file2_global_movie_song_group, "File2 Global Movie (Group)",
        EnabledIfSet2GlobalMovieSongGroup, EditMode_Full, true, false, 0,
        nullptr),
    MenuRowDef(
        ScreenEdit::file2_global_movie_song_group_and_genre,
        "File2 Global Movie (Group + Genre)",
        EnabledIfSet2GlobalMovieSongGroupAndGenre, EditMode_Full, true, false,
        0, nullptr),
    MenuRowDef(
        ScreenEdit::delete_change, "Remove Change", true, EditMode_Full, true,
        true, 0, nullptr));

static bool EnabledIfSet1SongBGAnimation() {
  return ScreenMiniMenu::s_viLastAnswers[ScreenEdit::file1_type] ==
             song_bganimation &&
         !g_BackgroundChange.rows[ScreenEdit::file1_song_bganimation]
              .choices.empty();
}
static bool EnabledIfSet1SongMovie() {
  return ScreenMiniMenu::s_viLastAnswers[ScreenEdit::file1_type] ==
             song_movie &&
         !g_BackgroundChange.rows[ScreenEdit::file1_song_movie].choices.empty();
}
static bool EnabledIfSet1SongBitmap() {
  return ScreenMiniMenu::s_viLastAnswers[ScreenEdit::file1_type] ==
             song_bitmap &&
         !g_BackgroundChange.rows[ScreenEdit::file1_song_still].choices.empty();
}
static bool EnabledIfSet1GlobalBGAnimation() {
  return ScreenMiniMenu::s_viLastAnswers[ScreenEdit::file1_type] ==
             global_bganimation &&
         !g_BackgroundChange.rows[ScreenEdit::file1_global_bganimation]
              .choices.empty();
}
static bool EnabledIfSet1GlobalMovie() {
  return ScreenMiniMenu::s_viLastAnswers[ScreenEdit::file1_type] ==
             global_movie &&
         !g_BackgroundChange.rows[ScreenEdit::file1_global_movie]
              .choices.empty();
}
static bool EnabledIfSet1GlobalMovieSongGroup() {
  return ScreenMiniMenu::s_viLastAnswers[ScreenEdit::file1_type] ==
             global_movie_song_group &&
         !g_BackgroundChange.rows[ScreenEdit::file1_global_movie_song_group]
              .choices.empty();
}
static bool EnabledIfSet1GlobalMovieSongGroupAndGenre() {
  return ScreenMiniMenu::s_viLastAnswers[ScreenEdit::file1_type] ==
             global_movie_song_group_and_genre &&
         !g_BackgroundChange
              .rows[ScreenEdit::file1_global_movie_song_group_and_genre]
              .choices.empty();
}
static bool EnabledIfSet2SongBGAnimation() {
  return ScreenMiniMenu::s_viLastAnswers[ScreenEdit::file2_type] ==
             song_bganimation &&
         !g_BackgroundChange.rows[ScreenEdit::file2_song_bganimation]
              .choices.empty();
}
static bool EnabledIfSet2SongMovie() {
  return ScreenMiniMenu::s_viLastAnswers[ScreenEdit::file2_type] ==
             song_movie &&
         !g_BackgroundChange.rows[ScreenEdit::file2_song_movie].choices.empty();
}
static bool EnabledIfSet2SongBitmap() {
  return ScreenMiniMenu::s_viLastAnswers[ScreenEdit::file2_type] ==
             song_bitmap &&
         !g_BackgroundChange.rows[ScreenEdit::file2_song_still].choices.empty();
}
static bool EnabledIfSet2GlobalBGAnimation() {
  return ScreenMiniMenu::s_viLastAnswers[ScreenEdit::file2_type] ==
             global_bganimation &&
         !g_BackgroundChange.rows[ScreenEdit::file2_global_bganimation]
              .choices.empty();
}
static bool EnabledIfSet2GlobalMovie() {
  return ScreenMiniMenu::s_viLastAnswers[ScreenEdit::file2_type] ==
             global_movie &&
         !g_BackgroundChange.rows[ScreenEdit::file2_global_movie]
              .choices.empty();
}
static bool EnabledIfSet2GlobalMovieSongGroup() {
  return ScreenMiniMenu::s_viLastAnswers[ScreenEdit::file2_type] ==
             global_movie_song_group &&
         !g_BackgroundChange.rows[ScreenEdit::file2_global_movie_song_group]
              .choices.empty();
}
static bool EnabledIfSet2GlobalMovieSongGroupAndGenre() {
  return ScreenMiniMenu::s_viLastAnswers[ScreenEdit::file2_type] ==
             global_movie_song_group_and_genre &&
         !g_BackgroundChange
              .rows[ScreenEdit::file2_global_movie_song_group_and_genre]
              .choices.empty();
}

std::string GetOneBakedRandomFile(Song* pSong, bool bTryGenre) {
  std::vector<std::string> vsPaths;
  std::vector<std::string> vsNames;
  BackgroundUtil::GetGlobalRandomMovies(pSong, "", vsPaths, vsNames, bTryGenre);

  return vsNames.empty() ? std::string() : vsNames[RandomInt(vsNames.size())];
}

MenuDef g_InsertTapAttack(
    "ScreenMiniMenuInsertTapAttack",
    MenuRowDef(
        -1, "Duration seconds", true, EditMode_Practice, true, false, 3, "5",
        "10", "15", "20", "25", "30", "35", "40", "45"),  // TODO: Replace
    MenuRowDef(
        -1, "Set modifiers", true, EditMode_Practice, true, true, 0,
        "Press Start"));

MenuDef g_InsertCourseAttack(
    "ScreenMiniMenuInsertCourseAttack",
    MenuRowDef(
        ScreenEdit::duration, "Duration seconds", true, EditMode_Practice, true,
        false, 3, "5", "10", "15", "20", "25", "30", "35", "40", "45"),
    MenuRowDef(
        ScreenEdit::set_mods, "Set modifiers", true, EditMode_Practice, true,
        true, 0, "Press Start"),
    MenuRowDef(
        ScreenEdit::remove, "Remove", true, EditMode_Practice, true, true, 0,
        "Press Start"));

MenuDef g_InsertStepAttack(
    "ScreenMiniMenuInsertCourseAttack",
    MenuRowDef(
        ScreenEdit::sa_duration, "Duration seconds", true, EditMode_Practice,
        true, false, 3, "5", "10", "15", "20", "25", "30", "35", "40", "45"),
    MenuRowDef(
        ScreenEdit::sa_set_mods, "Set modifiers", true, EditMode_Practice, true,
        true, 0, "Press Start"),
    MenuRowDef(
        ScreenEdit::sa_remove, "Remove", true, EditMode_Practice, true, true, 0,
        "Press Start"));

MenuDef g_CourseMode(
    "ScreenMiniMenuCourseDisplay",
    MenuRowDef(
        -1, "Play mods from course", true, EditMode_Practice, true, false, 0,
        nullptr));
