#include "BufferVisitor.hpp"
#include "ArrayBuffer.hpp"

const std::vector<std::string>& BufferVisitor::linesOf(const ArrayBuffer& buf) {
    return buf.lines_;
}
