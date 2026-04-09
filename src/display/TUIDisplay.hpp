#pragma once

#include "editor/Editor.hpp"

class TUIDisplay {
public:
    explicit TUIDisplay(Editor& editor);
    ~TUIDisplay();

    void init();
    void run();

private:
    void render(const Editor::Snapshot& snap);
    void drawStatusBar(const Editor::Snapshot& snap);
    void handleInput();

    Editor& editor_;
    bool running_ = false;
    std::size_t viewportTopRow_ = 0;
    int screenRows_ = 0;
    int screenCols_ = 0;
};
