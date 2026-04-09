#pragma once

#include "buffer/BufferVisitor.hpp"
#include "buffer/IBuffer.hpp"
#include "types.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

class Editor {
public:
    explicit Editor(std::unique_ptr<IBuffer> buf);

    void insertChar(char ch);
    void backspace();
    void deleteForward();
    void newline();
    void moveCursor(int drow, int dcol);
    void moveCursorHome();
    void moveCursorEnd();

    bool openFile(const std::string& path);
    bool saveFile(const std::string& path);
    void setFilePath(const std::string& path);
    const std::string& filePath() const;

    // Visitor path for viz — builds EditorCtx atomically under lock, then dispatches
    void acceptVisitor(BufferVisitor& v) const;

    // Zero-copy TUI path: locks mutex, fetches only [startRow, startRow+count) lines
    using LinesViewFn = std::function<void(const std::vector<std::string_view>&, CursorPos,
                                           const std::string& filePath,
                                           const std::string& bufferType,
                                           bool isDirty)>;
    void withLines(std::size_t startRow, std::size_t count, const LinesViewFn& fn) const;

    CursorPos getCursor() const;

    std::mutex& mutex();

private:
    void clampCursor();

    std::unique_ptr<IBuffer> buf_;
    CursorPos cursor_;
    std::string filePath_;
    bool dirty_ = false;
    mutable std::mutex mutex_;
};
