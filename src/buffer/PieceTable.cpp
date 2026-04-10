#include "PieceTable.hpp"

#include "buffer/IBuffer.hpp"
#include "buffer/ICursor.hpp"
#include "types.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// ---- PieceTableCursor --------------------------------------------------------
// Defined here — implementation detail of RopeBuffer, not exposed in header.
// PieceTable methods cast ICursor& to RopeCursor& via the helpers below.

class PieceTableCursor : public ICursor {
public:
    explicit PieceTableCursor(PieceTable const* _p, size_t i) : table(_p), rel_offset(i) {}

    CursorPos logicalPos() const override {
        size_t row = 0;
        size_t col = 0;
        size_t n   = table->pieces_behind_cursor.size();
        for (size_t pi = 0; pi < n; ++pi) {
            const PieceTable::Piece& p    = table->pieces_behind_cursor[pi];
            size_t                   stop = (pi + 1 < n) ? p.end : p.start + rel_offset;
            for (size_t i = p.start; i < stop; ++i) {
                if (table->str[i] == '\n') {
                    ++row;
                    col = 0;
                } else {
                    ++col;
                }
            }
        }
        return CursorPos{row, col};
    }

    size_t rel_offset;

private:
    PieceTable const* table;
};

static PieceTableCursor& cur(ICursor& c) {
    return static_cast<PieceTableCursor&>(c);
}
static const PieceTableCursor& cur(const ICursor& c) {
    return static_cast<const PieceTableCursor&>(c);
}

// ---- PieceTable --------------------------------------------------------
PieceTable::PieceTable() {
    pieces_behind_cursor.push_back(Piece{0, 0, {}});
}

PieceTable::~PieceTable() {}

std::unique_ptr<ICursor> PieceTable::makeCursor() {
    return std::make_unique<PieceTableCursor>(this, 0);
}

void PieceTable::moveLeft(ICursor& ic) {
    PieceTableCursor& cursor = cur(ic);

    if (cursor.rel_offset > 0) {
        --cursor.rel_offset;
    } else if (pieces_behind_cursor.size() > 1) {
        // else try to move 1 piece back
        // pieces_behind_cursor must have at least 1 piece
        pieces_after_cursor.push_back(pieces_behind_cursor.back());
        pieces_behind_cursor.pop_back();
        assert(pieces_behind_cursor.back().end != pieces_behind_cursor.back().start);

        Piece& curr_piece = pieces_behind_cursor.back();
        cursor.rel_offset = curr_piece.length() - 1; // point it to the end of the last piece
    }
}

void PieceTable::moveRight(ICursor& ic) {
    PieceTableCursor& cursor = cur(ic);

    // Normalize "past end of non-last piece": insertChar/backspace/deleteForward
    // can leave rel_offset == back().length() with pieces_after non-empty.
    // That position is logically equivalent to "on first char of the next piece",
    // so pull the next piece in and reset rel_offset before advancing.
    if (!pieces_behind_cursor.empty() &&
        cursor.rel_offset == pieces_behind_cursor.back().length() &&
        !pieces_after_cursor.empty()) {
        pieces_behind_cursor.push_back(pieces_after_cursor.back());
        pieces_after_cursor.pop_back();
        cursor.rel_offset = 0;
    }

    if (pieces_behind_cursor.empty() || pieces_behind_cursor.back().length() == 0) {
        return; // empty doc: nowhere to move
    }

    Piece const& curr_piece = pieces_behind_cursor.back();

    if (cursor.rel_offset == curr_piece.length() - 1 and (not pieces_after_cursor.empty())) {
        // we need to try to move to the front of the next piece
        pieces_behind_cursor.push_back(pieces_after_cursor.back());
        pieces_after_cursor.pop_back();
        cursor.rel_offset = 0;
    } else if ((cursor.rel_offset < curr_piece.length() - 1) or
               (cursor.rel_offset == curr_piece.length() - 1 and pieces_after_cursor.empty())) {
        // we progress in current piece or we hit EOF
        ++cursor.rel_offset;
    }
}

