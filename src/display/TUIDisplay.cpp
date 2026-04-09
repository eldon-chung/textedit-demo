#include "TUIDisplay.hpp"

#include <ncurses.h>
#include <algorithm>
#include <string>

TUIDisplay::TUIDisplay(Editor& editor) : editor_(editor) {}

TUIDisplay::~TUIDisplay() {
    endwin();
}

void TUIDisplay::init() {
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    start_color();
    use_default_colors();

    init_pair(1, COLOR_BLACK, COLOR_WHITE);  // status bar
    init_pair(2, -1, -1);                    // normal text

    getmaxyx(stdscr, screenRows_, screenCols_);
    running_ = true;
}

void TUIDisplay::run() {
    while (running_) {
        auto snap = editor_.getSnapshot();

        // Scroll viewport so cursor is visible
        if (snap.cursor.row < viewportTopRow_) {
            viewportTopRow_ = snap.cursor.row;
        } else if (snap.cursor.row >= viewportTopRow_ + static_cast<std::size_t>(screenRows_ - 1)) {
            viewportTopRow_ = snap.cursor.row - static_cast<std::size_t>(screenRows_ - 2);
        }

        render(snap);
        handleInput();
    }
}

void TUIDisplay::render(const Editor::Snapshot& snap) {
    getmaxyx(stdscr, screenRows_, screenCols_);
    erase();

    int drawRows = screenRows_ - 1;  // reserve last row for status bar
    for (int r = 0; r < drawRows; ++r) {
        std::size_t lineIdx = viewportTopRow_ + static_cast<std::size_t>(r);
        if (lineIdx >= snap.lines.size()) break;

        const auto& line = snap.lines[lineIdx];
        // Truncate to screen width
        int maxCols = screenCols_;
        std::string display = line.size() > static_cast<std::size_t>(maxCols)
                              ? line.substr(0, maxCols)
                              : line;
        mvprintw(r, 0, "%s", display.c_str());
    }

    drawStatusBar(snap);

    // Place cursor
    int cursorScreenRow = static_cast<int>(snap.cursor.row) - static_cast<int>(viewportTopRow_);
    int cursorScreenCol = static_cast<int>(snap.cursor.col);
    if (cursorScreenRow >= 0 && cursorScreenRow < screenRows_ - 1) {
        move(cursorScreenRow, std::min(cursorScreenCol, screenCols_ - 1));
    }

    refresh();
}

void TUIDisplay::drawStatusBar(const Editor::Snapshot& snap) {
    std::string name = snap.filePath.empty() ? "[No Name]" : snap.filePath;
    std::string dirty = snap.isDirty ? " *" : "";
    std::string pos = std::to_string(snap.cursor.row + 1) + ":" +
                      std::to_string(snap.cursor.col + 1);
    std::string status = " " + name + dirty + "  [" + snap.bufferType + "]  " + pos + " ";

    attron(COLOR_PAIR(1));
    mvhline(screenRows_ - 1, 0, ' ', screenCols_);
    mvprintw(screenRows_ - 1, 0, "%s", status.c_str());
    attroff(COLOR_PAIR(1));
}

void TUIDisplay::handleInput() {
    int ch = getch();
    switch (ch) {
        case KEY_UP:    editor_.moveCursor(-1, 0); break;
        case KEY_DOWN:  editor_.moveCursor(1, 0);  break;
        case KEY_LEFT:  editor_.moveCursor(0, -1); break;
        case KEY_RIGHT: editor_.moveCursor(0, 1);  break;
        case KEY_HOME:  editor_.moveCursorHome();  break;
        case KEY_END:   editor_.moveCursorEnd();   break;

        case KEY_BACKSPACE:
        case 127:
            editor_.backspace();
            break;

        case KEY_DC:
            editor_.deleteForward();
            break;

        case '\n':
        case KEY_ENTER:
            editor_.newline();
            break;

        case 19:  // Ctrl-S
            editor_.saveFile(editor_.filePath());
            break;

        case 17:  // Ctrl-Q
            running_ = false;
            break;

        case KEY_RESIZE:
            getmaxyx(stdscr, screenRows_, screenCols_);
            break;

        default:
            if (ch >= 32 && ch < 127) {
                editor_.insertChar(static_cast<char>(ch));
            }
            break;
    }
}
