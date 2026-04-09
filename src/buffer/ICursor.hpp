#pragma once

#include "types.hpp"

// Opaque position token — implementation defined per buffer type.
// The only thing the outside world can ask is the logical (row, col)
// for display purposes. Everything else is private to the buffer.
class ICursor {
public:
    virtual ~ICursor() = default;
    virtual CursorPos logicalPos() const = 0;
};
