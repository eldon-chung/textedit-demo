#pragma once

#include "BufferVisitor.hpp"
#include "ICursor.hpp"

#include <memory>
#include <string>
#include <vector>

class IBuffer {
public:
    virtual ~IBuffer() = default;

    // Cursor factory — each DS creates its own opaque cursor type
    virtual std::unique_ptr<ICursor> makeCursor() = 0;

    // Navigation — buffer updates the opaque cursor in-place
    virtual void moveLeft(ICursor&) = 0;
    virtual void moveRight(ICursor&) = 0;
    // desiredCol: the column to restore when moving vertically across
    // lines of different lengths (maintained by Editor, passed down here)
    virtual void moveUp(ICursor&, std::size_t desiredCol) = 0;
    virtual void moveDown(ICursor&, std::size_t desiredCol) = 0;
    virtual void moveLineStart(ICursor&) = 0;
    virtual void moveLineEnd(ICursor&) = 0;

    // Mutations — buffer updates cursor to the correct post-mutation position
    virtual void insertChar(ICursor&, char) = 0;
    virtual void backspace(ICursor&) = 0;     // delete char before cursor
    virtual void deleteForward(ICursor&) = 0; // delete char at cursor
    virtual void splitLine(ICursor&) = 0;     // Enter / newline

    // Viewport fetch — cursor is both the anchor and a locality hint.
    // Fills `out` with up to (above + 1 + below) lines centred on the
    // cursor's current line.  Returns the actual buffer row of out[0].
    virtual std::size_t fetchLines(const ICursor&,
                                   std::size_t above,
                                   std::size_t below,
                                   std::vector<std::string>& out) const = 0;

    // File I/O
    virtual void loadFromLines(const std::vector<std::string>& lines) = 0;
    virtual std::vector<std::string> getAllLines() const = 0;

    virtual std::string bufferTypeName() const = 0;
    virtual void accept(BufferVisitor&, const BufferVisitor::EditorCtx&) const = 0;
};
