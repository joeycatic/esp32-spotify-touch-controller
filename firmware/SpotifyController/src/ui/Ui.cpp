#include "Ui.h"

#include <algorithm>
#include <cstdio>

#include "../core/RuntimePolicy.h"
#include "AnimationPolicy.h"
#include "BrowseStyle.h"
#include "EventBinding.h"
#include "UiLayout.h"

namespace spotctl {

namespace {
constexpr uint32_t kDimAfterMs = 10U * 60U * 1000U;

const lv_style_transition_dsc_t *buttonTransition(bool pressed) {
  static lv_style_prop_t properties[] = {LV_STYLE_BG_COLOR,
                                         LV_STYLE_TRANSLATE_Y,
                                         LV_STYLE_PROP_INV};
  static lv_style_transition_dsc_t press_transition;
  static lv_style_transition_dsc_t release_transition;
  static bool initialized = false;
  if (!initialized) {
    const ButtonAnimationPlan plan = buttonAnimationPlan();
    lv_style_transition_dsc_init(&press_transition, properties,
                                 lv_anim_path_ease_out, plan.press_ms, 0,
                                 nullptr);
    lv_style_transition_dsc_init(&release_transition, properties,
                                 lv_anim_path_ease_out, plan.release_ms, 0,
                                 nullptr);
    initialized = true;
  }
  return pressed ? &press_transition : &release_transition;
}

void setTranslateX(void *object, int32_t value) {
  lv_obj_set_style_translate_x(static_cast<lv_obj_t *>(object),
                               static_cast<lv_coord_t>(value), 0);
}

Ui *self(lv_event_t *event) {
  return static_cast<Ui *>(lv_event_get_user_data(event));
}

std::string clockText(uint32_t milliseconds) {
  const uint32_t seconds = milliseconds / 1000U;
  char output[12];
  snprintf(output, sizeof(output), "%lu:%02lu",
           static_cast<unsigned long>(seconds / 60U),
           static_cast<unsigned long>(seconds % 60U));
  return output;
}
} // namespace

void Ui::begin(bool provisioning_mode) {
  last_interaction_ms_ = millis();
  provisioning_screen_ = provisioning_mode;
  if (provisioning_mode) {
    showSetup(board_.wide() ? "Connect UART1 to set up"
                            : "Connect USB to set up",
              "Run make provision after creating your Spotify app.");
  } else {
    showSetup("Starting", "Connecting securely to Spotify...");
  }
}

void Ui::clear() {
  lv_obj_clean(lv_scr_act());
  row_art_.clear();
  title_label_ = nullptr;
  subtitle_label_ = nullptr;
  status_label_ = nullptr;
  artwork_image_ = nullptr;
  artwork_placeholder_ = nullptr;
  progress_slider_ = nullptr;
  elapsed_label_ = nullptr;
  displayed_progress_seconds_ = UINT32_MAX;
  duration_label_ = nullptr;
  play_button_label_ = nullptr;
  shuffle_button_ = nullptr;
  repeat_button_ = nullptr;
  list_ = nullptr;
  mini_title_ = nullptr;
  mini_play_label_ = nullptr;
  message_label_ = nullptr;
  connection_badge_ = nullptr;
  calibration_target_ = nullptr;
  calibration_label_ = nullptr;
  factory_reset_button_ = nullptr;
  factory_reset_pressed_ = false;
  factory_reset_started_ms_ = 0;
}

void Ui::applyBaseStyle() {
  lv_obj_t *screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x080A0C), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(screen, lv_color_hex(0xF7F7F7), 0);
  lv_obj_set_style_text_font(
      screen, board_.wide() ? &lv_font_montserrat_18 : &lv_font_montserrat_14,
      0);
  // Nothing on these screens scrolls as a whole, and a scrollable screen
  // consumes horizontal drags as panning instead of emitting a gesture.
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  bindSingleEventHandler(
      [screen, this]() {
        return lv_obj_remove_event_cb_with_user_data(screen, gestureEvent,
                                                     this);
      },
      [screen, this]() {
        lv_obj_add_event_cb(screen, gestureEvent, LV_EVENT_GESTURE, this);
      });

