#include "RopeBuffer.hpp"

#include "types.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

// ---- Helper --------------------------------------------------------
void recompute_local(RopeBufferNode* node) {
    node->num_chars = node->left_num_chars() + ((node->is_eof()) ? 0 : 1) +
                      ((node->right) ? node->right->num_chars : 0);
    node->num_newlines = node->left_num_newlines() + ((node->c == '\n') ? 1 : 0) +
                         ((node->right) ? node->right->num_newlines : 0);
}

// ---- RopeCursor --------------------------------------------------------
// Defined here — implementation detail of RopeBuffer, not exposed in header.
// RopeBuffer methods cast ICursor& to RopeCursor& via the helpers below.

class RopeCursor : public ICursor {
public:
    explicit RopeCursor(RopeBufferNode* _node_ptr) : node_ptr(_node_ptr) {}

    CursorPos logicalPos() const override {
        size_t line_containing = RopeBuffer::absolute_lines(node_ptr);
        size_t col             = 0;

        RopeBufferNode* peek   = node_ptr;
        RopeBufferNode* behind = RopeBuffer::predecessor(peek);

        while (behind and !behind->is_newline()) {
            peek   = behind;
            behind = RopeBuffer::predecessor(peek);
            ++col;
        }
        // either no more behind or end of behind is newline
        return CursorPos{.row = line_containing, .col = col};
    }
    RopeBufferNode* node_ptr;
};

static RopeCursor& cur(ICursor& c) {
    return static_cast<RopeCursor&>(c);
}
static const RopeCursor& cur(const ICursor& c) {
    return static_cast<const RopeCursor&>(c);
}

// ---- RopeBuffer --------------------------------------------------------

RopeBuffer::RopeBuffer()
    : gen(rd()),
      distrib(std::uniform_int_distribution<uint64_t>(1, INT64_MAX)),
      //   root(std::make_unique<RopeBufferNode>(0, distrib(gen), nullptr)),
      root(new RopeBufferNode(0, distrib(gen), nullptr)),
      total_lines(1) {}

RopeBuffer::~RopeBuffer() {
    delete root;
}

std::unique_ptr<ICursor> RopeBuffer::makeCursor() {
    // Assumes makeCursor is only called at the beginning of the
    // program and never again
    static bool first_time_call = true;
    assert(first_time_call);
    first_time_call = false;
    return std::make_unique<RopeCursor>(root);
}

size_t RopeBuffer::absolute_position(RopeBufferNode const* curr_node) {

    // by convention the logical cursor is to the left of the current node
    // from here traverse to root and sum up all left tree sizes + 1
    assert(curr_node);
    size_t total_count = 0;
    total_count += curr_node->left_num_chars();
    RopeBufferNode const* prev_node = curr_node;
    curr_node                       = curr_node->parent;
    while (curr_node != nullptr) { // while we're not root node
        // the parent is on our left
        if (curr_node->right == prev_node) {
            total_count += (1 + curr_node->left_num_chars());
        }
        prev_node = curr_node;
        curr_node = curr_node->parent;
    }

    return total_count;
}

/*
    Returns nullptr if it's the rightmost (EOF)
*/
RopeBufferNode* RopeBuffer::successor(RopeBufferNode* node) {
    if (node->is_eof()) {
        return nullptr;
    }

    if (node->right) {
        node = node->right;
        while (node->left) {
            node = node->left;
        }
        return node;
    }

    RopeBufferNode* peek   = node;
    RopeBufferNode* parent = peek->parent;

    while (parent) {
        if (parent->left == peek) {
            return parent;
        }
        peek   = parent;
        parent = peek->parent;
    }

    assert(false);
    return nullptr;
}

/*
    Returns nullptr if it's the leftmost
*/
RopeBufferNode* RopeBuffer::predecessor(RopeBufferNode* node) {
    if (node->left) {
        node = node->left;
        while (node->right) {
            node = node->right;
        }
        return node;
    }

    RopeBufferNode* peek   = node;
    RopeBufferNode* parent = peek->parent;

    while (parent) {
        if (parent->right == peek) {
            return parent;
        }
        peek   = parent;
        parent = peek->parent;
    }

    return nullptr;
}

