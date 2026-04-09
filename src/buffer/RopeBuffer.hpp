#pragma once

#include "IBuffer.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <random>
#include <type_traits>

struct RopeBufferNode {
    RopeBufferNode* left;
    RopeBufferNode* right;
    RopeBufferNode* parent;
    std::size_t     num_chars;
    std::size_t     num_newlines;
    char            c; // '\0' = EOF sentinel

    std::uint64_t priority;

    RopeBufferNode(char _c, std::uint64_t _priority, RopeBufferNode* _parent)
        : left(nullptr),
          right(nullptr),
          parent(_parent),
          num_chars((_c != 0) ? 1 : 0),
          num_newlines((_c == '\n') ? 1 : 0),
          c(_c),
          priority(_priority) {}

    ~RopeBufferNode() {
        delete left;
        delete right;
    }

    std::size_t left_num_chars() const { return left ? left->num_chars : 0; }
    std::size_t left_num_newlines() const { return left ? left->num_newlines : 0; }
    bool        is_newline() const { return c == '\n'; }
    bool        is_eof() const { return c == 0; }
    std::size_t num_children() const { return (left != nullptr) + (right != nullptr); }
};

struct RopeCursor;

class RopeBuffer : public IBuffer {
public:
    RopeBuffer();
    ~RopeBuffer();
    // delete the copy and move constructors and
    // assignment operators; it's one buffer for all
    RopeBuffer(RopeBuffer const&)            = delete;
    RopeBuffer& operator=(RopeBuffer const&) = delete;
    RopeBuffer(RopeBuffer&&)                 = delete;
    RopeBuffer& operator=(RopeBuffer&&)      = delete;

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

    void                     loadFromLines(const std::vector<std::string>& lines) override;
    std::vector<std::string> getAllLines() const override;

    std::string bufferTypeName() const override;
    void        accept(BufferVisitor&, const BufferVisitor::EditorCtx&) const override;

private:
    friend class BufferVisitor;
    friend class RopeCursor;

    static size_t   absolute_position(RopeBufferNode const* cur);
    static size_t   absolute_lines(RopeBufferNode const* cur);
    RopeBufferNode* find_start_of_line(size_t line) const;
    size_t          find_length_of_line(size_t line);

    uint64_t sample() { return distrib(gen); }

    static RopeBufferNode* successor(RopeBufferNode* node);
    static RopeBufferNode* predecessor(RopeBufferNode* node);
    void                   recompute(RopeBufferNode* node);
    void                   rebalance(RopeBufferNode* node);
    void                   rotate_left(RopeBufferNode* node);
    void                   rotate_right(RopeBufferNode* node);

    std::random_device                      rd;
    std::mt19937                            gen;
    std::uniform_int_distribution<uint64_t> distrib;
    RopeBufferNode*                         root;
    size_t                                  total_lines;
};