void PieceTable::moveUp(ICursor& ic, size_t desiredCol) {
    // now we need to move to the previous line

    moveLineStart(ic); // move to the start of the line
    moveLeft(ic);      // move to previous line
    moveLineStart(ic); // back to the start
    PieceTableCursor& cursor = cur(ic);
    for (size_t iter = 0; iter < desiredCol; ++iter) {
        // try to move but stop if we see a newline
        assert(cursor.rel_offset + pieces_behind_cursor.back().start <= str.length());
        if (cursor.rel_offset + pieces_behind_cursor.back().start == str.length()) {
            break; // hit EOF
        } else if (str[cursor.rel_offset + pieces_behind_cursor.back().start] == '\n') {
            break; // hit the newline stahp
        }
        moveRight(ic); // safe to move right one more time
    }
}

void PieceTable::moveDown(ICursor& ic, size_t desiredCol) {
    moveLineEnd(ic); // move to the end of the current line ('\n')
    moveRight(ic);   // move to start of next line
    PieceTableCursor& cursor = cur(ic);
    for (size_t iter = 0; iter < desiredCol; ++iter) {
        // try to move but stop if we see a newline
        assert(cursor.rel_offset + pieces_behind_cursor.back().start <= str.length());
        if (cursor.rel_offset + pieces_behind_cursor.back().start == str.length()) {
            break; // hit EOF
        } else if (str[cursor.rel_offset + pieces_behind_cursor.back().start] == '\n') {
            break; // hit the newline stahp
        }
        moveRight(ic); // save to move right one more time
    }
}

void PieceTable::moveLineStart(ICursor& ic) {
    // Find the '\n' that ends the previous line, then step past it so the
    // cursor lands on the first actual character of the current line.
    PieceTableCursor& cursor     = cur(ic);
    Piece&            curr_piece = pieces_behind_cursor.back();

    size_t absolute_position = cursor.rel_offset + curr_piece.start;
    if (curr_piece.newline_pos.empty() or curr_piece.newline_pos.front() >= absolute_position) {
        // No usable newline in the current piece; walk backwards.
        bool found_newl = false;
        while (pieces_behind_cursor.size() > 1) {
            pieces_after_cursor.push_back(pieces_behind_cursor.back());
            pieces_behind_cursor.pop_back();
            if (!pieces_behind_cursor.back().newline_pos.empty()) {
                // Land cursor ON the preceding '\n', then step past it below.
                cursor.rel_offset = pieces_behind_cursor.back().newline_pos.back() -
                                    pieces_behind_cursor.back().start;
                found_newl        = true;
                break;
            }
        }

        if (!found_newl) {
            // No preceding newline anywhere — already on the first line.
            cursor.rel_offset = 0;
        } else {
            moveRight(ic); // advance past '\n' to first char of line
        }
        return;
    }

    // Easy case: the preceding '\n' is within the current piece.
    auto it = std::lower_bound(curr_piece.newline_pos.begin(), curr_piece.newline_pos.end(),
                               absolute_position);
    assert(it != curr_piece.newline_pos.begin());
    cursor.rel_offset = *--it - curr_piece.start; // ON the '\n'
    moveRight(ic);                                // step past it
}

void PieceTable::moveLineEnd(ICursor& ic) {
    // we need to find the newline after cursor
    PieceTableCursor& cursor     = cur(ic);
    Piece&            curr_piece = pieces_behind_cursor.back();

    size_t absolute_position = cursor.rel_offset + curr_piece.start;
    if (curr_piece.newline_pos.empty() or curr_piece.newline_pos.back() + 1 <= absolute_position) {
        // then we need to move out of the piece; and start iterating forwards
        bool found_newl = false;
        while (pieces_after_cursor.size() > 0) {
            // move forward
            pieces_behind_cursor.push_back(pieces_after_cursor.back());
            pieces_after_cursor.pop_back();
            // try this piece
            if (!pieces_behind_cursor.back().newline_pos.empty()) {
                // set it to just after this piece
                cursor.rel_offset = pieces_behind_cursor.back().newline_pos.front() -
                                    pieces_behind_cursor.back().start;
                found_newl        = true;
                break;
            }
        }

        if (!found_newl) {
            cursor.rel_offset = pieces_behind_cursor.back().length();
        }

        return;
    }

    // easy case: it's within the current piece:
    auto it = std::lower_bound(curr_piece.newline_pos.begin(), curr_piece.newline_pos.end(),
                               absolute_position);

    // it is the first element >= absolute position
    assert(it != curr_piece.newline_pos.end());
    // just one behind the iterator is the value we're looking for?
    cursor.rel_offset = *it - curr_piece.start;
}