void RopeBuffer::moveLeft(ICursor& ic) {
    RopeCursor&     cursor    = cur(ic);
    RopeBufferNode* curr_node = cursor.node_ptr;

    RopeBufferNode* peek_left = predecessor(curr_node);
    if (peek_left) {
        cursor.node_ptr = peek_left;
    }
    // otherwise leave the cursor alone
}

void RopeBuffer::moveRight(ICursor& ic) {
    RopeCursor&     cursor    = cur(ic);
    RopeBufferNode* curr_node = cursor.node_ptr;

    RopeBufferNode* peek_right = successor(curr_node);
    if (peek_right) {
        cursor.node_ptr = peek_right;
    }
    // otherwise leave the cursor alone
}

size_t RopeBuffer::find_length_of_line(size_t line) {
    RopeBufferNode* line_start      = find_start_of_line(line);
    RopeBufferNode* next_line_start = find_start_of_line(line + 1);
    return absolute_position(next_line_start) - absolute_position(line_start);
}

void RopeBuffer::moveUp(ICursor& ic, std::size_t desiredCol) {
    RopeCursor&     cursor    = cur(ic);
    RopeBufferNode* curr_node = cursor.node_ptr;

    // find the current line
    size_t current_line_idx = absolute_lines(curr_node);

    // if on starting line, just reset col to 0
    if (current_line_idx == 0) {
        // move it to the leftmost node
        while (curr_node->left) {
            curr_node = curr_node->left;
        }
        cursor.node_ptr = curr_node;
        return;
    }

    // otherwise:
    // move up the line by one
    --current_line_idx;
    curr_node = find_start_of_line(current_line_idx);

    // get the line_length
    size_t line_length = find_length_of_line(current_line_idx);

    cursor.node_ptr = curr_node;

    // move right by std::min(desiredCol, line_length)
    for (size_t i = 0; i < std::min(desiredCol, line_length); ++i) {
        RopeBuffer::moveRight(cursor);
    }
}

void RopeBuffer::moveDown(ICursor& ic, std::size_t desiredCol) {
    RopeCursor&     cursor    = cur(ic);
    RopeBufferNode* curr_node = cursor.node_ptr;

    // find the current line
    size_t current_line_idx = absolute_lines(curr_node);

    // if on last line
    if (current_line_idx == total_lines - 1) {
        // move it to the rightmost node
        curr_node = root;
        while (curr_node->right) {
            curr_node = curr_node->right;
        }
        assert(curr_node->is_eof());
        cursor.node_ptr = curr_node;
        return;
    }

    // otherwise:
    // move down the line by one
    ++current_line_idx;
    curr_node = find_start_of_line(current_line_idx);

    // get the line_length
    size_t line_length = find_length_of_line(current_line_idx);

    cursor.node_ptr = curr_node;

    // move right by std::min(desiredCol, line_length)
    for (size_t i = 0; i < std::min(desiredCol, line_length); ++i) {
        RopeBuffer::moveRight(cursor);
    }
}

void RopeBuffer::moveLineStart(ICursor& ic) {
    RopeCursor& cursor       = cur(ic);
    size_t      current_line = absolute_lines(cursor.node_ptr);

    cursor.node_ptr = find_start_of_line(current_line);
}

void RopeBuffer::moveLineEnd(ICursor& ic) {
    RopeCursor& cursor       = cur(ic);
    size_t      current_line = absolute_lines(cursor.node_ptr);
    assert(current_line < total_lines);

    RopeBufferNode* node = find_start_of_line(current_line + 1);
    cursor.node_ptr      = node;
    assert(node);
    if (!node->is_eof()) {
        RopeBuffer::moveLeft(cursor);
    }
}

