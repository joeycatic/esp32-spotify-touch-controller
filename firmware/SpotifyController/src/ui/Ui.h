#pragma once

#include <lvgl.h>

#include <cstdint>
#include <string>
#include <vector>

#include "../app/ControllerMessages.h"
#include "../app/NetworkService.h"
#include "../board/Board.h"
#include "../core/AppState.h"

namespace spotctl {

class Ui {
public:
  Ui(Board &board, NetworkService &network)
      : board_(board), network_(network) {}

  void begin(bool provisioning_mode);
  void tick();
  void handle(const NetworkEvent &event);
  void showConnecting(const char *ssid);
  void showOffline();
  void showFactoryResetCountdown(uint8_t seconds_remaining);
  void showFactoryResetComplete();

private:
  static void previousEvent(lv_event_t *event);
  static void playEvent(lv_event_t *event);
  static void nextEvent(lv_event_t *event);
  static void shuffleEvent(lv_event_t *event);
  static void repeatEvent(lv_event_t *event);
  static void deviceEvent(lv_event_t *event);
  static void volumeOpenEvent(lv_event_t *event);
  static void volumeEvent(lv_event_t *event);
  static void volumeCloseEvent(lv_event_t *event);
  static void seekEvent(lv_event_t *event);
  static void spotifyLinkEvent(lv_event_t *event);
  static void gestureEvent(lv_event_t *event);
  static void backEvent(lv_event_t *event);
  static void likedEvent(lv_event_t *event);
  static void playlistEvent(lv_event_t *event);
  static void trackEvent(lv_event_t *event);
  static void playPlaylistEvent(lv_event_t *event);
  static void listScrollEvent(lv_event_t *event);
  static void deviceRowEvent(lv_event_t *event);
  static void diagnosticsEvent(lv_event_t *event);
  static void calibrationEvent(lv_event_t *event);

  void clear();
  void applyBaseStyle();
  lv_obj_t *makeButton(lv_obj_t *parent, const char *symbol, lv_coord_t x,
                       lv_coord_t y, lv_coord_t width, lv_coord_t height,
                       lv_event_cb_t callback);
  void send(UiCommand command);
  void noteInteraction();
  void showSetup(const char *title, const char *detail);
  void showPlayer();
  void showLibrary(bool request_data);
  void showTracks(const std::string &title);
  void showPlayOnly(const PlaylistSummary &playlist);
  void showDevices();
  void showVolumeOverlay();
  void showQrCode();
  void showTouchCalibration();
  void rebuildPlaylistRows();
  void rebuildTrackRows();
  void rebuildDeviceRows();
  void updatePlaybackWidgets(const lv_img_dsc_t *new_artwork = nullptr);
  void updateMiniPlayer();
  void showMessage(const std::string &message, bool error = false);
  void destroyMessage();

  Board &board_;
  NetworkService &network_;
  Screen screen_{Screen::Diagnostics};
  PlaybackSnapshot playback_;
  std::vector<PlaylistSummary> playlists_;
  std::vector<TrackSummary> tracks_;
  std::vector<PlaybackDevice> devices_;
  bool playlists_have_more_{false};
  bool tracks_have_more_{false};
  bool tracks_are_liked_{false};
  std::string track_list_title_;
  PlaylistSummary play_only_playlist_;
  const lv_img_dsc_t *artwork_{nullptr};
  uint32_t last_interaction_ms_{0};
  uint32_t message_until_ms_{0};
  bool dimmed_{false};
  bool provisioning_screen_{false};
  uint8_t calibration_step_{0};

  lv_obj_t *title_label_{nullptr};
  lv_obj_t *subtitle_label_{nullptr};
  lv_obj_t *status_label_{nullptr};
  lv_obj_t *artwork_image_{nullptr};
  lv_obj_t *artwork_placeholder_{nullptr};
  lv_obj_t *progress_slider_{nullptr};
  lv_obj_t *elapsed_label_{nullptr};
  lv_obj_t *duration_label_{nullptr};
  lv_obj_t *play_button_label_{nullptr};
  lv_obj_t *shuffle_button_{nullptr};
  lv_obj_t *repeat_button_{nullptr};
  lv_obj_t *list_{nullptr};
  lv_obj_t *mini_title_{nullptr};
  lv_obj_t *mini_play_label_{nullptr};
  lv_obj_t *message_label_{nullptr};
  lv_obj_t *calibration_target_{nullptr};
  lv_obj_t *calibration_label_{nullptr};
};

} // namespace spotctl
