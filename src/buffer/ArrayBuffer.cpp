#include "ArrayBuffer.hpp"
#include "BufferVisitor.hpp"

#include <algorithm>
#include <cassert>

// ---- ArrayCursor --------------------------------------------------------
// Defined here — implementation detail of ArrayBuffer, not exposed in header.
// ArrayBuffer methods cast ICursor& to ArrayCursor& via the helpers below.

class ArrayCursor : public ICursor {
public:
    explicit ArrayCursor(CursorPos pos) : pos_(pos) {}
    CursorPos logicalPos() const override { return pos_; }
    CursorPos pos_;
};

static ArrayCursor& cur(ICursor& c) {
    return static_cast<ArrayCursor&>(c);
}
static const ArrayCursor& cur(const ICursor& c) {
    return static_cast<const ArrayCursor&>(c);
}

// ---- ArrayBuffer --------------------------------------------------------

ArrayBuffer::ArrayBuffer() : lines_({""}) {}

std::unique_ptr<ICursor> ArrayBuffer::makeCursor() {
    return std::make_unique<ArrayCursor>(CursorPos{0, 0});
}

void ArrayBuffer::moveLeft(ICursor& c) {
    auto& ac = cur(c);
    if (ac.pos_.col > 0) {
        --ac.pos_.col;
    } else if (ac.pos_.row > 0) {
        --ac.pos_.row;
        ac.pos_.col = lines_[ac.pos_.row].size();
    }
}

void ArrayBuffer::moveRight(ICursor& c) {
    auto& ac = cur(c);
    if (ac.pos_.col < lines_[ac.pos_.row].size()) {
        ++ac.pos_.col;
    } else if (ac.pos_.row + 1 < lines_.size()) {
        ++ac.pos_.row;
        ac.pos_.col = 0;
    }
}

void ArrayBuffer::moveUp(ICursor& c, std::size_t desiredCol) {
    auto& ac = cur(c);
    if (ac.pos_.row == 0) return;
    --ac.pos_.row;
    ac.pos_.col = std::min(desiredCol, lines_[ac.pos_.row].size());
}

void ArrayBuffer::moveDown(ICursor& c, std::size_t desiredCol) {
    auto& ac = cur(c);
    if (ac.pos_.row + 1 >= lines_.size()) return;
    ++ac.pos_.row;
    ac.pos_.col = std::min(desiredCol, lines_[ac.pos_.row].size());
}

void ArrayBuffer::moveLineStart(ICursor& c) {
    cur(c).pos_.col = 0;
}

void ArrayBuffer::moveLineEnd(ICursor& c) {
    auto& ac = cur(c);
    ac.pos_.col = lines_[ac.pos_.row].size();
}

void ArrayBuffer::insertChar(ICursor& c, char ch) {
    auto& ac = cur(c);
    lines_[ac.pos_.row].insert(ac.pos_.col, 1, ch);
    ++ac.pos_.col;
}

void ArrayBuffer::backspace(ICursor& c) {
    auto& ac = cur(c);
    if (ac.pos_.col > 0) {
        --ac.pos_.col;
        lines_[ac.pos_.row].erase(ac.pos_.col, 1);
    } else if (ac.pos_.row > 0) {
        std::size_t prevLen = lines_[ac.pos_.row - 1].size();
        lines_[ac.pos_.row - 1] += lines_[ac.pos_.row];
        lines_.erase(lines_.begin() + ac.pos_.row);
        --ac.pos_.row;
        ac.pos_.col = prevLen;
    }
}

void ArrayBuffer::deleteForward(ICursor& c) {
    auto& ac = cur(c);
    if (ac.pos_.col < lines_[ac.pos_.row].size()) {
        lines_[ac.pos_.row].erase(ac.pos_.col, 1);
    } else if (ac.pos_.row + 1 < lines_.size()) {
        lines_[ac.pos_.row] += lines_[ac.pos_.row + 1];
        lines_.erase(lines_.begin() + ac.pos_.row + 1);
    }
}

void ArrayBuffer::splitLine(ICursor& c) {
    auto& ac = cur(c);
    std::string tail = lines_[ac.pos_.row].substr(ac.pos_.col);
    lines_[ac.pos_.row].erase(ac.pos_.col);
    lines_.insert(lines_.begin() + ac.pos_.row + 1, std::move(tail));
    ++ac.pos_.row;
    ac.pos_.col = 0;
}

std::size_t ArrayBuffer::fetchLines(const ICursor& c,
                                    std::size_t above,
                                    std::size_t below,
                                    std::vector<std::string>& out) const {
    const auto& ac = cur(c);
    std::size_t row     = ac.pos_.row;
    std::size_t start   = row > above ? row - above : 0;
    std::size_t end     = std::min(row + below + 1, lines_.size());
    out.clear();
    out.reserve(end - start);
    for (std::size_t i = start; i < end; ++i)
        out.push_back(lines_[i]);
    return start;
}

void ArrayBuffer::loadFromLines(const std::vector<std::string>& lines) {
    lines_ = lines.empty() ? std::vector<std::string>{""} : lines;
}

std::vector<std::string> ArrayBuffer::getAllLines() const {
    return lines_;
}

std::string ArrayBuffer::bufferTypeName() const {
    return "ArrayBuffer";
}

void ArrayBuffer::accept(BufferVisitor& v, const BufferVisitor::EditorCtx& ctx) const {
    v.visit(*this, ctx);
}