void RopeBuffer::insertChar(ICursor& ic, char c) {
    // insert just one behind the current node
    // find rightmost node on left subtree then rebalance with rotations later?
    if (c == '\n') {
        ++total_lines;
    }
    RopeCursor&     cursor = cur(ic);
    RopeBufferNode* parent = cursor.node_ptr;
    if (parent->left) {
        parent = parent->left;
        while (parent->right) {
            parent = parent->right;
        }
        parent->right = new RopeBufferNode(c, sample(), parent);
    } else {
        parent->left = new RopeBufferNode(c, sample(), parent);
    }
    // recompute(parent);
    rebalance(parent);
}

void RopeBuffer::backspace(ICursor& ic) {

    RopeCursor& cursor = cur(ic);
    if (absolute_position(cursor.node_ptr) == 0) {
        return;
    }
    // move back one and deleteForward
    moveLeft(ic);
    deleteForward(ic);
}

void RopeBuffer::deleteForward(ICursor& ic) {

    RopeCursor& cursor = cur(ic);
    if (cursor.node_ptr->is_eof()) {
        return;
    }

    // delete the current node
    RopeBufferNode* original     = cursor.node_ptr;
    RopeBufferNode* current_node = original;
    cursor.node_ptr              = successor(original);

    if (!current_node->left and !current_node->right) {
        // remember the parent
        RopeBufferNode* parent = current_node->parent;
        if (current_node->is_newline()) {
            --total_lines;
        }
        delete current_node;
        recompute(parent);
        return;
    }

    if (!current_node->left or !current_node->right) {
        // shift one our child up
        RopeBufferNode* child  = (current_node->left) ? current_node->left : current_node->right;
        RopeBufferNode* parent = current_node->parent;

        if (!parent) {
            root = child;
        } else {
            if (parent->left == current_node) {
                parent->left = child;
            } else {
                assert(parent->right == current_node);
                parent->right = child;
            }
        }
        if (child) {
            child->parent = parent;
        }

        // zero out your stuff so dtor works
        current_node->left  = nullptr;
        current_node->right = nullptr;
        if (current_node->is_newline()) {
            --total_lines;
        }
        delete current_node;
        recompute(parent);
        return;
    }

    // annoying case, find successor
    RopeBufferNode* succ = successor(current_node);
    // we are skipping the EOF case earlier on so succ must exist
    assert(succ);
    // replace current node with succ
    // handle succ children
    RopeBufferNode* succ_parent = succ->parent;
    if (!succ->left or !succ->right) {
        // shift one our child up
        RopeBufferNode* child = (succ->left) ? succ->left : succ->right;

        if (succ_parent->left == succ) {
            succ_parent->left = child;
        } else {
            succ_parent->right = child;
        }
        if (child) {
            child->parent = succ_parent;
        }
    }

    RopeBufferNode* parent = current_node->parent;
    if (parent->left == current_node) {
        parent->left = succ;
    } else {
        parent->right = succ;
    }
    succ->parent = parent;
    succ->left   = current_node->left;
    if (current_node->left) {
        current_node->left->parent = succ;
    }
    succ->right = current_node->right;
    if (current_node->right) {
        current_node->right->parent = succ;
    }

    current_node->left  = nullptr;
    current_node->right = nullptr;
    if (current_node->is_newline()) {
        --total_lines;
    }
    delete current_node;
    recompute(succ_parent == current_node ? succ : succ_parent);
}

// handle's parent pointers if they exist
void RopeBuffer::rotate_left(RopeBufferNode* node) {

    RopeBufferNode* parent = node->parent;

    RopeBufferNode* right = node->right;
    assert(right); // if not there's nothing to rotate upwards
    RopeBufferNode* rightleft = node->right->left;

    node->right = rightleft;
    if (rightleft) {
        rightleft->parent = node;
    }

    right->left  = node;
    node->parent = right;

    if (parent) {
        ((parent->left == node) ? parent->left : parent->right) = right;
    }
    right->parent = parent;

    // recompute parent, right, node
    recompute_local(node);
    recompute_local(right);
    // recompute_local(parent);
}

