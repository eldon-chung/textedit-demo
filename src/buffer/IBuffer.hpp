#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

class IBuffer {
public:
    virtual ~IBuffer() = default;

    virtual std::size_t lineCount() const = 0;
    virtual std::size_t lineLength(std::size_t row) const = 0;
    virtual char charAt(std::size_t row, std::size_t col) const = 0;
    virtual std::string getLine(std::size_t row) const = 0;

    virtual void insertChar(std::size_t row, std::size_t col, char ch) = 0;
    virtual void deleteChar(std::size_t row, std::size_t col) = 0;  // forward delete
    virtual void splitLine(std::size_t row, std::size_t col) = 0;   // Enter
    virtual void joinLines(std::size_t row) = 0;                    // backspace at col 0

    virtual void loadFromLines(const std::vector<std::string>& lines) = 0;
    virtual std::vector<std::string> getAllLines() const = 0;           // copy — for viz snapshot
    virtual std::vector<std::string_view> getLinesView() const = 0;    // zero-copy — caller must hold mutex

    virtual std::string bufferTypeName() const = 0;
};
