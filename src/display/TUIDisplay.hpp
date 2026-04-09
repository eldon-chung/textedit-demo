#pragma once

#include "editor/Editor.hpp"

#include <string>
#include <string_view>
#include <vector>

class TUIDisplay {
public:
    explicit TUIDisplay(Editor& editor);
    ~TUIDisplay();

    void init();
    void run();

private:
    void render(const std::vector<std::string_view>& lines, CursorPos cursor,
                const std::string& filePath, const std::string& bufferType, bool isDirty);
    void drawStatusBar(CursorPos cursor, const std::string& filePath,
                       const std::string& bufferType, bool isDirty);
    void handleInput();

    Editor& editor_;
    bool running_ = false;
    std::size_t viewportTopRow_ = 0;
    int screenRows_ = 0;
    int screenCols_ = 0;
};