void PieceTable::insertChar(ICursor& ic, char ch) {

    PieceTableCursor& cursor = cur(ic);

    // Fast path: cursor is past the last char of the last piece AND that
    // piece ends exactly at the backing string — just grow it in place.
    // This preserves the consecutive-typing optimization.
    {
        Piece& back = pieces_behind_cursor.back();
        if (cursor.rel_offset == back.length() && back.end == str.length()) {
            str.push_back(ch);
            ++back.end;
            ++cursor.rel_offset;
            if (ch == '\n') {
                back.newline_pos.push_back(str.length() - 1);
            }
            return;
        }
    }

    // Normalize state B: rel_offset == length with pieces_after non-empty
    // means the cursor is logically on the first char of the next piece.
    // Pull that next piece in so the cursor is at rel_offset 0 of a real piece.
    // This can be left by backspace/deleteForward middle splits.
    if (cursor.rel_offset == pieces_behind_cursor.back().length() &&
        !pieces_after_cursor.empty()) {
        pieces_behind_cursor.push_back(pieces_after_cursor.back());
        pieces_after_cursor.pop_back();
        cursor.rel_offset = 0;
    }

    Piece& curr_piece = pieces_behind_cursor.back();

    if (cursor.rel_offset == 0) {
        // Cursor is at the start of the current piece — push that piece to the
        // after-stack and open a fresh one-char piece for the new character.
        pieces_after_cursor.push_back(pieces_behind_cursor.back());
        pieces_behind_cursor.pop_back();

        str.push_back(ch);
        pieces_behind_cursor.push_back({str.length() - 1, str.length()});
        if (ch == '\n') {
            pieces_behind_cursor.back().newline_pos.push_back(str.length() - 1);
        }
        cursor.rel_offset = pieces_behind_cursor.back().length(); // advance past inserted char
    } else {
        // Cursor is in the middle — split the current piece into left/right,
        // distribute its newlines, then append a one-char piece for the new char.
        assert(cursor.rel_offset > 0 and cursor.rel_offset < curr_piece.length());
        Piece left_piece  = {curr_piece.start, curr_piece.start + cursor.rel_offset};
        Piece right_piece = {curr_piece.start + cursor.rel_offset, curr_piece.end};

        // Distribute newlines before the pop invalidates curr_piece.
        for (size_t pos : curr_piece.newline_pos) {
            if (pos < left_piece.end)
                left_piece.newline_pos.push_back(pos);
            else if (pos >= right_piece.start)
                right_piece.newline_pos.push_back(pos);
        }

        pieces_behind_cursor.pop_back();
        pieces_behind_cursor.push_back(std::move(left_piece));
        pieces_after_cursor.push_back(std::move(right_piece));
        str.push_back(ch);
        pieces_behind_cursor.push_back({str.length() - 1, str.length()});
        if (ch == '\n') {
            pieces_behind_cursor.back().newline_pos.push_back(str.length() - 1);
        }
        cursor.rel_offset = pieces_behind_cursor.back().length(); // advance past inserted char
    }
}

