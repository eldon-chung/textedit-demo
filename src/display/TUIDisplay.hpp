#pragma once

#include "editor/Editor.hpp"

#include <string>
#include <vector>

class TUIDisplay {
public:
    explicit TUIDisplay(Editor& editor);
    ~TUIDisplay();

    void init();
    void run();

private:
    void render(const std::vector<std::string>& lines,
                std::size_t cursorScreenRow,
                const Editor::ViewState& vs);
    void drawStatusBar(const Editor::ViewState& vs);
    void handleInput();

    Editor&     editor_;
    bool        running_        = false;
    std::size_t viewportTopRow_ = 0;
    int         screenRows_     = 0;
    int         screenCols_     = 0;

    std::vector<std::string> lineBuf_;  // reused across frames
};