  connection_badge_ = lv_label_create(screen);
  lv_label_set_text(connection_badge_, "OFFLINE");
  lv_obj_set_style_text_font(connection_badge_, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(connection_badge_, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_color(connection_badge_, lv_color_hex(0xB3263E), 0);
  lv_obj_set_style_bg_opa(connection_badge_, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(connection_badge_, 4, 0);
  lv_obj_set_style_pad_ver(connection_badge_, 2, 0);
  lv_obj_set_style_radius(connection_badge_, 4, 0);
  lv_obj_align(connection_badge_, LV_ALIGN_TOP_RIGHT,
               board_.wide() ? -76 : -3, board_.wide() ? 22 : 2);
  if (!offline_) {
    lv_obj_add_flag(connection_badge_, LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_move_foreground(connection_badge_);
}

void Ui::addWideChrome() {
  if (!board_.wide()) {
    return;
  }
  lv_obj_t *top = lv_obj_create(lv_scr_act());
  lv_obj_set_pos(top, 0, 0);
  lv_obj_set_size(top, 1024, 64);
  lv_obj_set_style_bg_color(top, lv_color_hex(0x0E1115), 0);
  lv_obj_set_style_border_width(top, 0, 0);
  lv_obj_set_style_radius(top, 0, 0);
  lv_obj_set_style_pad_all(top, 0, 0);
  lv_obj_clear_flag(top, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *brand = lv_label_create(top);
  lv_label_set_text(brand, "SPOTIFY CONTROLLER");
  lv_obj_set_style_text_color(brand, lv_color_hex(0x1ED760), 0);
  lv_obj_set_style_text_font(brand, &lv_font_montserrat_20, 0);
  lv_obj_align(brand, LV_ALIGN_LEFT_MID, 8, 0);
  lv_obj_t *settings = makeButton(top, LV_SYMBOL_SETTINGS, 952, 0, 64, 64,
                                  settingsEvent);
  lv_obj_set_style_bg_opa(settings, LV_OPA_TRANSP, 0);

  lv_obj_t *nav = lv_obj_create(lv_scr_act());
  lv_obj_set_pos(nav, 0, 528);
  lv_obj_set_size(nav, 1024, 72);
  lv_obj_set_style_bg_color(nav, lv_color_hex(0x0E1115), 0);
  lv_obj_set_style_border_width(nav, 0, 0);
  lv_obj_set_style_radius(nav, 0, 0);
  lv_obj_set_style_pad_all(nav, 0, 0);
  lv_obj_clear_flag(nav, LV_OBJ_FLAG_SCROLLABLE);
  struct NavItem {
    const char *label;
    lv_event_cb_t callback;
  };
  const NavItem items[] = {{"Now Playing", nowPlayingEvent},
                           {"Library", libraryNavEvent},
                           {"Devices", deviceEvent},
                           {"Volume", volumeOpenEvent},
                           {"QR", qrNavEvent}};
  for (size_t i = 0; i < 5; ++i) {
    lv_obj_t *button = makeButton(nav, items[i].label,
                                  static_cast<lv_coord_t>(i * 202 + 4), 2,
                                  194, 64, items[i].callback);
    lv_obj_set_style_radius(button, 12, 0);
    if (i == 1 && (screen_ == Screen::Library || screen_ == Screen::Playlist)) {
      lv_obj_set_style_bg_color(button, lv_color_hex(0x182A22), 0);
      lv_obj_set_style_text_color(button, lv_color_hex(0x1ED760), 0);
    }
  }
  if (connection_badge_ != nullptr) {
    lv_obj_move_foreground(connection_badge_);
  }
}

void Ui::showWidePlayer() {
  clear();
  applyBaseStyle();
  screen_ = Screen::Player;
  addWideChrome();

  status_label_ = lv_label_create(lv_scr_act());
  lv_label_set_text(status_label_, playback_.device.name.empty()
                                      ? "No active device"
                                      : playback_.device.name.c_str());
  lv_obj_set_style_text_color(status_label_, lv_color_hex(0xA6ABB2), 0);
  lv_obj_set_pos(status_label_, 690, 22);
  lv_obj_add_flag(status_label_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(status_label_, deviceEvent, LV_EVENT_CLICKED, this);

  artwork_placeholder_ = lv_obj_create(lv_scr_act());
  lv_obj_set_pos(artwork_placeholder_, 40, 84);
  lv_obj_set_size(artwork_placeholder_, 400, 400);
  lv_obj_set_style_bg_color(artwork_placeholder_, lv_color_hex(0x15191E), 0);
  lv_obj_set_style_border_width(artwork_placeholder_, 0, 0);
  lv_obj_set_style_radius(artwork_placeholder_, 18, 0);
  lv_obj_t *note = lv_label_create(artwork_placeholder_);
  lv_label_set_text(note, LV_SYMBOL_AUDIO);
  lv_obj_set_style_text_color(note, lv_color_hex(0x39414B), 0);
  lv_obj_set_style_text_font(note, &lv_font_montserrat_24, 0);
  lv_obj_center(note);

  artwork_image_ = lv_img_create(lv_scr_act());
  lv_obj_set_pos(artwork_image_, 40, 84);
  lv_obj_set_size(artwork_image_, 400, 400);
  lv_img_set_size_mode(artwork_image_, LV_IMG_SIZE_MODE_REAL);
  lv_img_set_antialias(artwork_image_, true);
  lv_obj_add_flag(artwork_image_, LV_OBJ_FLAG_HIDDEN);

  title_label_ = lv_label_create(lv_scr_act());
  lv_obj_set_pos(title_label_, 500, 105);
  lv_obj_set_width(title_label_, 470);
  lv_label_set_long_mode(title_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_style_text_font(title_label_, &lv_font_montserrat_24, 0);
  subtitle_label_ = lv_label_create(lv_scr_act());
  lv_obj_set_pos(subtitle_label_, 500, 150);
  lv_obj_set_width(subtitle_label_, 470);
  lv_label_set_long_mode(subtitle_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_style_text_color(subtitle_label_, lv_color_hex(0xA6ABB2), 0);
  lv_obj_set_style_text_font(subtitle_label_, &lv_font_montserrat_18, 0);

  progress_slider_ = lv_slider_create(lv_scr_act());
  lv_obj_set_pos(progress_slider_, 500, 215);
  lv_obj_set_size(progress_slider_, 470, 18);
  lv_obj_set_style_bg_color(progress_slider_, lv_color_hex(0x343940), LV_PART_MAIN);
  lv_obj_set_style_bg_color(progress_slider_, lv_color_hex(0x1ED760), LV_PART_INDICATOR);
  lv_obj_add_event_cb(progress_slider_, seekEvent, LV_EVENT_RELEASED, this);
  elapsed_label_ = lv_label_create(lv_scr_act());
  lv_obj_set_pos(elapsed_label_, 500, 242);
  duration_label_ = lv_label_create(lv_scr_act());
  lv_obj_set_pos(duration_label_, 920, 242);
  for (lv_obj_t *label : {elapsed_label_, duration_label_}) {
    lv_obj_set_style_text_color(label, lv_color_hex(0x858B94), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
  }

  shuffle_button_ = makeButton(lv_scr_act(), LV_SYMBOL_SHUFFLE, 500, 305, 72,
                               72, shuffleEvent);
  makeButton(lv_scr_act(), LV_SYMBOL_PREV, 590, 305, 72, 72, previousEvent);
  lv_obj_t *play = makeButton(lv_scr_act(), LV_SYMBOL_PLAY, 680, 293, 96, 96,
                              playEvent);
  play_button_label_ = lv_obj_get_child(play, 0);
  makeButton(lv_scr_act(), LV_SYMBOL_NEXT, 794, 305, 72, 72, nextEvent);
  repeat_button_ = makeButton(lv_scr_act(), LV_SYMBOL_LOOP, 884, 305, 72, 72,
                              repeatEvent);
  updatePlaybackWidgets();
}

void Ui::showWideSettings() {
  clear();
  applyBaseStyle();
  screen_ = Screen::Diagnostics;
  addWideChrome();
  lv_obj_t *heading = lv_label_create(lv_scr_act());
  lv_label_set_text(heading, "Diagnostics & setup");
  lv_obj_set_style_text_font(heading, &lv_font_montserrat_24, 0);
  lv_obj_set_pos(heading, 48, 92);
  const HardwareStatus &hardware = board_.status();
  char detail[180];
  snprintf(detail, sizeof(detail),
           "%s\nDisplay %s   Touch %s   Flash %u MB   PSRAM %u MB",
           boardProfileName(hardware.profile), hardware.display_ready ? "OK" : "FAIL",
           hardware.touch_ready ? "OK" : "FAIL",
           static_cast<unsigned>(hardware.flash_bytes / (1024U * 1024U)),
           static_cast<unsigned>(hardware.psram_bytes / (1024U * 1024U)));
  status_label_ = lv_label_create(lv_scr_act());
  lv_label_set_text(status_label_, detail);
  lv_obj_set_pos(status_label_, 48, 145);
  lv_obj_set_style_text_color(status_label_, lv_color_hex(0xA6ABB2), 0);
  makeButton(lv_scr_act(), "Touch test", 48, 240, 280, 64, diagnosticsEvent);
  factory_reset_button_ = lv_btn_create(lv_scr_act());
  lv_obj_set_pos(factory_reset_button_, 48, 330);
  lv_obj_set_size(factory_reset_button_, 360, 64);
  lv_obj_set_style_radius(factory_reset_button_, 32, 0);
  lv_obj_set_style_bg_color(factory_reset_button_, lv_color_hex(0x7A1D2B), 0);
  lv_obj_t *reset_label = lv_label_create(factory_reset_button_);
  lv_label_set_text(reset_label, "Hold 3s: reset setup");
  lv_obj_center(reset_label);
  lv_obj_add_event_cb(factory_reset_button_, factoryResetEvent, LV_EVENT_ALL,
                      this);
}

lv_obj_t *Ui::makeButton(lv_obj_t *parent, const char *symbol, lv_coord_t x,
                         lv_coord_t y, lv_coord_t width, lv_coord_t height,
                         lv_event_cb_t callback) {
  lv_obj_t *button = lv_btn_create(parent);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, width, height);
  lv_obj_set_style_radius(button, height / 2, 0);
  lv_obj_set_style_bg_color(button, lv_color_hex(0x171A1F), 0);
  lv_obj_set_style_bg_color(button, lv_color_hex(0x1ED760), LV_STATE_PRESSED);
  const ButtonAnimationPlan animation = buttonAnimationPlan();
  lv_obj_set_style_translate_y(button, animation.pressed_translate_y_px,
                               LV_STATE_PRESSED);
  lv_obj_set_style_transition(button, buttonTransition(false), 0);
  lv_obj_set_style_transition(button, buttonTransition(true),
                              LV_STATE_PRESSED);
  lv_obj_set_style_shadow_width(button, 0, 0);
  lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, this);
  lv_obj_t *label = lv_label_create(button);
  lv_label_set_text(label, symbol);
  lv_obj_center(label);
  return button;
}

void Ui::showSetup(const char *title, const char *detail) {
  clear();
  applyBaseStyle();
  screen_ = Screen::Setup;
  lv_obj_t *logo = lv_label_create(lv_scr_act());
  lv_label_set_text(logo, LV_SYMBOL_AUDIO);
  lv_obj_set_style_text_color(logo, lv_color_hex(0x1ED760), 0);
  lv_obj_set_style_text_font(logo, &lv_font_montserrat_24, 0);
  lv_obj_align(logo, LV_ALIGN_CENTER, 0, board_.wide() ? -120 : -70);

  title_label_ = lv_label_create(lv_scr_act());
  lv_label_set_text(title_label_, title);
  lv_obj_set_width(title_label_, board_.wide() ? 760 : 216);
  lv_obj_set_style_text_align(title_label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(title_label_,
                             board_.wide() ? &lv_font_montserrat_24
                                           : &lv_font_montserrat_18,
                             0);
  lv_obj_align(title_label_, LV_ALIGN_CENTER, 0, board_.wide() ? -55 : -20);

  subtitle_label_ = lv_label_create(lv_scr_act());
  lv_label_set_long_mode(subtitle_label_, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(subtitle_label_, board_.wide() ? 760 : 204);
  lv_obj_set_style_text_color(subtitle_label_, lv_color_hex(0xB3B3B3), 0);
  lv_obj_set_style_text_align(subtitle_label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(subtitle_label_, detail);
  lv_obj_align(subtitle_label_, LV_ALIGN_CENTER, 0, board_.wide() ? 10 : 35);

  const HardwareStatus &hardware = board_.status();
  char diagnostics[64];
  snprintf(diagnostics, sizeof(diagnostics), "Display %s  Touch %s\nFlash %u MB  PSRAM %u MB",
           hardware.display_ready ? "OK" : "FAIL",
           hardware.touch_ready ? "OK" : "FAIL",
           static_cast<unsigned>(hardware.flash_bytes / (1024U * 1024U)),
           static_cast<unsigned>(hardware.psram_bytes / (1024U * 1024U)));
  status_label_ = lv_label_create(lv_scr_act());
  lv_label_set_text(status_label_, diagnostics);
  lv_obj_set_style_text_color(status_label_, lv_color_hex(0x6F7680), 0);
  lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(status_label_, LV_ALIGN_BOTTOM_MID, 0, board_.wide() ? -80 : -20);
  lv_obj_add_flag(status_label_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(status_label_, diagnosticsEvent, LV_EVENT_CLICKED, this);
}

void Ui::showConnecting(const char *ssid) {
  std::string detail = "Joining ";
  detail += ssid;
  showSetup("Connecting to Wi-Fi", detail.c_str());
}

void Ui::showOnline() { setOffline(false); }

void Ui::showOffline() {
  setOffline(true);
  showMessage("Offline - reconnecting", true);
}

void Ui::setOffline(bool offline) {
  offline_ = offline;
  if (connection_badge_ != nullptr) {
    if (offline_) {
      lv_obj_clear_flag(connection_badge_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(connection_badge_);
    } else {
      lv_obj_add_flag(connection_badge_, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (screen_ == Screen::Player && status_label_ != nullptr) {
    lv_label_set_text(status_label_,
                      offline_ ? "Offline"
                               : (playback_.device.name.empty()
                                      ? "No active device"
                                      : playback_.device.name.c_str()));
  }
}

void Ui::showFactoryResetCountdown(uint8_t seconds_remaining) {
  char detail[96];
  snprintf(detail, sizeof(detail),
           "Keep holding BOOT to erase saved Wi-Fi and Spotify login.\n%u seconds remaining",
           seconds_remaining);
  showSetup("Factory reset", detail);
}

void Ui::showFactoryResetComplete() {
  provisioning_screen_ = true;
  showSetup("Factory reset complete", "Run make provision to configure the controller again.");
}

void Ui::showSetupSaved() {
  showSetup("Setup saved", "Restarting...");
}

void Ui::showPlayer() {
  if (board_.wide()) {
    showWidePlayer();
    return;
  }
  clear();
  applyBaseStyle();
  screen_ = Screen::Player;

  lv_obj_t *spotify = lv_label_create(lv_scr_act());
  lv_label_set_text(spotify, "SPOTIFY");
  lv_obj_set_style_text_color(spotify, lv_color_hex(0x1ED760), 0);
  lv_obj_set_style_text_font(spotify, &lv_font_montserrat_12, 0);
  lv_obj_set_pos(spotify, 10, 4);
  lv_obj_add_flag(spotify, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(spotify, spotifyLinkEvent, LV_EVENT_CLICKED, this);

  status_label_ = lv_label_create(lv_scr_act());
  lv_label_set_text(status_label_, playback_.device.name.empty()
                                      ? "No active device"
                                      : playback_.device.name.c_str());
  lv_obj_set_style_text_color(status_label_, lv_color_hex(0x858B94), 0);
  lv_obj_set_style_text_font(status_label_, &lv_font_montserrat_12, 0);
  lv_obj_align(status_label_, LV_ALIGN_TOP_RIGHT, -10, 4);
  lv_obj_add_flag(status_label_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(status_label_, deviceEvent, LV_EVENT_CLICKED, this);

  artwork_placeholder_ = lv_obj_create(lv_scr_act());
  lv_obj_set_pos(artwork_placeholder_, 28, 20);
  lv_obj_set_size(artwork_placeholder_, 184, 184);
  lv_obj_set_style_bg_color(artwork_placeholder_, lv_color_hex(0x15191E), 0);
  lv_obj_set_style_border_width(artwork_placeholder_, 0, 0);
  lv_obj_set_style_radius(artwork_placeholder_, 8, 0);
  lv_obj_t *note = lv_label_create(artwork_placeholder_);
  lv_label_set_text(note, LV_SYMBOL_AUDIO);
  lv_obj_set_style_text_color(note, lv_color_hex(0x39414B), 0);
  lv_obj_set_style_text_font(note, &lv_font_montserrat_24, 0);
  lv_obj_center(note);

  artwork_image_ = lv_img_create(lv_scr_act());
  lv_obj_set_pos(artwork_image_, 28, 20);
  lv_obj_set_size(artwork_image_, 184, 184);
  // The object size is the size after zoom, so a larger cover scales into
  // this box instead of the box itself being scaled down.
  lv_img_set_size_mode(artwork_image_, LV_IMG_SIZE_MODE_REAL);
  lv_obj_add_flag(artwork_image_, LV_OBJ_FLAG_HIDDEN);
  lv_img_set_antialias(artwork_image_, true);

  title_label_ = lv_label_create(lv_scr_act());
  lv_obj_set_pos(title_label_, 12, 207);
  lv_obj_set_width(title_label_, 216);
  lv_label_set_long_mode(title_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_style_text_font(title_label_, &lv_font_montserrat_16, 0);

  subtitle_label_ = lv_label_create(lv_scr_act());
  lv_obj_set_pos(subtitle_label_, 12, 228);
  lv_obj_set_width(subtitle_label_, 216);
  lv_label_set_long_mode(subtitle_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_style_text_color(subtitle_label_, lv_color_hex(0xA6ABB2), 0);
  lv_obj_set_style_text_font(subtitle_label_, &lv_font_montserrat_12, 0);

  progress_slider_ = lv_slider_create(lv_scr_act());
  lv_obj_set_pos(progress_slider_, 12, 247);
  lv_obj_set_size(progress_slider_, 216, 10);
  lv_obj_set_style_bg_color(progress_slider_, lv_color_hex(0x343940),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_color(progress_slider_, lv_color_hex(0x1ED760),
                            LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(progress_slider_, LV_OPA_TRANSP, LV_PART_KNOB);
  lv_obj_add_event_cb(progress_slider_, seekEvent, LV_EVENT_RELEASED, this);

  elapsed_label_ = lv_label_create(lv_scr_act());
  lv_obj_set_pos(elapsed_label_, 12, 257);
  duration_label_ = lv_label_create(lv_scr_act());
  lv_obj_align(duration_label_, LV_ALIGN_TOP_RIGHT, -12, 257);
  for (lv_obj_t *time_label : {elapsed_label_, duration_label_}) {
    lv_obj_set_style_text_color(time_label, lv_color_hex(0x747B84), 0);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_12, 0);
  }

  shuffle_button_ = makeButton(lv_scr_act(), LV_SYMBOL_SHUFFLE, 2, 275, 36, 40,
                               shuffleEvent);
  makeButton(lv_scr_act(), LV_SYMBOL_PREV, 40, 275, 40, 40, previousEvent);
  lv_obj_t *play = makeButton(lv_scr_act(), LV_SYMBOL_PLAY, 82, 270, 48, 48,
                              playEvent);
  play_button_label_ = lv_obj_get_child(play, 0);
  makeButton(lv_scr_act(), LV_SYMBOL_NEXT, 132, 275, 40, 40, nextEvent);
  repeat_button_ = makeButton(lv_scr_act(), LV_SYMBOL_LOOP, 174, 275, 36, 40,
                              repeatEvent);
  makeButton(lv_scr_act(), LV_SYMBOL_VOLUME_MAX, 212, 275, 26, 40,
             volumeOpenEvent);

  updatePlaybackWidgets();
}

void Ui::showLibrary(bool request_data) {
  clear();
  applyBaseStyle();
  screen_ = Screen::Library;
  if (board_.wide()) {
    addWideChrome();
  } else {
    makeButton(lv_scr_act(), LV_SYMBOL_LEFT, 4, 4, 38, 34, backEvent);
  }
  lv_obj_t *heading = lv_label_create(lv_scr_act());
  lv_label_set_text(heading, "Your Library");
  lv_obj_set_style_text_font(heading, board_.wide() ? &lv_font_montserrat_24
                                                   : &lv_font_montserrat_18, 0);
  lv_obj_set_pos(heading, board_.wide() ? 44 : 52, board_.wide() ? 86 : 11);
  if (board_.wide()) {
    lv_obj_t *detail = lv_label_create(lv_scr_act());
    lv_label_set_text(detail, "Find your next listen");
    lv_obj_set_pos(detail, 44, 120);
    lv_obj_set_style_text_font(detail, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(detail, lv_color_hex(0xA6ABB2), 0);
    lv_obj_t *liked = makeButton(lv_scr_act(), LV_SYMBOL_AUDIO "   Liked Songs",
                                 736, 80, 248, 64, likedEvent);
    lv_obj_set_style_radius(liked, 16, 0);
    lv_obj_set_style_bg_color(liked, lv_color_hex(0x182A22), 0);
    lv_obj_set_style_text_color(liked, lv_color_hex(0x1ED760), 0);
  }
  list_ = lv_list_create(lv_scr_act());
  lv_obj_set_pos(list_, board_.wide() ? 40 : 4, board_.wide() ? 160 : 44);
  lv_obj_set_size(list_, board_.wide() ? 944 : 232,
                  board_.wide() ? 352 : 222);
  styleBrowseList(list_, board_.wide(), true);
  lv_obj_add_event_cb(list_, listScrollEvent, LV_EVENT_SCROLL_END, this);
  rebuildPlaylistRows();
  updateMiniPlayer();
  if (request_data) {
    requestPlaylists();
  }
}

void Ui::requestPlaylists() {
  if (!playlist_load_state_.shouldRequest(playlists_.size())) {
    return;
  }
  if (send(UiCommand{UiCommandType::LoadPlaylists})) {
    playlist_load_state_.markRequested();
  }
}

namespace {
constexpr size_t kMaximumRowThumbnails = 24;
} // namespace

std::string Ui::rowKey(size_t encoded) const {
  if (encoded == 0) {
    return {};
  }
  const size_t index = encoded - 1;
  if (screen_ == Screen::Library) {
    return index < playlists_.size() ? playlists_[index].id : std::string();
  }
  if (screen_ == Screen::Playlist) {
    return index < tracks_.size() ? tracks_[index].uri : std::string();
  }
  return {};
}

lv_obj_t *Ui::rowForKey(const std::string &key) const {
  if (list_ == nullptr || key.empty()) {
    return nullptr;
  }
  const uint32_t children = lv_obj_get_child_cnt(list_);
  for (uint32_t child = 0; child < children; ++child) {
    lv_obj_t *row = lv_obj_get_child(list_, child);
    if (row == nullptr) {
      continue;
    }
    const size_t encoded =
        reinterpret_cast<size_t>(lv_obj_get_user_data(row));
    if (rowKey(encoded) == key) {
      return row;
    }
  }
  return nullptr;
}

void Ui::setRowThumbnail(lv_obj_t *row, const ArtworkHandle &frame) {
  if (row == nullptr || !frame || frame->image.header.w == 0) {
    return;
  }
  lv_obj_t *icon = lv_obj_get_child(row, 0);
  if (icon == nullptr || !lv_obj_check_type(icon, &lv_img_class)) {
    return;
  }
  lv_img_set_src(icon, &frame->image);
  lv_img_set_antialias(icon, true);
  // Decoded at row scale already, so it is drawn 1:1 with no transform: no
  // pivot or zoom to get wrong, and nothing to rescale on every redraw.
  lv_obj_set_size(icon, static_cast<lv_coord_t>(frame->image.header.w),
                  static_cast<lv_coord_t>(frame->image.header.h));
}

void Ui::resetRowIcon(lv_obj_t *row) {
  if (row == nullptr) {
    return;
  }
  lv_obj_t *icon = lv_obj_get_child(row, 0);
  if (icon == nullptr || !lv_obj_check_type(icon, &lv_img_class)) {
    return;
  }
  lv_img_set_src(icon, screen_ == Screen::Library ? LV_SYMBOL_AUDIO
                                                  : LV_SYMBOL_PLAY);
  lv_img_set_zoom(icon, 256);
  lv_obj_set_size(icon, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
}

const ArtworkHandle *Ui::storedThumbnail(const std::string &key) const {
  for (const auto &entry : row_art_) {
    if (entry.first == key) {
      return &entry.second;
    }
  }
  return nullptr;
}

void Ui::storeThumbnail(const std::string &key, const ArtworkHandle &frame) {
  for (auto &entry : row_art_) {
    if (entry.first == key) {
      entry.second = frame;
      return;
    }
  }
  while (row_art_.size() >= kMaximumRowThumbnails) {
    // Detach the image before the pixels it points at are released.
    resetRowIcon(rowForKey(row_art_.front().first));
    row_art_.pop_front();
  }
  row_art_.emplace_back(key, frame);
}

void Ui::applyThumbnail(const std::string &key, const ArtworkHandle &frame) {
  if (key.empty() || !frame) {
    return;
  }
  storeThumbnail(key, frame);
  setRowThumbnail(rowForKey(key), frame);
}

void Ui::applyStoredThumbnails() {
  if (list_ == nullptr) {
    return;
  }
  const uint32_t children = lv_obj_get_child_cnt(list_);
  for (uint32_t child = 0; child < children; ++child) {
    lv_obj_t *row = lv_obj_get_child(list_, child);
    if (row == nullptr) {
      continue;
    }
    const std::string key =
        rowKey(reinterpret_cast<size_t>(lv_obj_get_user_data(row)));
    if (key.empty()) {
      continue;
    }
    if (const ArtworkHandle *frame = storedThumbnail(key)) {
      setRowThumbnail(row, *frame);
    }
  }
}

void Ui::requestVisibleThumbnails() {
  if (list_ == nullptr ||
      (screen_ != Screen::Library && screen_ != Screen::Playlist)) {
    return;
  }
  // Coordinates are only meaningful once the layout has been recalculated.
  lv_obj_update_layout(list_);
  lv_area_t viewport;
  lv_obj_get_coords(list_, &viewport);

  // Whatever was queued belonged to the previous window; those rows may have
  // scrolled away, and fetching them would delay the ones now on screen.
  network_.resetThumbnailRequests();
  const uint32_t children = lv_obj_get_child_cnt(list_);
  for (uint32_t child = 0; child < children; ++child) {
    lv_obj_t *row = lv_obj_get_child(list_, child);
    if (row == nullptr) {
      continue;
    }
    const size_t encoded =
        reinterpret_cast<size_t>(lv_obj_get_user_data(row));
    const std::string key = rowKey(encoded);
    if (key.empty() || storedThumbnail(key) != nullptr) {
      continue;
    }
    lv_area_t area;
    lv_obj_get_coords(row, &area);
    if (!rowIntersectsViewport(area.y1, area.y2, viewport.y1, viewport.y2)) {
      continue;
    }
    const size_t index = encoded - 1;
    const std::string url =
        screen_ == Screen::Library
            ? (index < playlists_.size() ? playlists_[index].thumbnail_url
                                         : std::string())
            : (index < tracks_.size() ? tracks_[index].thumbnail_url
                                      : std::string());
    if (url.empty()) {
      continue;
    }
    network_.requestThumbnail(key, url);
  }
}

void Ui::rebuildPlaylistRows() {
  if (list_ == nullptr) {
    return;
  }
  const lv_coord_t scroll_y = lv_obj_get_scroll_y(list_);
  lv_obj_clean(list_);
  if (!board_.wide()) {
    lv_obj_t *liked = lv_list_add_btn(list_, LV_SYMBOL_AUDIO, "Liked Songs");
    styleBrowseRow(liked, true);
    lv_obj_set_height(liked, 52);
    lv_obj_add_event_cb(liked, likedEvent, LV_EVENT_CLICKED, this);
  }
  for (size_t index = 0; index < playlists_.size(); ++index) {
    const PlaylistSummary &playlist = playlists_[index];
    std::string label = playlist.name;
    if (!playlist.owner.empty()) {
      label += "\nby " + playlist.owner;
    }
    if (!playlist.items_browsable) {
      label += "  (play only)";
    }
    lv_obj_t *row;
    if (board_.wide()) {
      std::string detail = playlist.owner.empty() ? "Playlist"
                                                 : "by " + playlist.owner;
      if (!playlist.items_browsable) {
        detail = "Play only / " + detail;
      }
      row = makeWideBrowseRow(list_, playlist.name.c_str(), detail.c_str(),
                              playlist.items_browsable ? LV_SYMBOL_RIGHT
                                                       : LV_SYMBOL_PLAY,
                              true);
    } else {
      row = lv_list_add_btn(list_, LV_SYMBOL_AUDIO, label.c_str());
      lv_obj_set_height(row, 52);
      styleBrowseRow(row);
    }
    lv_obj_set_user_data(row, reinterpret_cast<void *>(index + 1));
    lv_obj_add_event_cb(row, playlistEvent, LV_EVENT_CLICKED, this);
  }
  lv_obj_scroll_to_y(list_, scroll_y, LV_ANIM_OFF);
  applyStoredThumbnails();
  if (board_.capabilities().media.row_thumbnails) {
    requestVisibleThumbnails();
  }
}

void Ui::showTracks(const std::string &title) {
  clear();
  applyBaseStyle();
  screen_ = Screen::Playlist;
  if (board_.wide()) {
    addWideChrome();
    makeButton(lv_scr_act(), LV_SYMBOL_LEFT, 40, 76, 64, 64, backEvent);
  } else {
    makeButton(lv_scr_act(), LV_SYMBOL_LEFT, 4, 4, 38, 34, backEvent);
  }
  lv_obj_t *heading = lv_label_create(lv_scr_act());
  lv_obj_set_pos(heading, board_.wide() ? 124 : 50,
                 board_.wide() ? 94 : 9);
  lv_obj_set_width(heading, board_.wide() ? 820 : 180);
  lv_label_set_long_mode(heading, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_style_text_font(heading, board_.wide() ? &lv_font_montserrat_24
                                                   : &lv_font_montserrat_18, 0);
  lv_label_set_text(heading, title.c_str());
  list_ = lv_list_create(lv_scr_act());
  lv_obj_set_pos(list_, board_.wide() ? 40 : 4,
                 board_.wide() ? 152 : 44);
  lv_obj_set_size(list_, board_.wide() ? 944 : 232,
                  board_.wide() ? 360 : 222);
  styleBrowseList(list_, board_.wide(), false);
  lv_obj_add_event_cb(list_, listScrollEvent, LV_EVENT_SCROLL_END, this);
  rebuildTrackRows();
  updateMiniPlayer();
}

void Ui::showPlayOnly(const PlaylistSummary &playlist) {
  play_only_playlist_ = playlist;
  clear();
  applyBaseStyle();
  screen_ = Screen::Playlist;
  if (board_.wide()) {
    addWideChrome();
    makeButton(lv_scr_act(), LV_SYMBOL_LEFT, 40, 76, 64, 64, backEvent);
  } else {
    makeButton(lv_scr_act(), LV_SYMBOL_LEFT, 4, 4, 38, 34, backEvent);
  }
  title_label_ = lv_label_create(lv_scr_act());
  lv_obj_set_width(title_label_, board_.wide() ? 700 : 210);
  lv_obj_set_style_text_align(title_label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(title_label_, &lv_font_montserrat_20, 0);
  lv_label_set_text(title_label_, playlist.name.c_str());
  lv_obj_align(title_label_, LV_ALIGN_CENTER, 0, board_.wide() ? -90 : -55);
  subtitle_label_ = lv_label_create(lv_scr_act());
  lv_obj_set_width(subtitle_label_, board_.wide() ? 700 : 205);
  lv_label_set_long_mode(subtitle_label_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(subtitle_label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(subtitle_label_, lv_color_hex(0xA6ABB2), 0);
  lv_label_set_text(subtitle_label_,
                    "Spotify allows this playlist to be started, but its song list is hidden in development mode.");
  lv_obj_align(subtitle_label_, LV_ALIGN_CENTER, 0, 5);
  lv_obj_t *play = makeButton(lv_scr_act(), LV_SYMBOL_PLAY,
                              board_.wide() ? 432 : 65,
                              board_.wide() ? 350 : 210,
                              board_.wide() ? 160 : 110,
                              board_.wide() ? 72 : 48,
                              playPlaylistEvent);
  lv_obj_set_style_bg_color(play, lv_color_hex(0x1ED760), 0);
  updateMiniPlayer();
}

void Ui::rebuildTrackRows() {
  if (list_ == nullptr) {
    return;
  }
  const lv_coord_t scroll_y = lv_obj_get_scroll_y(list_);
  lv_obj_clean(list_);
  for (size_t index = 0; index < tracks_.size(); ++index) {
    const TrackSummary &track = tracks_[index];
    std::string label = playback_.item.uri == track.uri ? "• " : "";
    label += track.title;
    if (!track.artists.empty()) {
      label += "\n" + track.artists;
    }
    if (track.duration_ms > 0) {
      label += "  ·  " + clockText(track.duration_ms);
    }
    lv_obj_t *row;
    if (board_.wide()) {
      const std::string duration = track.duration_ms > 0
                                       ? clockText(track.duration_ms)
                                       : "";
      row = makeWideBrowseRow(list_, track.title.c_str(), track.artists.c_str(),
                              duration.c_str(), false,
                              playback_.item.uri == track.uri);
    } else {
      row = lv_list_add_btn(list_, LV_SYMBOL_PLAY, label.c_str());
      lv_obj_set_height(row, 54);
      styleBrowseRow(row, playback_.item.uri == track.uri);
    }
    lv_obj_set_user_data(row, reinterpret_cast<void *>(index + 1));
    lv_obj_add_event_cb(row, trackEvent, LV_EVENT_CLICKED, this);
  }
  lv_obj_scroll_to_y(list_, scroll_y, LV_ANIM_OFF);
  applyStoredThumbnails();
  if (board_.capabilities().media.row_thumbnails) {
    requestVisibleThumbnails();
  }
}

void Ui::showDevices() {
  clear();
  applyBaseStyle();
  screen_ = Screen::Devices;
  if (board_.wide()) {
    addWideChrome();
  } else {
    makeButton(lv_scr_act(), LV_SYMBOL_LEFT, 4, 4, 38, 34, backEvent);
  }
  lv_obj_t *heading = lv_label_create(lv_scr_act());
  lv_label_set_text(heading, "Play on");
  lv_obj_set_style_text_font(heading, &lv_font_montserrat_18, 0);
  lv_obj_set_pos(heading, board_.wide() ? 48 : 52,
                 board_.wide() ? 82 : 11);
  list_ = lv_list_create(lv_scr_act());
  lv_obj_set_pos(list_, board_.wide() ? 40 : 4,
                 board_.wide() ? 125 : 48);
  lv_obj_set_size(list_, board_.wide() ? 944 : 232,
                  board_.wide() ? 387 : 264);
  lv_obj_set_style_bg_color(list_, lv_color_hex(0x080A0C), 0);
  lv_obj_set_style_border_width(list_, 0, 0);
  rebuildDeviceRows();
}

void Ui::showVolumeOverlay() {
  lv_obj_t *panel = lv_obj_create(lv_scr_act());
  lv_obj_set_size(panel, board_.wide() ? 560 : 220,
                  board_.wide() ? 190 : 112);
  lv_obj_align(panel, LV_ALIGN_CENTER, 0, board_.wide() ? 0 : 45);
  lv_obj_set_style_bg_color(panel, lv_color_hex(0x171A1F), 0);
  lv_obj_set_style_border_color(panel, lv_color_hex(0x31363E), 0);
  lv_obj_set_style_radius(panel, 12, 0);
  lv_obj_move_foreground(panel);

  lv_obj_t *heading = lv_label_create(panel);
  lv_label_set_text(heading, "Volume");
  lv_obj_set_style_text_font(heading, &lv_font_montserrat_16, 0);
  lv_obj_set_pos(heading, 6, 3);

  lv_obj_t *close = lv_btn_create(panel);
  lv_obj_set_size(close, board_.wide() ? 64 : 34,
                  board_.wide() ? 64 : 30);
  lv_obj_align(close, LV_ALIGN_TOP_RIGHT, 7, -7);
  lv_obj_set_style_bg_opa(close, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_width(close, 0, 0);
  lv_obj_add_event_cb(close, volumeCloseEvent, LV_EVENT_CLICKED, this);
  lv_obj_t *close_label = lv_label_create(close);
  lv_label_set_text(close_label, LV_SYMBOL_CLOSE);
  lv_obj_center(close_label);

  lv_obj_t *slider = lv_slider_create(panel);
  lv_obj_set_pos(slider, board_.wide() ? 20 : 8,
                 board_.wide() ? 102 : 55);
  lv_obj_set_size(slider, board_.wide() ? 480 : 176,
                  board_.wide() ? 24 : 16);
  lv_slider_set_range(slider, 0, 100);
  lv_slider_set_value(slider, std::max(0, playback_.volume_percent), LV_ANIM_OFF);
  lv_obj_set_style_bg_color(slider, lv_color_hex(0x1ED760), LV_PART_INDICATOR);
  lv_obj_add_event_cb(slider, volumeEvent, LV_EVENT_RELEASED, this);
}

void Ui::rebuildDeviceRows() {
  if (list_ == nullptr) {
    return;
  }
  lv_obj_clean(list_);
  if (devices_.empty()) {
    lv_list_add_text(list_, "Open Spotify on a phone or computer");
  }
  for (size_t index = 0; index < devices_.size(); ++index) {
    std::string label = devices_[index].name;
    if (devices_[index].active) {
      label += "  active";
    }
    if (devices_[index].restricted) {
      label += "  restricted";
    }
    lv_obj_t *row = lv_list_add_btn(list_, LV_SYMBOL_VOLUME_MAX, label.c_str());
    lv_obj_set_user_data(row, reinterpret_cast<void *>(index + 1));
    lv_obj_add_event_cb(row, deviceRowEvent, LV_EVENT_CLICKED, this);
  }
}

void Ui::showQrCode() {
  if (playback_.item.spotify_url.empty()) {
    showMessage("No Spotify link for this item", true);
    return;
  }
  clear();
  applyBaseStyle();
  screen_ = Screen::QrCode;
  if (board_.wide()) {
    addWideChrome();
  } else {
    makeButton(lv_scr_act(), LV_SYMBOL_LEFT, 4, 4, 38, 34, backEvent);
  }
  lv_obj_t *heading = lv_label_create(lv_scr_act());
  lv_label_set_text(heading, "Open in Spotify");
  lv_obj_set_style_text_font(heading, &lv_font_montserrat_18, 0);
  lv_obj_align(heading, LV_ALIGN_TOP_MID, 0, board_.wide() ? 78 : 12);
  lv_obj_t *qr = lv_qrcode_create(lv_scr_act(), board_.wide() ? 360 : 204,
                                  lv_color_hex(0x000000),
                                  lv_color_hex(0xFFFFFF));
  lv_qrcode_update(qr, playback_.item.spotify_url.data(),
                   playback_.item.spotify_url.size());
  lv_obj_align(qr, LV_ALIGN_CENTER, 0, board_.wide() ? 10 : 14);
  lv_obj_t *caption = lv_label_create(lv_scr_act());
  lv_label_set_text(caption, "Scan with your phone");
  lv_obj_set_style_text_color(caption, lv_color_hex(0xA6ABB2), 0);
  lv_obj_align(caption, LV_ALIGN_BOTTOM_MID, 0,
               board_.wide() ? -82 : -10);
}

void Ui::showTouchCalibration() {
  clear();
  applyBaseStyle();
  screen_ = Screen::Diagnostics;
  calibration_step_ = 0;
  if (board_.wide()) {
    addWideChrome();
    makeButton(lv_scr_act(), LV_SYMBOL_LEFT, 40, 72, 64, 64, backEvent);
  } else {
    makeButton(lv_scr_act(), LV_SYMBOL_LEFT, 4, 4, 38, 34, backEvent);
  }
  calibration_label_ = lv_label_create(lv_scr_act());
  lv_obj_set_width(calibration_label_, board_.wide() ? 620 : 170);
  lv_obj_set_style_text_align(calibration_label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(calibration_label_, "Touch each green corner target");
  lv_obj_align(calibration_label_, LV_ALIGN_CENTER, 0, 0);
  calibration_target_ = lv_btn_create(lv_scr_act());
  const lv_coord_t target = board_.wide() ? 64 : 44;
  lv_obj_set_size(calibration_target_, target, target);
  lv_obj_set_pos(calibration_target_, 0, board_.wide() ? 64 : 42);
  lv_obj_set_style_radius(calibration_target_, target / 2, 0);
  lv_obj_set_style_bg_color(calibration_target_, lv_color_hex(0x1ED760), 0);
  lv_obj_add_event_cb(calibration_target_, calibrationEvent, LV_EVENT_CLICKED, this);
}

void Ui::updatePlaybackWidgets() {
  if (screen_ == Screen::Player) {
    lv_label_set_text(title_label_,
                      playback_.has_item ? playback_.item.title.c_str()
                                         : "Nothing playing");
    lv_label_set_text(subtitle_label_,
                      playback_.has_item ? playback_.item.subtitle.c_str()
                                         : "Start Spotify on another device");
    lv_label_set_text(status_label_,
                      offline_ ? "Offline"
                               : (playback_.device.name.empty()
                                      ? "No active device"
                                      : playback_.device.name.c_str()));
    lv_label_set_text(play_button_label_,
                      playback_.is_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    lv_obj_set_style_text_color(shuffle_button_,
                                playback_.shuffle ? lv_color_hex(0x1ED760)
                                                  : lv_color_hex(0xF7F7F7),
                                0);
    lv_obj_set_style_text_color(repeat_button_,
                                playback_.repeat != RepeatMode::Off
                                    ? lv_color_hex(0x1ED760)
                                    : lv_color_hex(0xF7F7F7),
                                0);
    const uint32_t maximum = std::max<uint32_t>(1, playback_.item.duration_ms / 1000U);
    lv_slider_set_range(progress_slider_, 0, static_cast<int32_t>(maximum));
    lv_label_set_text(duration_label_, clockText(playback_.item.duration_ms).c_str());
    if (artwork_) {
      lv_img_set_src(artwork_image_, &artwork_->image);
      const lv_coord_t source_width =
          static_cast<lv_coord_t>(artwork_->image.header.w);
      if (source_width > 0) {
        // REAL size mode centres the zoomed cover in the box, so the default
        // centre pivot is the correct one here.
        lv_img_set_zoom(
            artwork_image_,
            static_cast<uint16_t>(board_.capabilities().media.player_art_size *
                                  256 / source_width));
      }
      lv_obj_clear_flag(artwork_image_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(artwork_placeholder_, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(artwork_image_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(artwork_placeholder_, LV_OBJ_FLAG_HIDDEN);
    }
  }
  updateMiniPlayer();
}

void Ui::updateMiniPlayer() {
  if (board_.wide()) {
    return;
  }
  if (screen_ != Screen::Library && screen_ != Screen::Playlist) {
    return;
  }
  if (mini_title_ == nullptr) {
    lv_obj_t *bar = lv_obj_create(lv_scr_act());
    lv_obj_set_pos(bar, 4, 268);
    lv_obj_set_size(bar, 232, 48);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x171A1F), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 8, 0);
    mini_title_ = lv_label_create(bar);
    lv_obj_set_pos(mini_title_, 8, 7);
    lv_obj_set_width(mini_title_, 168);
    lv_label_set_long_mode(mini_title_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_t *play = makeButton(bar, LV_SYMBOL_PLAY, 182, 2, 42, 42, playEvent);
    mini_play_label_ = lv_obj_get_child(play, 0);
  }
  if (mini_title_ != nullptr) {
    lv_label_set_text(mini_title_, playback_.has_item
                                       ? playback_.item.title.c_str()
                                       : "Nothing playing");
    lv_label_set_text(mini_play_label_,
                      playback_.is_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
  }
}

void Ui::animatePlayerSwipe(SwipeDirection direction) {
  if (screen_ != Screen::Player) {
    return;
  }
  const SwipeAnimationPlan plan = swipeAnimationPlan(direction);
  for (lv_obj_t *object : {artwork_image_, artwork_placeholder_, title_label_,
                           subtitle_label_}) {
    if (object == nullptr) {
      continue;
    }
    lv_anim_del(object, setTranslateX);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, object);
    lv_anim_set_exec_cb(&animation, setTranslateX);
    lv_anim_set_values(&animation, 0, plan.offset_px);
    lv_anim_set_time(&animation, plan.outward_ms);
    lv_anim_set_playback_time(&animation, plan.return_ms);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_start(&animation);
  }
}

void Ui::showMessage(const std::string &message, bool error) {
  destroyMessage();
  message_label_ = lv_label_create(lv_scr_act());
  lv_obj_set_width(message_label_, board_.wide() ? 640 : 220);
  lv_label_set_long_mode(message_label_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(message_label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_bg_opa(message_label_, LV_OPA_90, 0);
  lv_obj_set_style_bg_color(message_label_,
                            error ? lv_color_hex(0x7A1D2B)
                                  : lv_color_hex(0x1B4930),
                            0);
  lv_obj_set_style_pad_all(message_label_, 8, 0);
  lv_obj_set_style_radius(message_label_, 8, 0);
  lv_label_set_text(message_label_, message.c_str());
  lv_obj_align(message_label_, LV_ALIGN_TOP_MID, 0, 26);
  lv_obj_move_foreground(message_label_);
  message_until_ms_ = millis() + 4000;
}

void Ui::destroyMessage() {
  if (message_label_ != nullptr) {
    lv_obj_del(message_label_);
    message_label_ = nullptr;
  }
}

void Ui::handle(const NetworkEvent &event) {
  switch (event.type) {
  case NetworkEventType::Authorized:
    showMessage("Spotify connected");
    // Warm the first page while the player is already usable. The next swipe
    // can render the cached rows immediately instead of waiting on the API.
    requestPlaylists();
    break;
  case NetworkEventType::AuthorizationRequired:
    provisioning_screen_ = true;
    showSetup("Spotify login required",
              board_.wide() ? "Reconnect UART1 and run make provision."
                            : "Reconnect USB and run make provision.");
    break;
  case NetworkEventType::Playback:
    {
      const bool wake_for_new_playback =
          playback_.item.uri != event.playback.item.uri ||
          (!playback_.is_playing && event.playback.is_playing);
      ArtworkHandle previous_artwork;
      if (playback_.item.uri != event.playback.item.uri || event.artwork) {
        previous_artwork = artwork_;
        artwork_ = event.artwork;
      }
      playback_ = event.playback;
      if (screen_ == Screen::Setup || screen_ == Screen::Diagnostics) {
        showPlayer();
      }
      updatePlaybackWidgets();
      if (wake_for_new_playback) {
        noteInteraction();
      }
      // Keep the old PSRAM buffer alive until LVGL has been pointed at the
      // replacement (or the image widget has been hidden/deleted).
      previous_artwork.reset();
    }
    break;
  case NetworkEventType::Playlists:
    playlist_load_state_.markLoaded();
    if (event.replace) {
      playlists_.clear();
    }
    playlists_.insert(playlists_.end(), event.playlists.begin(),
                      event.playlists.end());
    if (playlists_.size() > 60) {
      playlists_.erase(playlists_.begin(), playlists_.end() - 60);
    }
    playlists_have_more_ = event.has_more;
    if (screen_ == Screen::Library) {
      rebuildPlaylistRows();
    }
    break;
  case NetworkEventType::Tracks:
    if (event.replace) {
      tracks_.clear();
      tracks_are_liked_ = event.liked;
      track_list_title_ = event.title;
    }
    tracks_.insert(tracks_.end(), event.tracks.begin(), event.tracks.end());
    if (tracks_.size() > 60) {
      tracks_.erase(tracks_.begin(), tracks_.end() - 60);
    }
    tracks_have_more_ = event.has_more;
    if (event.replace) {
      showTracks(track_list_title_);
    } else if (screen_ == Screen::Playlist) {
      rebuildTrackRows();
    }
    break;
  case NetworkEventType::Devices:
    devices_ = event.devices;
    showDevices();
    break;
  case NetworkEventType::Artwork:
    applyThumbnail(event.key, event.artwork);
    break;
  case NetworkEventType::Error: {
    playlist_load_state_.markFailed();
    std::string message = event.error.user_message;
    if (event.error.category == ErrorCategory::RateLimited &&
        event.error.retry_after_ms > 0) {
      message += " (retry in " +
                 std::to_string(event.error.retry_after_ms / 1000U) + "s)";
    }
    showMessage(message, true);
    if (event.error.category == ErrorCategory::NoDevice) {
      send(UiCommand{UiCommandType::LoadDevices});
    }
    break;
  }
  case NetworkEventType::Status:
    showMessage(event.message);
    break;
  }
}

void Ui::tick() {
  const uint32_t now = millis();
  if (offline_ && connection_badge_ != nullptr) {
    lv_obj_move_foreground(connection_badge_);
  }
  if (lv_disp_get_inactive_time(nullptr) < 500) {
    noteInteraction();
  }
  if (message_label_ != nullptr &&
      static_cast<int32_t>(now - message_until_ms_) >= 0) {
    destroyMessage();
  }
  if (screen_ == Screen::Player && playback_.has_item &&
      progress_slider_ != nullptr) {
    // Ui::tick runs every loop pass, but the elapsed time only changes once a
    // second. lv_label_set_text reallocates and invalidates unconditionally,
    // so rewriting the identical string kept the label redrawing and reflushing
    // continuously for no visible change.
    const uint32_t progress = interpolatedProgressMs(playback_, now);
    const uint32_t seconds = progress / 1000U;
    if (seconds != displayed_progress_seconds_) {
      displayed_progress_seconds_ = seconds;
      lv_slider_set_value(progress_slider_, static_cast<int32_t>(seconds),
                          LV_ANIM_OFF);
      if (elapsed_label_ != nullptr) {
        lv_label_set_text(elapsed_label_, clockText(progress).c_str());
      }
    }
  }
  if (factory_reset_pressed_ && !factory_reset_requested_ &&
      now - factory_reset_started_ms_ >= 3000) {
    factory_reset_pressed_ = false;
    factory_reset_requested_ = true;
    showSetup("Factory reset requested", "Erasing setup and restarting...");
  }
  const bool should_dim = !playback_.is_playing && now - last_interaction_ms_ > kDimAfterMs;
  if (should_dim != dimmed_) {
    dimmed_ = should_dim;
    board_.setBacklight(dimmed_ ? 20 : 70);
  }
}

UiAction Ui::pollAction() {
  if (!factory_reset_requested_) {
    return UiAction::None;
  }
  factory_reset_requested_ = false;
  return UiAction::FactoryResetRequested;
}

bool Ui::send(UiCommand command) {
  noteInteraction();
  if (!network_.enqueue(command)) {
    showMessage("Spotify is not connected yet", true);
    return false;
  }
  return true;
}

void Ui::noteInteraction() {
  last_interaction_ms_ = millis();
  if (dimmed_) {
    dimmed_ = false;
    board_.setBacklight(70);
  }
}

void Ui::previousEvent(lv_event_t *event) {
  self(event)->send(UiCommand{UiCommandType::Previous});
}
void Ui::playEvent(lv_event_t *event) {
  Ui *ui = self(event);
  if (ui->send(UiCommand{UiCommandType::TogglePlay})) {
    ui->playback_.is_playing = !ui->playback_.is_playing;
    ui->updatePlaybackWidgets();
  }
}
void Ui::nextEvent(lv_event_t *event) {
  self(event)->send(UiCommand{UiCommandType::Next});
}
void Ui::shuffleEvent(lv_event_t *event) {
  self(event)->send(UiCommand{UiCommandType::ToggleShuffle});
}
void Ui::repeatEvent(lv_event_t *event) {
  self(event)->send(UiCommand{UiCommandType::CycleRepeat});
}
void Ui::deviceEvent(lv_event_t *event) {
  Ui *ui = self(event);
  ui->send(UiCommand{UiCommandType::LoadDevices});
}
void Ui::volumeOpenEvent(lv_event_t *event) {
  Ui *ui = self(event);
  ui->noteInteraction();
  ui->showVolumeOverlay();
}
void Ui::volumeEvent(lv_event_t *event) {
  Ui *ui = self(event);
  UiCommand command{UiCommandType::SetVolume};
  command.value = static_cast<uint32_t>(
      lv_slider_get_value(lv_event_get_target(event)));
  ui->send(std::move(command));
}
void Ui::volumeCloseEvent(lv_event_t *event) {
  lv_obj_t *button = lv_event_get_target(event);
  lv_obj_del(lv_obj_get_parent(button));
}
void Ui::seekEvent(lv_event_t *event) {
  Ui *ui = self(event);
  UiCommand command{UiCommandType::Seek};
  command.value = static_cast<uint32_t>(
                      lv_slider_get_value(lv_event_get_target(event))) *
                  1000U;
  ui->send(std::move(command));
}
void Ui::spotifyLinkEvent(lv_event_t *event) { self(event)->showQrCode(); }

void Ui::gestureEvent(lv_event_t *event) {
  Ui *ui = self(event);
  const lv_dir_t direction = lv_indev_get_gesture_dir(lv_indev_get_act());
  if (ui->screen_ == Screen::Player && direction == LV_DIR_LEFT) {
    if (ui->send(UiCommand{UiCommandType::Next})) {
      ui->animatePlayerSwipe(SwipeDirection::Next);
    }
  } else if (ui->screen_ == Screen::Player && direction == LV_DIR_RIGHT) {
    if (ui->send(UiCommand{UiCommandType::Previous})) {
      ui->animatePlayerSwipe(SwipeDirection::Previous);
    }
  } else if (ui->screen_ == Screen::Player && direction == LV_DIR_TOP) {
    ui->showLibrary(true);
  } else if ((ui->screen_ == Screen::Library ||
              ui->screen_ == Screen::Playlist) &&
             direction == LV_DIR_BOTTOM) {
    ui->showPlayer();
  }
}

void Ui::backEvent(lv_event_t *event) {
  Ui *ui = self(event);
  if (ui->screen_ == Screen::Diagnostics) {
    if (ui->playback_.has_item) {
      ui->showPlayer();
    } else {
      ui->showSetup(ui->provisioning_screen_ ? "Connect USB to set up" : "Starting",
                    ui->provisioning_screen_
                        ? "Run make provision after creating your Spotify app."
                        : "Connecting securely to Spotify...");
    }
  } else if (ui->screen_ == Screen::Playlist) {
    ui->showLibrary(false);
  } else {
    ui->showPlayer();
  }
}

void Ui::likedEvent(lv_event_t *event) {
  self(event)->send(UiCommand{UiCommandType::LoadLikedSongs});
}

void Ui::playlistEvent(lv_event_t *event) {
  Ui *ui = self(event);
  const size_t encoded = reinterpret_cast<size_t>(
      lv_obj_get_user_data(lv_event_get_target(event)));
  if (encoded == 0 || encoded - 1 >= ui->playlists_.size()) {
    return;
  }
  const PlaylistSummary &playlist = ui->playlists_[encoded - 1];
  if (!playlist.items_browsable) {
    ui->showPlayOnly(playlist);
    return;
  }
  UiCommand command{UiCommandType::LoadPlaylist};
  command.id = playlist.id;
  command.uri = playlist.uri;
  command.title = playlist.name;
  ui->send(std::move(command));
}

void Ui::trackEvent(lv_event_t *event) {
  Ui *ui = self(event);
  const size_t encoded = reinterpret_cast<size_t>(
      lv_obj_get_user_data(lv_event_get_target(event)));
  if (encoded == 0 || encoded - 1 >= ui->tracks_.size()) {
    return;
  }
  const size_t index = encoded - 1;
  UiCommand command{UiCommandType::PlayTrack};
  command.uri = ui->tracks_[index].uri;
  command.position = ui->tracks_[index].position;
  command.liked = ui->tracks_are_liked_;
  if (command.liked) {
    const size_t end = std::min(ui->tracks_.size(), index + 50);
    for (size_t next = index; next < end; ++next) {
      command.uris.push_back(ui->tracks_[next].uri);
    }
  }
  if (ui->send(std::move(command))) {
    ui->showPlayer();
  }
}

void Ui::playPlaylistEvent(lv_event_t *event) {
  Ui *ui = self(event);
  UiCommand command{UiCommandType::PlayPlaylist};
  command.uri = ui->play_only_playlist_.uri;
  if (ui->send(std::move(command))) {
    ui->showPlayer();
  }
}

void Ui::listScrollEvent(lv_event_t *event) {
  Ui *ui = self(event);
  if (ui->list_ == nullptr) {
    return;
  }
  if (ui->board_.capabilities().media.row_thumbnails) {
    ui->requestVisibleThumbnails();
  }
  if (lv_obj_get_scroll_bottom(ui->list_) > 8) {
    return;
  }
  if (ui->screen_ == Screen::Library && ui->playlists_have_more_) {
    if (ui->send(UiCommand{UiCommandType::LoadMorePlaylists})) {
      ui->playlists_have_more_ = false;
    }
  } else if (ui->screen_ == Screen::Playlist && ui->tracks_have_more_) {
    if (ui->send(UiCommand{UiCommandType::LoadMoreTracks})) {
      ui->tracks_have_more_ = false;
    }
  }
}

void Ui::deviceRowEvent(lv_event_t *event) {
  Ui *ui = self(event);
  const size_t encoded = reinterpret_cast<size_t>(
      lv_obj_get_user_data(lv_event_get_target(event)));
  if (encoded == 0 || encoded - 1 >= ui->devices_.size()) {
    return;
  }
  const PlaybackDevice &device = ui->devices_[encoded - 1];
  if (device.restricted || device.id.empty()) {
    ui->showMessage("Spotify does not allow transfer to this device", true);
    return;
  }
  UiCommand command{UiCommandType::TransferDevice};
  command.id = device.id;
  if (ui->send(std::move(command))) {
    ui->showPlayer();
  }
}

void Ui::diagnosticsEvent(lv_event_t *event) {
  self(event)->showTouchCalibration();
}

void Ui::calibrationEvent(lv_event_t *event) {
  Ui *ui = self(event);
  ++ui->calibration_step_;
  if (ui->calibration_step_ >= 4) {
    lv_obj_add_flag(ui->calibration_target_, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(ui->calibration_label_,
                      "Touch test passed\nAll four corners responded");
    lv_obj_set_style_text_color(ui->calibration_label_, lv_color_hex(0x1ED760),
                                0);
    return;
  }
  static constexpr lv_coord_t compact_positions[4][2] = {
      {0, 42}, {196, 42}, {196, 276}, {0, 276}};
  static constexpr lv_coord_t wide_positions[4][2] = {
      {0, 64}, {960, 64}, {960, 464}, {0, 464}};
  const lv_coord_t(*positions)[2] =
      ui->board_.wide() ? wide_positions : compact_positions;
  lv_obj_set_pos(ui->calibration_target_, positions[ui->calibration_step_][0],
                 positions[ui->calibration_step_][1]);
}

void Ui::settingsEvent(lv_event_t *event) { self(event)->showWideSettings(); }

void Ui::nowPlayingEvent(lv_event_t *event) { self(event)->showPlayer(); }

void Ui::libraryNavEvent(lv_event_t *event) {
  self(event)->showLibrary(true);
}

void Ui::qrNavEvent(lv_event_t *event) { self(event)->showQrCode(); }

void Ui::factoryResetEvent(lv_event_t *event) {
  Ui *ui = self(event);
  const lv_event_code_t code = lv_event_get_code(event);
  if (code == LV_EVENT_PRESSED) {
    ui->factory_reset_pressed_ = true;
    ui->factory_reset_started_ms_ = millis();
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    ui->factory_reset_pressed_ = false;
    ui->factory_reset_started_ms_ = 0;
  }
}

} // namespace spotctl
