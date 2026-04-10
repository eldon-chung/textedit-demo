#include "Editor.hpp"

#include <algorithm>
#include <fstream>

Editor::Editor(std::unique_ptr<IBuffer> buf)
    : buf_(std::move(buf)), cursor_(buf_->makeCursor()) {}

// ---- Mutations ----------------------------------------------------------

void Editor::insertChar(char ch) {
    std::lock_guard<std::mutex> lock(mutex_);
    buf_->insertChar(*cursor_, ch);
    desiredCol_ = cursor_->logicalPos().col;
    dirty_ = true;
}

void Editor::backspace() {
    std::lock_guard<std::mutex> lock(mutex_);
    buf_->backspace(*cursor_);
    desiredCol_ = cursor_->logicalPos().col;
    dirty_ = true;
}

void Editor::deleteForward() {
    std::lock_guard<std::mutex> lock(mutex_);
    buf_->deleteForward(*cursor_);
    desiredCol_ = cursor_->logicalPos().col;
    dirty_ = true;
}

void Editor::newline() {
    std::lock_guard<std::mutex> lock(mutex_);
    buf_->splitLine(*cursor_);
    desiredCol_ = 0;
    dirty_ = true;
}

// ---- Navigation ---------------------------------------------------------

void Editor::moveCursor(int drow, int dcol) {
    std::lock_guard<std::mutex> lock(mutex_);
    if      (drow < 0) buf_->moveUp(*cursor_, desiredCol_);
    else if (drow > 0) buf_->moveDown(*cursor_, desiredCol_);
    else if (dcol < 0) { buf_->moveLeft(*cursor_);  desiredCol_ = cursor_->logicalPos().col; }
    else if (dcol > 0) { buf_->moveRight(*cursor_); desiredCol_ = cursor_->logicalPos().col; }
}

void Editor::moveCursorHome() {
    std::lock_guard<std::mutex> lock(mutex_);
    buf_->moveLineStart(*cursor_);
    desiredCol_ = 0;
}

void Editor::moveCursorEnd() {
    std::lock_guard<std::mutex> lock(mutex_);
    buf_->moveLineEnd(*cursor_);
    desiredCol_ = cursor_->logicalPos().col;
}

// ---- File I/O -----------------------------------------------------------

bool Editor::openFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream f(path);
    if (!f) return false;
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(f, line))
        lines.push_back(line);
    buf_->loadFromLines(lines);
    cursor_    = buf_->makeCursor();
    desiredCol_ = 0;
    filePath_  = path;
    dirty_     = false;
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
    dirty_    = false;
    return true;
}

void Editor::setFilePath(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    filePath_ = path;
}

const std::string& Editor::filePath() const {
    return filePath_;
}

// ---- Read paths ---------------------------------------------------------

CursorPos Editor::getCursor() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cursor_->logicalPos();
}

std::size_t Editor::fetchViewport(std::size_t above, std::size_t below,
                                  std::vector<std::string>& out,
                                  ViewState& vs) const {
    std::lock_guard<std::mutex> lock(mutex_);
    vs = {cursor_->logicalPos(), buf_->bufferTypeName(), filePath_, dirty_};
    return buf_->fetchLines(*cursor_, above, below, out);
}

void Editor::acceptVisitor(BufferVisitor& v) const {
    std::lock_guard<std::mutex> lock(mutex_);
    BufferVisitor::EditorCtx ctx{cursor_->logicalPos(), cursor_.get(), filePath_,
                                 buf_->bufferTypeName(), dirty_};
    buf_->accept(v, ctx);
}

std::mutex& Editor::mutex() {
    return mutex_;
}
