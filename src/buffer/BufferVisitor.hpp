#pragma once

#include "types.hpp"

#include <cstddef>
#include <string>
#include <vector>

// Forward-declare all visitable buffer types
class ArrayBuffer;
class RopeBuffer;
struct RopeBufferNode;
class PieceTable;
class ICursor;

class BufferVisitor {
public:
    // Editor metadata passed alongside the buffer internals
    struct EditorCtx {
        CursorPos      cursor;
        const ICursor* cursorHandle = nullptr; // opaque cursor, for buffer-specific access
        std::string    filePath;
        std::string    bufferType;
        bool           isDirty;
    };

    // Flat description of one PieceTable piece, safe to pass across the
    // attorney boundary without exposing the private Piece type.
    struct PieceInfo {
        std::size_t              start;
        std::size_t              end;
        std::vector<std::size_t> newlines; // absolute positions in str
        bool                     behind;   // true = behind cursor, false = after
    };

    virtual ~BufferVisitor() = default;

    virtual void visit(const ArrayBuffer&, const EditorCtx&) = 0;
    virtual void visit(const RopeBuffer&, const EditorCtx&)  = 0;
    virtual void visit(const PieceTable&, const EditorCtx&) {}

protected:
    // Attorney pattern: BufferVisitor is friend of each buffer type and
    // exposes their raw internals here. Derived visitors call these.
    static const std::vector<std::string>& linesOf(const ArrayBuffer&);
    static const RopeBufferNode*           rootOf(const RopeBuffer&);

    static const std::string&          strOf(const PieceTable&);
    static std::vector<PieceInfo>      piecesOf(const PieceTable&); // document order
    static std::size_t                 numBehindOf(const PieceTable&);
};