// handle's parent pointers if they exist
void RopeBuffer::rotate_right(RopeBufferNode* node) {

    RopeBufferNode* parent = node->parent;

    RopeBufferNode* left = node->left;
    assert(left); // if not there's nothing to rotate upwards
    RopeBufferNode* leftright = node->left->right;

    node->left = leftright;
    if (leftright) {
        leftright->parent = node;
    }

    left->right  = node;
    node->parent = left;

    if (parent) {
        ((parent->left == node) ? parent->left : parent->right) = left;
    }
    left->parent = parent;

    // recompute root, left, node
    recompute_local(node);
    recompute_local(left);
    // recompute_local(parent);
}

/*
    Warning! Should only be called when
    either child violates the heap invariant.

    Rebalance will not fix the case where both children violate the heap
    invariant. This fact will be asserted and the program will fail
    if this requirement is not met
*/
void RopeBuffer::rebalance(RopeBufferNode* node) {
    while (node->parent) { // while we're *not* at root node
        recompute_local(node);
        if (node->num_children() == 0) {
            node = node->parent; // move towards root node
        } else if (node->num_children() == 1) {
            if (node->left and node->left->priority < node->priority) {
                rotate_right(node);
            } else if (node->right and node->right->priority < node->priority) {
                rotate_left(node);
            }
        } else {
            assert(not(node->left->priority < node->priority and
                       node->right->priority < node->priority));
            if (node->left->priority < node->priority) {
                rotate_right(node);
            } else if (node->right->priority < node->priority) {
                rotate_left(node);
            }
        }
        node = node->parent; // move towards root node
    }
    recompute_local(node);

    // now we're at root, handle this case a little bit specially
    // cause rotation bullshit

    if (node->num_children() == 1) {
        if (node->left and node->left->priority < node->priority) {
            root = node->left;
            rotate_right(node);
        } else if (node->right and node->right->priority < node->priority) {
            root = node->right;
            rotate_left(node);
        }
    } else if (node->num_children() == 2) {
        assert(
            not(node->left->priority < node->priority and node->right->priority < node->priority));
        if (node->left->priority < node->priority) {
            root = node->left;
            rotate_right(node);
        } else if (node->right->priority < node->priority) {
            root = node->right;
            rotate_left(node);
        }
    }
}

void RopeBuffer::recompute(RopeBufferNode* node) {
    while (node) {
        recompute_local(node);
        node = node->parent;
    }
}

void RopeBuffer::splitLine(ICursor& ic) {
    insertChar(ic, '\n');
}

size_t RopeBuffer::absolute_lines(RopeBufferNode const* curr_node) {

    // by convention the logical cursor is to the left of the current node
    // from here traverse to root and sum up all left tree sizes + 1
    assert(curr_node);
    size_t total_count = 0;
    total_count += curr_node->left_num_newlines();
    RopeBufferNode const* prev_node = curr_node;
    curr_node                       = curr_node->parent;
    while (curr_node != nullptr) { // while we're not root node
        // the parent is on our left
        if (curr_node->right == prev_node) {
            total_count += (curr_node->left_num_newlines());
            total_count += (curr_node->is_newline());
        }
        prev_node = curr_node;
        curr_node = curr_node->parent;
    }

    return total_count;
}

/*
    line ranges from 0 to num_lines
    num_lines gets you the EOF node
    any larger and you get nullptr
*/
RopeBufferNode* RopeBuffer::find_start_of_line(size_t line) const {

    if (line == total_lines) {
        RopeBufferNode* curr_node = root;
        while (curr_node->right) {
            curr_node = curr_node->right;
        }
        return curr_node;
    }

    if (line > total_lines) {
        return nullptr;
    }

    RopeBufferNode* curr_node = root;
    assert(curr_node);

    if (line > curr_node->num_newlines) {
        return nullptr;
    }

    if (line == 0) { // special case, just the leftmost node
        while (curr_node->left) {
            curr_node = curr_node->left;
        }

        return curr_node;
    }

    while (true) {
        if (curr_node->left_num_newlines() == line - 1 && curr_node->is_newline()) {
            break; // found the correct node
        } else if (curr_node->left_num_newlines() >= line) {
            curr_node = curr_node->left;
        } else {
            line -= curr_node->left_num_newlines();
            line -= curr_node->is_newline();
            curr_node = curr_node->right;
        }
    }

    // now move to the "next" node
    // either leftmost of right subtree
    if (curr_node->right) {
        curr_node = curr_node->right;
        while (curr_node->left) {
            curr_node = curr_node->left;
        }
        return curr_node;
    }
    // or first right parent
    RopeBufferNode* parent = curr_node->parent;
    // we must have a parent cause minimum EOF should be on our right somewhere..
    assert(parent);
    while (parent->left != curr_node) {
        curr_node = parent;
        parent    = curr_node->parent;
    }
    return parent;
}

