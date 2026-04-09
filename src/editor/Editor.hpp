#pragma once

#include "buffer/IBuffer.hpp"

#include <memory>
#include <mutex>
#include <string>
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

    std::mutex& mutex();

private:
    void clampCursor();

    std::unique_ptr<IBuffer> buf_;
    CursorPos cursor_;
    std::string filePath_;
    bool dirty_ = false;
    mutable std::mutex mutex_;
};
