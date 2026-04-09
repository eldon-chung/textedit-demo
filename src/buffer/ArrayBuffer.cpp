#include "ArrayBuffer.hpp"
#include "BufferVisitor.hpp"

#include <algorithm>

ArrayBuffer::ArrayBuffer() : lines_({""}) {}

std::size_t ArrayBuffer::lineCount() const {
    return lines_.size();
}

std::size_t ArrayBuffer::lineLength(std::size_t row) const {
    return lines_.at(row).size();
}

char ArrayBuffer::charAt(std::size_t row, std::size_t col) const {
    return lines_.at(row).at(col);
}

std::string ArrayBuffer::getLine(std::size_t row) const {
    return lines_.at(row);
}

void ArrayBuffer::insertChar(std::size_t row, std::size_t col, char ch) {
    lines_.at(row).insert(col, 1, ch);
}

void ArrayBuffer::deleteChar(std::size_t row, std::size_t col) {
    auto& line = lines_.at(row);
    if (col < line.size()) {
        line.erase(col, 1);
    }
}

void ArrayBuffer::splitLine(std::size_t row, std::size_t col) {
    auto& line = lines_.at(row);
    std::string tail = line.substr(col);
    line.erase(col);
    lines_.insert(lines_.begin() + row + 1, std::move(tail));
}

void ArrayBuffer::joinLines(std::size_t row) {
    if (row + 1 >= lines_.size()) return;
    lines_.at(row) += lines_.at(row + 1);
    lines_.erase(lines_.begin() + row + 1);
}

void ArrayBuffer::loadFromLines(const std::vector<std::string>& lines) {
    if (lines.empty()) {
        lines_ = {""};
    } else {
        lines_ = lines;
    }
}

std::vector<std::string> ArrayBuffer::getAllLines() const {
    return lines_;
}

std::vector<std::string_view> ArrayBuffer::getLinesView(std::size_t startRow, std::size_t count) const {
    if (startRow >= lines_.size()) return {};
    std::size_t end = std::min(startRow + count, lines_.size());
    return {lines_.begin() + startRow, lines_.begin() + end};
}

std::string ArrayBuffer::bufferTypeName() const {
    return "ArrayBuffer";
}

void ArrayBuffer::accept(BufferVisitor& v, const BufferVisitor::EditorCtx& ctx) const {
    v.visit(*this, ctx);
}
