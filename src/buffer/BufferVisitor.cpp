#include "BufferVisitor.hpp"

#include "ArrayBuffer.hpp"
#include "RopeBuffer.hpp"

const std::vector<std::string>& BufferVisitor::linesOf(const ArrayBuffer& buf) {
    return buf.lines_;
}

const RopeBufferNode* BufferVisitor::rootOf(const RopeBuffer& buf) {
    return buf.root;
}