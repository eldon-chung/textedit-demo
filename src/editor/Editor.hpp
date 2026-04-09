#pragma once

#include "buffer/IBuffer.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

struct CursorPos {
    std::size_t row = 0;
    std::size_t col = 0;
};

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

    struct Snapshot {
        std::vector<std::string> lines;
        CursorPos cursor;
        std::string bufferType;
        std::string filePath;
        bool isDirty;
    };
    Snapshot getSnapshot() const;

    // Zero-copy TUI path: locks mutex, calls fn(views, cursor) while held
    using LinesViewFn = std::function<void(const std::vector<std::string_view>&, CursorPos,
                                           const std::string& filePath,
                                           const std::string& bufferType,
                                           bool isDirty)>;
    void withLines(const LinesViewFn& fn) const;

    std::mutex& mutex();

private:
    void clampCursor();

    std::unique_ptr<IBuffer> buf_;
    CursorPos cursor_;
    std::string filePath_;
    bool dirty_ = false;
    mutable std::mutex mutex_;
};
