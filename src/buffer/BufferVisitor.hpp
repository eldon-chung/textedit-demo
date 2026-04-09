#pragma once

#include "types.hpp"

#include <string>
#include <vector>

// Forward-declare all visitable buffer types
class ArrayBuffer;
class RopeBuffer;
struct RopeBufferNode;

class BufferVisitor {
public:
    // Editor metadata passed alongside the buffer internals
    struct EditorCtx {
        CursorPos   cursor;
        std::string filePath;
        std::string bufferType;
        bool        isDirty;
    };

    virtual ~BufferVisitor() = default;

    virtual void visit(const ArrayBuffer&, const EditorCtx&) = 0;
    virtual void visit(const RopeBuffer&, const EditorCtx&)  = 0;
    // future: virtual void visit(const PieceTable&,    const EditorCtx&) = 0;

protected:
    // Attorney pattern: BufferVisitor is friend of each buffer type and
    // exposes their raw internals here. Derived visitors call these.
    static const std::vector<std::string>& linesOf(const ArrayBuffer&);
    static const RopeBufferNode*           rootOf(const RopeBuffer&);
};
