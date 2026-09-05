#include "Ui.h"

#include <algorithm>
#include <cstdio>

#include "../core/RuntimePolicy.h"

namespace spotctl {

namespace {
constexpr uint32_t kDimAfterMs = 10U * 60U * 1000U;

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
    showSetup("Connect USB to set up",
              "Run make provision after creating your Spotify app.");
  } else {
    showSetup("Starting", "Connecting securely to Spotify...");
  }
}

void Ui::clear() {
  lv_obj_clean(lv_scr_act());
  title_label_ = nullptr;
  subtitle_label_ = nullptr;
  status_label_ = nullptr;
  artwork_image_ = nullptr;
  artwork_placeholder_ = nullptr;
  progress_slider_ = nullptr;
  elapsed_label_ = nullptr;
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
}

void Ui::applyBaseStyle() {
  lv_obj_t *screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x080A0C), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(screen, lv_color_hex(0xF7F7F7), 0);
  lv_obj_set_style_text_font(screen, &lv_font_montserrat_14, 0);
  lv_obj_add_event_cb(screen, gestureEvent, LV_EVENT_GESTURE, this);

  connection_badge_ = lv_label_create(screen);
  lv_label_set_text(connection_badge_, "OFFLINE");
  lv_obj_set_style_text_font(connection_badge_, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(connection_badge_, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_color(connection_badge_, lv_color_hex(0xB3263E), 0);
  lv_obj_set_style_bg_opa(connection_badge_, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(connection_badge_, 4, 0);
  lv_obj_set_style_pad_ver(connection_badge_, 2, 0);
  lv_obj_set_style_radius(connection_badge_, 4, 0);
  lv_obj_align(connection_badge_, LV_ALIGN_TOP_RIGHT, -3, 2);
  if (!offline_) {
    lv_obj_add_flag(connection_badge_, LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_move_foreground(connection_badge_);
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
  lv_obj_align(logo, LV_ALIGN_CENTER, 0, -70);

  title_label_ = lv_label_create(lv_scr_act());
  lv_label_set_text(title_label_, title);
  lv_obj_set_width(title_label_, 216);
  lv_obj_set_style_text_align(title_label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(title_label_, &lv_font_montserrat_18, 0);
  lv_obj_align(title_label_, LV_ALIGN_CENTER, 0, -20);

  subtitle_label_ = lv_label_create(lv_scr_act());
  lv_label_set_long_mode(subtitle_label_, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(subtitle_label_, 204);
  lv_obj_set_style_text_color(subtitle_label_, lv_color_hex(0xB3B3B3), 0);
  lv_obj_set_style_text_align(subtitle_label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(subtitle_label_, detail);
  lv_obj_align(subtitle_label_, LV_ALIGN_CENTER, 0, 35);

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
  lv_obj_align(status_label_, LV_ALIGN_BOTTOM_MID, 0, -20);
  lv_obj_add_flag(status_label_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(status_label_, diagnosticsEvent, LV_EVENT_CLICKED, this);
}

void Ui::showConnecting(const char *ssid) {
  std::string detail = "Joining ";
  detail += ssid;
  showSetup("Connecting to Wi-Fi", detail.c_str());
}

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

void Ui::showPlayer() {
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
  makeButton(lv_scr_act(), LV_SYMBOL_LEFT, 4, 4, 38, 34, backEvent);
  lv_obj_t *heading = lv_label_create(lv_scr_act());
  lv_label_set_text(heading, "Your Library");
  lv_obj_set_style_text_font(heading, &lv_font_montserrat_18, 0);
  lv_obj_set_pos(heading, 52, 11);
  list_ = lv_list_create(lv_scr_act());
  lv_obj_set_pos(list_, 4, 44);
  lv_obj_set_size(list_, 232, 222);
  lv_obj_set_style_bg_color(list_, lv_color_hex(0x080A0C), 0);
  lv_obj_set_style_border_width(list_, 0, 0);
  lv_obj_set_style_pad_all(list_, 2, 0);
  lv_obj_add_event_cb(list_, listScrollEvent, LV_EVENT_SCROLL_END, this);
  rebuildPlaylistRows();
  updateMiniPlayer();
  if (request_data) {
    send(UiCommand{UiCommandType::LoadPlaylists});
  }
}

void Ui::rebuildPlaylistRows() {
  if (list_ == nullptr) {
    return;
  }
  const lv_coord_t scroll_y = lv_obj_get_scroll_y(list_);
  lv_obj_clean(list_);
  lv_obj_t *liked = lv_list_add_btn(list_, LV_SYMBOL_AUDIO, "Liked Songs");
  lv_obj_set_style_bg_color(liked, lv_color_hex(0x182A22), 0);
  lv_obj_add_event_cb(liked, likedEvent, LV_EVENT_CLICKED, this);
  for (size_t index = 0; index < playlists_.size(); ++index) {
    const PlaylistSummary &playlist = playlists_[index];
    std::string label = playlist.name;
    if (!playlist.owner.empty()) {
      label += "\nby " + playlist.owner;
    }
    if (!playlist.items_browsable) {
      label += "  (play only)";
    }
    lv_obj_t *row = lv_list_add_btn(list_, LV_SYMBOL_AUDIO, label.c_str());
    lv_obj_set_height(row, 52);
    lv_obj_set_user_data(row, reinterpret_cast<void *>(index + 1));
    lv_obj_add_event_cb(row, playlistEvent, LV_EVENT_CLICKED, this);
  }
  lv_obj_scroll_to_y(list_, scroll_y, LV_ANIM_OFF);
}

void Ui::showTracks(const std::string &title) {
  clear();
  applyBaseStyle();
  screen_ = Screen::Playlist;
  makeButton(lv_scr_act(), LV_SYMBOL_LEFT, 4, 4, 38, 34, backEvent);
  lv_obj_t *heading = lv_label_create(lv_scr_act());
  lv_obj_set_pos(heading, 50, 9);
  lv_obj_set_width(heading, 180);
  lv_label_set_long_mode(heading, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_style_text_font(heading, &lv_font_montserrat_18, 0);
  lv_label_set_text(heading, title.c_str());
  list_ = lv_list_create(lv_scr_act());
  lv_obj_set_pos(list_, 4, 44);
  lv_obj_set_size(list_, 232, 222);
  lv_obj_set_style_bg_color(list_, lv_color_hex(0x080A0C), 0);
  lv_obj_set_style_border_width(list_, 0, 0);
  lv_obj_add_event_cb(list_, listScrollEvent, LV_EVENT_SCROLL_END, this);
  rebuildTrackRows();
  updateMiniPlayer();
}

void Ui::showPlayOnly(const PlaylistSummary &playlist) {
  play_only_playlist_ = playlist;
  clear();
  applyBaseStyle();
  screen_ = Screen::Playlist;
  makeButton(lv_scr_act(), LV_SYMBOL_LEFT, 4, 4, 38, 34, backEvent);
  title_label_ = lv_label_create(lv_scr_act());
  lv_obj_set_width(title_label_, 210);
  lv_obj_set_style_text_align(title_label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(title_label_, &lv_font_montserrat_20, 0);
  lv_label_set_text(title_label_, playlist.name.c_str());
  lv_obj_align(title_label_, LV_ALIGN_CENTER, 0, -55);
  subtitle_label_ = lv_label_create(lv_scr_act());
  lv_obj_set_width(subtitle_label_, 205);
  lv_label_set_long_mode(subtitle_label_, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(subtitle_label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(subtitle_label_, lv_color_hex(0xA6ABB2), 0);
  lv_label_set_text(subtitle_label_,
                    "Spotify allows this playlist to be started, but its song list is hidden in development mode.");
  lv_obj_align(subtitle_label_, LV_ALIGN_CENTER, 0, 5);
  lv_obj_t *play = makeButton(lv_scr_act(), LV_SYMBOL_PLAY, 65, 210, 110, 48,
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
    lv_obj_t *row = lv_list_add_btn(list_, LV_SYMBOL_PLAY, label.c_str());
    lv_obj_set_height(row, 54);
    lv_obj_set_user_data(row, reinterpret_cast<void *>(index + 1));
    lv_obj_add_event_cb(row, trackEvent, LV_EVENT_CLICKED, this);
  }
  lv_obj_scroll_to_y(list_, scroll_y, LV_ANIM_OFF);
}

void Ui::showDevices() {
  clear();
  applyBaseStyle();
  screen_ = Screen::Devices;
  makeButton(lv_scr_act(), LV_SYMBOL_LEFT, 4, 4, 38, 34, backEvent);
  lv_obj_t *heading = lv_label_create(lv_scr_act());
  lv_label_set_text(heading, "Play on");
  lv_obj_set_style_text_font(heading, &lv_font_montserrat_18, 0);
  lv_obj_set_pos(heading, 52, 11);
  list_ = lv_list_create(lv_scr_act());
  lv_obj_set_pos(list_, 4, 48);
  lv_obj_set_size(list_, 232, 264);
  lv_obj_set_style_bg_color(list_, lv_color_hex(0x080A0C), 0);
  lv_obj_set_style_border_width(list_, 0, 0);
  rebuildDeviceRows();
}

void Ui::showVolumeOverlay() {
  lv_obj_t *panel = lv_obj_create(lv_scr_act());
  lv_obj_set_size(panel, 220, 112);
  lv_obj_align(panel, LV_ALIGN_CENTER, 0, 45);
  lv_obj_set_style_bg_color(panel, lv_color_hex(0x171A1F), 0);
  lv_obj_set_style_border_color(panel, lv_color_hex(0x31363E), 0);
  lv_obj_set_style_radius(panel, 12, 0);
  lv_obj_move_foreground(panel);

  lv_obj_t *heading = lv_label_create(panel);
  lv_label_set_text(heading, "Volume");
  lv_obj_set_style_text_font(heading, &lv_font_montserrat_16, 0);
  lv_obj_set_pos(heading, 6, 3);

  lv_obj_t *close = lv_btn_create(panel);
  lv_obj_set_size(close, 34, 30);
  lv_obj_align(close, LV_ALIGN_TOP_RIGHT, 7, -7);
  lv_obj_set_style_bg_opa(close, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_width(close, 0, 0);
  lv_obj_add_event_cb(close, volumeCloseEvent, LV_EVENT_CLICKED, this);
  lv_obj_t *close_label = lv_label_create(close);
  lv_label_set_text(close_label, LV_SYMBOL_CLOSE);
  lv_obj_center(close_label);

  lv_obj_t *slider = lv_slider_create(panel);
  lv_obj_set_pos(slider, 8, 55);
  lv_obj_set_size(slider, 176, 16);
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
  makeButton(lv_scr_act(), LV_SYMBOL_LEFT, 4, 4, 38, 34, backEvent);
  lv_obj_t *heading = lv_label_create(lv_scr_act());
  lv_label_set_text(heading, "Open in Spotify");
  lv_obj_set_style_text_font(heading, &lv_font_montserrat_18, 0);
  lv_obj_align(heading, LV_ALIGN_TOP_MID, 0, 12);
  lv_obj_t *qr = lv_qrcode_create(lv_scr_act(), 204, lv_color_hex(0x000000),
                                  lv_color_hex(0xFFFFFF));
  lv_qrcode_update(qr, playback_.item.spotify_url.data(),
                   playback_.item.spotify_url.size());
  lv_obj_align(qr, LV_ALIGN_CENTER, 0, 14);
  lv_obj_t *caption = lv_label_create(lv_scr_act());
  lv_label_set_text(caption, "Scan with your phone");
  lv_obj_set_style_text_color(caption, lv_color_hex(0xA6ABB2), 0);
  lv_obj_align(caption, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void Ui::showTouchCalibration() {
  clear();
  applyBaseStyle();
  screen_ = Screen::Diagnostics;
  calibration_step_ = 0;
  makeButton(lv_scr_act(), LV_SYMBOL_LEFT, 4, 4, 38, 34, backEvent);
  calibration_label_ = lv_label_create(lv_scr_act());
  lv_obj_set_width(calibration_label_, 170);
  lv_obj_set_style_text_align(calibration_label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(calibration_label_, "Touch each green corner target");
  lv_obj_align(calibration_label_, LV_ALIGN_CENTER, 0, 0);
  calibration_target_ = lv_btn_create(lv_scr_act());
  lv_obj_set_size(calibration_target_, 44, 44);
  lv_obj_set_pos(calibration_target_, 0, 42);
  lv_obj_set_style_radius(calibration_target_, 22, 0);
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
      const uint16_t zoom = artwork_->image.header.w > 0
                                ? static_cast<uint16_t>(184U * 256U /
                                                        artwork_->image.header.w)
                                : 256;
      lv_img_set_zoom(artwork_image_, zoom);
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

void Ui::showMessage(const std::string &message, bool error) {
  destroyMessage();
  message_label_ = lv_label_create(lv_scr_act());
  lv_obj_set_width(message_label_, 220);
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
    setOffline(false);
    showMessage("Spotify connected");
    break;
  case NetworkEventType::AuthorizationRequired:
    provisioning_screen_ = true;
    showSetup("Spotify login required", "Reconnect USB and run make provision.");
    break;
  case NetworkEventType::Playback:
    setOffline(false);
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
    setOffline(false);
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
    setOffline(false);
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
    setOffline(false);
    devices_ = event.devices;
    showDevices();
    break;
  case NetworkEventType::Error: {
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
    const uint32_t progress = interpolatedProgressMs(playback_, now);
    lv_slider_set_value(progress_slider_, static_cast<int32_t>(progress / 1000U),
                        LV_ANIM_OFF);
    lv_label_set_text(elapsed_label_, clockText(progress).c_str());
  }
  const bool should_dim = !playback_.is_playing && now - last_interaction_ms_ > kDimAfterMs;
  if (should_dim != dimmed_) {
    dimmed_ = should_dim;
    board_.setBacklight(dimmed_ ? 20 : 70);
  }
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
  if (ui->screen_ == Screen::Player && direction == LV_DIR_TOP) {
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
  if (ui->list_ == nullptr || lv_obj_get_scroll_bottom(ui->list_) > 8) {
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
  static constexpr lv_coord_t positions[4][2] = {
      {0, 42}, {196, 42}, {196, 276}, {0, 276}};
  if (ui->calibration_step_ >= 4) {
    lv_obj_add_flag(ui->calibration_target_, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(ui->calibration_label_,
                      "Touch test passed\nAll four corners responded");
    lv_obj_set_style_text_color(ui->calibration_label_, lv_color_hex(0x1ED760),
                                0);
    return;
  }
  lv_obj_set_pos(ui->calibration_target_,
                 positions[ui->calibration_step_][0],
                 positions[ui->calibration_step_][1]);
}

} // namespace spotctl
