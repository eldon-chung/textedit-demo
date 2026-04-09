#include "Editor.hpp"

#include <fstream>
#include <algorithm>

Editor::Editor(std::unique_ptr<IBuffer> buf) : buf_(std::move(buf)) {}

void Editor::insertChar(char ch) {
    std::lock_guard<std::mutex> lock(mutex_);
    buf_->insertChar(cursor_.row, cursor_.col, ch);
    ++cursor_.col;
    dirty_ = true;
}

void Editor::backspace() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (cursor_.col > 0) {
        --cursor_.col;
        buf_->deleteChar(cursor_.row, cursor_.col);
        dirty_ = true;
    } else if (cursor_.row > 0) {
        std::size_t prevLen = buf_->lineLength(cursor_.row - 1);
        buf_->joinLines(cursor_.row - 1);
        --cursor_.row;
        cursor_.col = prevLen;
        dirty_ = true;
    }
}

void Editor::deleteForward() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (cursor_.col < buf_->lineLength(cursor_.row)) {
        buf_->deleteChar(cursor_.row, cursor_.col);
        dirty_ = true;
    } else if (cursor_.row + 1 < buf_->lineCount()) {
        buf_->joinLines(cursor_.row);
        dirty_ = true;
    }
}

void Editor::newline() {
    std::lock_guard<std::mutex> lock(mutex_);
    buf_->splitLine(cursor_.row, cursor_.col);
    ++cursor_.row;
    cursor_.col = 0;
    dirty_ = true;
}

void Editor::moveCursor(int drow, int dcol) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (drow < 0 && cursor_.row > 0) {
        cursor_.row -= static_cast<std::size_t>(-drow);
    } else if (drow > 0) {
        cursor_.row = std::min(cursor_.row + static_cast<std::size_t>(drow),
                               buf_->lineCount() - 1);
    }
    if (dcol < 0 && cursor_.col > 0) {
        cursor_.col -= static_cast<std::size_t>(-dcol);
    } else if (dcol > 0) {
        cursor_.col += static_cast<std::size_t>(dcol);
    }
    clampCursor();
}

void Editor::moveCursorHome() {
    std::lock_guard<std::mutex> lock(mutex_);
    cursor_.col = 0;
}

void Editor::moveCursorEnd() {
    std::lock_guard<std::mutex> lock(mutex_);
    cursor_.col = buf_->lineLength(cursor_.row);
}

bool Editor::openFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream f(path);
    if (!f) return false;
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(f, line)) {
        lines.push_back(line);
    }
    buf_->loadFromLines(lines);
    cursor_ = {0, 0};
    filePath_ = path;
    dirty_ = false;
    return true;
}

bool Editor::saveFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (path.empty()) return false;
    std::ofstream f(path);
    if (!f) return false;
    auto lines = buf_->getAllLines();
    for (std::size_t i = 0; i < lines.size(); ++i) {
        f << lines[i];
        if (i + 1 < lines.size()) f << '\n';
    }
    filePath_ = path;
    dirty_ = false;
    return true;
}

void Editor::setFilePath(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    filePath_ = path;
}

const std::string& Editor::filePath() const {
    return filePath_;
}

Editor::Snapshot Editor::getSnapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return {buf_->getAllLines(), cursor_, buf_->bufferTypeName(), filePath_, dirty_};
}

std::mutex& Editor::mutex() {
    return mutex_;
}

void Editor::clampCursor() {
    std::size_t maxRow = buf_->lineCount() - 1;
    if (cursor_.row > maxRow) cursor_.row = maxRow;
    std::size_t maxCol = buf_->lineLength(cursor_.row);
    if (cursor_.col > maxCol) cursor_.col = maxCol;
}
