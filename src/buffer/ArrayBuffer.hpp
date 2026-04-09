#pragma once

#include "IBuffer.hpp"

#include <string>
#include <vector>

class ArrayBuffer : public IBuffer {
public:
    ArrayBuffer();

    std::unique_ptr<ICursor> makeCursor() override;

    void moveLeft(ICursor&) override;
    void moveRight(ICursor&) override;
    void moveUp(ICursor&, std::size_t desiredCol) override;
    void moveDown(ICursor&, std::size_t desiredCol) override;
    void moveLineStart(ICursor&) override;
    void moveLineEnd(ICursor&) override;

    void insertChar(ICursor&, char) override;
    void backspace(ICursor&) override;
    void deleteForward(ICursor&) override;
    void splitLine(ICursor&) override;

    std::size_t fetchLines(const ICursor&, std::size_t above, std::size_t below,
                           std::vector<std::string>& out) const override;

    void loadFromLines(const std::vector<std::string>& lines) override;
    std::vector<std::string> getAllLines() const override;

    std::string bufferTypeName() const override;
    void accept(BufferVisitor&, const BufferVisitor::EditorCtx&) const override;

private:
    friend class BufferVisitor;
    std::vector<std::string> lines_;
};
