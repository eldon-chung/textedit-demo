#include "TUIDisplay.hpp"

#include <ncurses.h>
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

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
        editor_.withLines([this](const std::vector<std::string_view>& lines, CursorPos cursor,
                                 const std::string& filePath, const std::string& bufferType,
                                 bool isDirty) {
            // Scroll viewport so cursor is visible
            if (cursor.row < viewportTopRow_) {
                viewportTopRow_ = cursor.row;
            } else if (cursor.row >= viewportTopRow_ + static_cast<std::size_t>(screenRows_ - 1)) {
                viewportTopRow_ = cursor.row - static_cast<std::size_t>(screenRows_ - 2);
            }
            render(lines, cursor, filePath, bufferType, isDirty);
        });
        handleInput();
    }
}

void TUIDisplay::render(const std::vector<std::string_view>& lines, CursorPos cursor,
                        const std::string& filePath, const std::string& bufferType, bool isDirty) {
    getmaxyx(stdscr, screenRows_, screenCols_);
    erase();

    int drawRows = screenRows_ - 1;  // reserve last row for status bar
    for (int r = 0; r < drawRows; ++r) {
        std::size_t lineIdx = viewportTopRow_ + static_cast<std::size_t>(r);
        if (lineIdx >= lines.size()) break;

        std::string_view line = lines[lineIdx];
        if (line.size() > static_cast<std::size_t>(screenCols_))
            line = line.substr(0, screenCols_);
        mvprintw(r, 0, "%.*s", static_cast<int>(line.size()), line.data());
    }

    drawStatusBar(cursor, filePath, bufferType, isDirty);

    int cursorScreenRow = static_cast<int>(cursor.row) - static_cast<int>(viewportTopRow_);
    int cursorScreenCol = static_cast<int>(cursor.col);
    if (cursorScreenRow >= 0 && cursorScreenRow < screenRows_ - 1) {
        move(cursorScreenRow, std::min(cursorScreenCol, screenCols_ - 1));
    }

    refresh();
}

void TUIDisplay::drawStatusBar(CursorPos cursor, const std::string& filePath,
                               const std::string& bufferType, bool isDirty) {
    std::string name = filePath.empty() ? "[No Name]" : filePath;
    std::string dirty = isDirty ? " *" : "";
    std::string pos = std::to_string(cursor.row + 1) + ":" + std::to_string(cursor.col + 1);
    std::string status = " " + name + dirty + "  [" + bufferType + "]  " + pos + " ";

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
