#pragma once

namespace spotctl {

// Rebuilding a view can reuse its parent object. Replace the old event binding
// instead of stacking another callback on that persistent object.
template <typename RemoveHandler, typename AddHandler>
void bindSingleEventHandler(RemoveHandler remove_handler,
                            AddHandler add_handler) {
  while (remove_handler()) {
  }
  add_handler();
}

} // namespace spotctl
