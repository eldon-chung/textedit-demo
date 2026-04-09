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
        getmaxyx(stdscr, screenRows_, screenCols_);
        int drawRows = screenRows_ - 1;  // minus status bar
        if (drawRows <= 0) { handleInput(); continue; }

        // 1. Quick cursor read to compute viewport before the fetch
        CursorPos cursor = editor_.getCursor();

        if (cursor.row < viewportTopRow_)
            viewportTopRow_ = cursor.row;
        else if (cursor.row >= viewportTopRow_ + static_cast<std::size_t>(drawRows))
            viewportTopRow_ = cursor.row - static_cast<std::size_t>(drawRows) + 1;

        std::size_t above = cursor.row - viewportTopRow_;
        std::size_t below = static_cast<std::size_t>(drawRows) - 1 - above;

        // 2. Fetch only the visible range — cursor is the locality hint
        Editor::ViewState vs;
        std::size_t startRow = editor_.fetchViewport(above, below, lineBuf_, vs);

        // Actual cursor screen row (may differ from `above` near buffer top)
        std::size_t cursorScreenRow = vs.cursor.row - startRow;

        render(lineBuf_, cursorScreenRow, vs);
        handleInput();
    }
}

void TUIDisplay::render(const std::vector<std::string>& lines,
                        std::size_t cursorScreenRow,
                        const Editor::ViewState& vs) {
    erase();

    int drawRows = screenRows_ - 1;
    for (int r = 0; r < drawRows; ++r) {
        if (static_cast<std::size_t>(r) >= lines.size()) break;
        std::string_view line = lines[static_cast<std::size_t>(r)];
        if (static_cast<int>(line.size()) > screenCols_)
            line = line.substr(0, static_cast<std::size_t>(screenCols_));
        mvprintw(r, 0, "%.*s", static_cast<int>(line.size()), line.data());
    }

    drawStatusBar(vs);

    int csRow = static_cast<int>(cursorScreenRow);
    int csCol = static_cast<int>(vs.cursor.col);
    if (csRow >= 0 && csRow < screenRows_ - 1)
        move(csRow, std::min(csCol, screenCols_ - 1));

    refresh();
}

void TUIDisplay::drawStatusBar(const Editor::ViewState& vs) {
    std::string name   = vs.filePath.empty() ? "[No Name]" : vs.filePath;
    std::string dirty  = vs.isDirty ? " *" : "";
    std::string pos    = std::to_string(vs.cursor.row + 1) + ":" +
                         std::to_string(vs.cursor.col + 1);
    std::string status = " " + name + dirty + "  [" + vs.bufferType + "]  " + pos + " ";

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
        case 127:           editor_.backspace();      break;
        case KEY_DC:        editor_.deleteForward();  break;
        case '\n':
        case KEY_ENTER:     editor_.newline();         break;

        case 19: editor_.saveFile(editor_.filePath()); break;  // Ctrl-S
        case 17: running_ = false;                     break;  // Ctrl-Q

        case KEY_RESIZE:
            getmaxyx(stdscr, screenRows_, screenCols_);
            break;

        default:
            if (ch >= 32 && ch < 127)
                editor_.insertChar(static_cast<char>(ch));
            break;
    }
}
