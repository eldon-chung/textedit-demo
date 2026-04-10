#pragma once

#include "IBuffer.hpp"

#include <cassert>
#include <cstdint>
#include <deque>
#include <memory>

struct PieceTableCursor;

class PieceTable : public IBuffer {
public:
    PieceTable();
    ~PieceTable();
    // delete the copy and move constructors and
    // assignment operators; it's one buffer for all
    PieceTable(PieceTable const&)            = delete;
    PieceTable& operator=(PieceTable const&) = delete;
    PieceTable(PieceTable&&)                 = delete;
    PieceTable& operator=(PieceTable&&)      = delete;

    std::unique_ptr<ICursor> makeCursor() override;

    void moveLeft(ICursor&) override;
    void moveRight(ICursor&) override;
    void moveUp(ICursor&, std::size_t desiredCol) override;
    void moveDown(ICursor&, std::size_t desiredCol) override;
    void moveLineStart(ICursor&) override;
    void moveLineEnd(ICursor&) override;

    void insertChar(ICursor&, char) override;
    void backspace(ICursor&) override;
    void deleteForward(ICursor&) override;
    void splitLine(ICursor&) override;

    std::size_t fetchLines(const ICursor&, std::size_t above, std::size_t below,
                           std::vector<std::string>& out) const override;

    void                     loadFromLines(const std::vector<std::string>& lines) override;
    std::vector<std::string> getAllLines() const override;

    // Expose cursor's rel_offset within pieces_behind_cursor.back() for visualization.
    std::size_t cursorRelOffset(const ICursor&) const;

    std::string bufferTypeName() const override;
    void        accept(BufferVisitor&, const BufferVisitor::EditorCtx&) const override;

private:
    friend class BufferVisitor;
    friend class PieceTableCursor;

    struct Piece {
        size_t             start;
        size_t             end; // exclusive ending
        std::deque<size_t> newline_pos;

        size_t length() const { return end - start; }
        size_t num_newlines() const { return newline_pos.size(); }
    };

    std::string        str;
    std::vector<Piece> pieces_behind_cursor; // to the left of cursor
    std::vector<Piece> pieces_after_cursor;  // to the right of cursor (but in reverse order)
};