void PieceTable::backspace(ICursor& ic) {
    // use these two to figure out our current cursor position
    PieceTableCursor& cursor     = cur(ic);
    Piece&            curr_piece = pieces_behind_cursor.back();

    if (curr_piece.length() > 0 and cursor.rel_offset == curr_piece.length()) {
        // easy case just shrink the current piece
        --curr_piece.end;
        --cursor.rel_offset;

        // maintain the newline positions on the piece itself
        if (str[pieces_behind_cursor.back().end] == '\n') {
            assert(pieces_behind_cursor.back().newline_pos.back() ==
                   pieces_behind_cursor.back().end);
            pieces_behind_cursor.back().newline_pos.pop_back();
        }
    } else if (cursor.rel_offset == 0 and pieces_behind_cursor.size() >= 2) {
        // move to the previous piece first
        pieces_after_cursor.push_back(pieces_behind_cursor.back());
        pieces_behind_cursor.pop_back();
        pieces_behind_cursor.back().end--;
        cursor.rel_offset = pieces_behind_cursor.back().length();

        // maintain the newline positions on the piece itself
        if (str[pieces_behind_cursor.back().end] == '\n') {
            assert(pieces_behind_cursor.back().newline_pos.back() ==
                   pieces_behind_cursor.back().end);
            pieces_behind_cursor.back().newline_pos.pop_back();
        }
    } else if (cursor.rel_offset > 0) {
        // need to split into 2 pieces, one behind, one after. then append the char piece
        Piece left_piece  = {curr_piece.start, curr_piece.start + cursor.rel_offset - 1};
        Piece right_piece = {curr_piece.start + cursor.rel_offset, curr_piece.end};

        // split up the newlines. discard the newline if it's deleted
        for (size_t pos : pieces_behind_cursor.back().newline_pos) {
            if (pos < left_piece.end) {
                left_piece.newline_pos.push_back(pos);
            } else if (pos >= right_piece.start) {
                right_piece.newline_pos.push_back(pos);
            }
        }

        pieces_behind_cursor.pop_back(); // remove this piece
        pieces_behind_cursor.push_back(left_piece);
        pieces_after_cursor.push_back(right_piece);
        cursor.rel_offset = pieces_behind_cursor.back().length();
    }

    // invariant, cursor should now be just after the current piece
    // need to handle the 0 span case
    if (pieces_behind_cursor.back().length() > 0) {
        return;
    }

    pieces_behind_cursor.pop_back();

    if (not pieces_after_cursor.empty()) { // now bring forward the next piece
        pieces_behind_cursor.push_back(pieces_after_cursor.back());
        pieces_after_cursor.pop_back();
        cursor.rel_offset = 0;
    } else if (not pieces_behind_cursor
                       .empty()) { // place it at the last position of the previous piece
        cursor.rel_offset = pieces_behind_cursor.back().length();
    } else {
        assert(pieces_behind_cursor.empty() and pieces_after_cursor.empty());
        pieces_behind_cursor.push_back({str.length(), str.length()}); // new empty piece
    }
}

void PieceTable::deleteForward(ICursor& ic) {
    // use these two to figure out our current cursor position
    PieceTableCursor& cursor     = cur(ic);
    Piece&            curr_piece = pieces_behind_cursor.back();

    if (cursor.rel_offset == 0) {
        // easy case just shrink the current piece
        if (str[curr_piece.start] == '\n') {
            curr_piece.newline_pos.pop_front();
        }
        ++curr_piece.start;
    } else if (cursor.rel_offset == curr_piece.length() and !pieces_after_cursor.empty()) {
        // move to the next piece first
        pieces_behind_cursor.push_back(pieces_after_cursor.back());
        pieces_after_cursor.pop_back();
        if (str[pieces_behind_cursor.back().start] == '\n') {
            pieces_behind_cursor.back().newline_pos.pop_front();
        }
        pieces_behind_cursor.back().start++;
        cursor.rel_offset = 0;
    } else if (cursor.rel_offset < curr_piece.length()) {
        // need to split into 2 pieces, one behind, one after. then append the char piece
        Piece left_piece  = {curr_piece.start, curr_piece.start + cursor.rel_offset};
        Piece right_piece = {curr_piece.start + cursor.rel_offset + 1, curr_piece.end};

        // split up the newlines. discard the newline if it's deleted
        for (size_t pos : pieces_behind_cursor.back().newline_pos) {
            if (pos < left_piece.end) {
                left_piece.newline_pos.push_back(pos);
            } else if (pos >= right_piece.start) {
                right_piece.newline_pos.push_back(pos);
            }
        }

        pieces_behind_cursor.pop_back(); // remove this piece
        pieces_behind_cursor.push_back(left_piece);
        if (right_piece.length() > 0) {
            pieces_after_cursor.push_back(right_piece); // if it's an empty piece don't bother
        }
        cursor.rel_offset = pieces_behind_cursor.back().length();
    }

    // invariant, cursor should now be just after the current piece
    // need to handle the 0 span case
    if (pieces_behind_cursor.back().length() > 0) {
        return;
    }

    // otherwise the piece we're on is tmpy
    pieces_behind_cursor.pop_back();

    if (not pieces_after_cursor.empty()) { // now bring forward the next piece
        pieces_behind_cursor.push_back(pieces_after_cursor.back());
        pieces_after_cursor.pop_back();
        cursor.rel_offset = 0;
    } else if (not pieces_behind_cursor
                       .empty()) { // place it at the last position of the previous piece
        cursor.rel_offset = pieces_behind_cursor.back().length();
    } else {
        assert(pieces_behind_cursor.empty() and pieces_after_cursor.empty());
        pieces_behind_cursor.push_back({str.length(), str.length()}); // new empty piece
    }
}