void RopeBuffer::loadFromLines(const std::vector<std::string>& lines) {
    delete root;
    root        = nullptr;
    total_lines = lines.empty() ? 1 : lines.size();

    // Build right-skewed treap (valid BST; insert chars in reverse order).
    // In-order traversal of the result = char0, char1, ..., charN, EOF.
    RopeBufferNode* tail = new RopeBufferNode('\0', sample(), nullptr); // EOF

    for (int li = (int)lines.size() - 1; li >= 0; --li) {
        if (li + 1 < (int)lines.size()) {
            // newline that separates this line from the next
            RopeBufferNode* nl = new RopeBufferNode('\n', sample(), nullptr);
            nl->right          = tail;
            tail->parent       = nl;
            tail               = nl;
        }
        const std::string& line = lines[li];
        for (int ci = (int)line.size() - 1; ci >= 0; --ci) {
            RopeBufferNode* node = new RopeBufferNode(line[ci], sample(), nullptr);
            node->right          = tail;
            tail->parent         = node;
            tail                 = node;
        }
    }

    root = tail; // leftmost node becomes root of right-skewed tree

    // Recompute subtree counts bottom-up via post-order DFS.
    std::function<void(RopeBufferNode*)> fix = [&](RopeBufferNode* n) {
        if (!n)
            return;
        fix(n->left);
        fix(n->right);
        n->num_chars = (n->left ? n->left->num_chars : 0) + (n->c != '\0' ? 1 : 0) +
                       (n->right ? n->right->num_chars : 0);
        n->num_newlines = (n->left ? n->left->num_newlines : 0) + (n->c == '\n' ? 1 : 0) +
                          (n->right ? n->right->num_newlines : 0);
    };
    fix(root);
}

std::string RopeBuffer::bufferTypeName() const {
    return "(Treap Based) Rope";
}

std::size_t RopeBuffer::fetchLines(const ICursor& ic, std::size_t above, std::size_t below,
                                   std::vector<std::string>& out) const {
    const auto& ac        = cur(ic);
    size_t      curr_line = absolute_lines(ac.node_ptr); // finds the current line

    std::size_t start = curr_line > above ? curr_line - above : 0;
    std::size_t end   = std::min(curr_line + below + 1, total_lines);

    out.clear();
    out.reserve(end - start);
    // now start walking from starth line to last line
    RopeBufferNode* start_it = find_start_of_line(start);
    RopeBufferNode* end_it   = find_start_of_line(end);

    // TODO: pray that succ works
    out.emplace_back();
    while (start_it && start_it != end_it) {
        if (start_it->is_newline()) {
            out.emplace_back();
        } else {
            out.back().push_back(start_it->c);
        }
        start_it = successor(start_it);
    }
    assert(start_it); // we should never hit null
    return start;
}

std::vector<std::string> RopeBuffer::getAllLines() const {
    std::vector<std::string> buffer;
    buffer.reserve(total_lines);
    RopeBufferNode* start_it = find_start_of_line(0);
    buffer.emplace_back();
    while (!start_it->is_eof()) {
        if (start_it->is_newline()) {
            buffer.emplace_back();
        } else {
            buffer.back().push_back(start_it->c);
        }
        start_it = successor(start_it);
    }

    return buffer;
}

void RopeBuffer::accept(BufferVisitor& v, const BufferVisitor::EditorCtx& ctx) const {
    v.visit(*this, ctx);
}
