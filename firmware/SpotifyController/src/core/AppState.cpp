#include "AppState.h"

namespace spotctl {

AppState reduce(const AppState &current, const AppEvent &event) {
  AppState next = current;
  switch (event.type) {
  case AppEventType::WifiConnecting:
    next.connection = ConnectionState::Connecting;
    break;
  case AppEventType::WifiConnected:
    next.connection = ConnectionState::Online;
    next.message.clear();
    break;
  case AppEventType::WifiDisconnected:
    next.connection = ConnectionState::Disconnected;
    next.loading = false;
    next.message = event.message.empty() ? "Offline" : event.message;
    break;
  case AppEventType::AuthorizationReady:
    next.authorization = AuthorizationState::Ready;
    next.message.clear();
    break;
  case AppEventType::AuthorizationRequired:
    next.authorization = AuthorizationState::Required;
    next.loading = false;
    next.screen = Screen::Setup;
    next.message = event.message;
    break;
  case AppEventType::PlaybackLoading:
    next.loading = true;
    break;
  case AppEventType::PlaybackLoaded:
    next.playback = event.playback;
    next.loading = false;
    next.message.clear();
    if (next.screen == Screen::Diagnostics || next.screen == Screen::Setup) {
      next.screen = Screen::Player;
    }
    break;
  case AppEventType::PlaybackFailed:
    next.loading = false;
    next.message = event.message;
    break;
  case AppEventType::Navigate:
    next.screen = event.screen;
    next.message.clear();
    break;
  case AppEventType::ClearMessage:
    next.message.clear();
    break;
  }
  return next;
}

} // namespace spotctl