void PieceTable::splitLine(ICursor& ic) {
    insertChar(ic, '\n');
}

std::vector<std::string> PieceTable::getAllLines() const {
    std::vector<std::string> out;
    out.emplace_back();
    auto emit = [&](const Piece& p) {
        for (size_t i = p.start; i < p.end; ++i) {
            if (str[i] == '\n') out.emplace_back();
            else                out.back().push_back(str[i]);
        }
    };
    for (const Piece& p : pieces_behind_cursor) emit(p);
    for (int i = (int)pieces_after_cursor.size() - 1; i >= 0; --i) emit(pieces_after_cursor[i]);
    return out;
}

std::string PieceTable::bufferTypeName() const {
    return "PieceTable (Linear Version)";
}

std::size_t PieceTable::cursorRelOffset(const ICursor& ic) const {
    return cur(ic).rel_offset;
}

void PieceTable::accept(BufferVisitor& v, const BufferVisitor::EditorCtx& ctx) const {
    v.visit(*this, ctx);
}

void PieceTable::loadFromLines(const std::vector<std::string>& lines) {
    // squish into a single string
    str.clear();
    size_t total_size = 0;
    for (auto const& l : lines) {
        total_size += l.size();
    }

    std::deque<size_t> newlines = {};
    str.reserve(total_size + lines.size() - 1);
    for (auto const& l : lines) {
        str.append(l);
        newlines.push_back(str.length());
        str.push_back('\n');
    }
    newlines.pop_back(); // remove the last one
    assert(str.back() == '\n');
    str.pop_back();

    pieces_after_cursor.clear();
    pieces_behind_cursor.clear();
    pieces_behind_cursor.push_back({0, str.length()});
    pieces_behind_cursor.back().newline_pos = std::move(newlines);
}

size_t PieceTable::fetchLines(const ICursor& ic, size_t above, size_t below,
                              std::vector<std::string>& out) const {
    out.clear();
    const PieceTableCursor& cursor = cur(ic);

    // Count newlines strictly before the cursor to find current_line.
    size_t current_line = 0;
    for (size_t i = 0; i + 1 < pieces_behind_cursor.size(); ++i)
        current_line += pieces_behind_cursor[i].num_newlines();
    {
        const Piece& cp  = pieces_behind_cursor.back();
        size_t       abs = cp.start + cursor.rel_offset;
        for (size_t nl : cp.newline_pos) {
            if (nl < abs)
                ++current_line;
            else
                break; // newline_pos is sorted
        }
    }

    size_t start_line = current_line > above ? current_line - above : 0;
    size_t end_line   = current_line + below; // inclusive

    // Walk all pieces in document order, emitting lines [start_line, end_line].
    size_t line = 0;
    if (start_line == 0)
        out.emplace_back();

    auto visit = [&](const Piece& p) {
        for (size_t i = p.start; i < p.end; ++i) {
            if (line > end_line)
                return;
            if (str[i] == '\n') {
                ++line;
                if (line >= start_line && line <= end_line)
                    out.emplace_back();
            } else if (line >= start_line) {
                out.back().push_back(str[i]);
            }
        }
    };

    for (const Piece& p : pieces_behind_cursor)
        visit(p);
    for (int i = (int)pieces_after_cursor.size() - 1; i >= 0; --i)
        visit(pieces_after_cursor[i]);

    return start_line;
}
