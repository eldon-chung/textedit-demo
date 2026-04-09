#pragma once

#include "buffer/BufferVisitor.hpp"
#include "buffer/IBuffer.hpp"
#include "buffer/ICursor.hpp"
#include "types.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class Editor {
public:
    explicit Editor(std::unique_ptr<IBuffer> buf);

    // Mutations
    void insertChar(char ch);
    void backspace();
    void deleteForward();
    void newline();

    // Navigation
    void moveCursor(int drow, int dcol);
    void moveCursorHome();
    void moveCursorEnd();

    // File I/O
    bool openFile(const std::string& path);
    bool saveFile(const std::string& path);
    void setFilePath(const std::string& path);
    const std::string& filePath() const;

    // Quick cursor read — for TUI viewport calculation
    CursorPos getCursor() const;

    // Viewport fetch — lines centred on cursor, plus editor metadata.
    // Returns the actual buffer row of out[0].
    struct ViewState {
        CursorPos   cursor;
        std::string bufferType;
        std::string filePath;
        bool        isDirty;
    };
    std::size_t fetchViewport(std::size_t above, std::size_t below,
                              std::vector<std::string>& out,
                              ViewState& vs) const;

    // Visitor path for viz
    void acceptVisitor(BufferVisitor& v) const;

    std::mutex& mutex();

private:
    std::unique_ptr<IBuffer>  buf_;
    std::unique_ptr<ICursor>  cursor_;
    std::size_t               desiredCol_ = 0;
    std::string               filePath_;
    bool                      dirty_      = false;
    mutable std::mutex        mutex_;
};
