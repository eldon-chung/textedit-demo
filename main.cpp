#include "buffer/ArrayBuffer.hpp"
#include "buffer/RopeBuffer.hpp"
#include "display/TUIDisplay.hpp"
#include "editor/Editor.hpp"
#include "server/VisServer.hpp"

#include <memory>

int main(int argc, char* argv[]) {
    // auto buf = std::make_unique<ArrayBuffer>();
    auto   buf = std::make_unique<RopeBuffer>();
    Editor editor(std::move(buf));

    if (argc > 1) {
        editor.openFile(argv[1]);
    }

    VisServer server(editor, 8080);
    server.start();

    TUIDisplay display(editor);
    display.init();
    display.run();

    server.stop();
    return 0;
}
