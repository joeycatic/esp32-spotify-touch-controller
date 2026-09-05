#pragma once

#include <string>

#include "core/Models.h"

namespace spotctl {

enum class ConnectionState { Disconnected, Connecting, Online };
enum class AuthorizationState { Unknown, Ready, Required };
enum class Screen {
  Diagnostics,
  Setup,
  Player,
  Library,
  Playlist,
  Devices,
  QrCode,
  FactoryReset,
};

struct AppState {
  ConnectionState connection{ConnectionState::Disconnected};
  AuthorizationState authorization{AuthorizationState::Unknown};
  Screen screen{Screen::Diagnostics};
  PlaybackSnapshot playback;
  bool loading{false};
  std::string message;
};

enum class AppEventType {
  WifiConnecting,
  WifiConnected,
  WifiDisconnected,
  AuthorizationReady,
  AuthorizationRequired,
  PlaybackLoading,
  PlaybackLoaded,
  PlaybackFailed,
  Navigate,
  ClearMessage,
};

struct AppEvent {
  AppEventType type;
  PlaybackSnapshot playback;
  Screen screen{Screen::Player};
  std::string message;

  explicit AppEvent(AppEventType event_type) : type(event_type) {}
};

AppState reduce(const AppState &current, const AppEvent &event);

} // namespace spotctl
