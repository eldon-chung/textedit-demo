# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and Run

```bash
# Configure (first time)
mkdir -p build && cd build && cmake ..

# Build
cmake --build build
# or
cd build && make

# Run
./build/textedit              # empty document
./build/textedit myfile.txt   # load file
```

No test suite or linter — only `clang-format` for style (`.clang-format` is LLVM-based, 4-space indent, 100 col limit). A `compile_commands.json` symlink at project root points into `build/` for clangd.

## Architecture

A TUI text editor that demonstrates swappable buffer data structures with a live HTTP visualization server.

**Main components** (created in `main.cpp`):
- **`Editor`** — central orchestrator; owns the buffer and cursor, handles mutations/navigation/file I/O, mutex-protected for thread safety
- **`TUIDisplay`** — ncurses terminal UI; main event loop, input → Editor calls, viewport rendering with zero-copy `string_view`
- **`VisServer`** — HTTP server on port 8080 (separate thread); serves an HTML/D3.js visualization and a `/data.json` endpoint

**Buffer abstraction** (`src/buffer/`):
- `IBuffer` / `ICursor` — pure abstract interfaces; cursor is opaque (only exposes `logicalPos() → CursorPos{row,col}`)
- `RopeBuffer` — active implementation; character-level treap (randomized BST), O(log n) ops; each node stores one char plus subtree aggregates (`num_chars`, `num_newlines`)
- `ArrayBuffer` — simple `vector<string>` implementation; kept for reference/visualization heatmap

**Visitor pattern** (`BufferVisitor`):
- Uses the attorney pattern to expose buffer internals to visitors without making the whole class public
- `VisServer` uses `JsonVizVisitor` (concrete visitor) to serialize tree structure for the D3.js frontend
- RopeBuffer visualization maps x-axis → character position (in-order index), y-axis → tree depth

**Key design decisions:**
- Buffer implementations are swapped at compile time (line 11 of `main.cpp`)
- `fetchLines()` takes the cursor as a locality hint so `RopeBuffer` can start traversal from the current tree node
- Viewport-only fetching — TUI never requests more lines than the terminal height
- HTTP server thread and TUI main thread share the Editor via mutex; no complex synchronization needed since human input is slow

## Adding a New Buffer Implementation

1. Implement `IBuffer` and a corresponding `ICursor` subclass
2. Add a `visit(YourBuffer&, EditorCtx)` method to `BufferVisitor` (and update `JsonVizVisitor` in `VisServer.cpp`)
3. Swap the `make_unique<>` call in `main.cpp`
