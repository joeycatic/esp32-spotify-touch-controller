#pragma once

#include <cstdint>

// The expander's reset pulse is timed with delay(); native tests assert the
// byte sequence, not the timing, so this is deliberately a no-op.
inline void delay(unsigned long) {}
