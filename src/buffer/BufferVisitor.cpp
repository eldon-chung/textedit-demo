#include "BufferVisitor.hpp"

#include "ArrayBuffer.hpp"
#include "PieceTable.hpp"
#include "RopeBuffer.hpp"

const std::vector<std::string>& BufferVisitor::linesOf(const ArrayBuffer& buf) {
    return buf.lines_;
}

const RopeBufferNode* BufferVisitor::rootOf(const RopeBuffer& buf) {
    return buf.root;
}

const std::string& BufferVisitor::strOf(const PieceTable& buf) {
    return buf.str;
}

std::vector<BufferVisitor::PieceInfo> BufferVisitor::piecesOf(const PieceTable& buf) {
    std::vector<PieceInfo> out;
    out.reserve(buf.pieces_behind_cursor.size() + buf.pieces_after_cursor.size());

    for (const auto& p : buf.pieces_behind_cursor) {
        PieceInfo info;
        info.start  = p.start;
        info.end    = p.end;
        info.behind = true;
        for (std::size_t nl : p.newline_pos)
            info.newlines.push_back(nl);
        out.push_back(std::move(info));
    }

    // pieces_after_cursor is stored in reverse document order (back = next after cursor)
    for (int i = (int)buf.pieces_after_cursor.size() - 1; i >= 0; --i) {
        const auto& p = buf.pieces_after_cursor[i];
        PieceInfo info;
        info.start  = p.start;
        info.end    = p.end;
        info.behind = false;
        for (std::size_t nl : p.newline_pos)
            info.newlines.push_back(nl);
        out.push_back(std::move(info));
    }

    return out;
}

std::size_t BufferVisitor::numBehindOf(const PieceTable& buf) {
    return buf.pieces_behind_cursor.size();
}