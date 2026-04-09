#pragma once

#include "IBuffer.hpp"
#include <vector>
#include <string>

class ArrayBuffer : public IBuffer {
public:
    ArrayBuffer();

    std::size_t lineCount() const override;
    std::size_t lineLength(std::size_t row) const override;
    char charAt(std::size_t row, std::size_t col) const override;
    std::string getLine(std::size_t row) const override;

    void insertChar(std::size_t row, std::size_t col, char ch) override;
    void deleteChar(std::size_t row, std::size_t col) override;
    void splitLine(std::size_t row, std::size_t col) override;
    void joinLines(std::size_t row) override;

    void loadFromLines(const std::vector<std::string>& lines) override;
    std::vector<std::string> getAllLines() const override;

    std::string bufferTypeName() const override;

private:
    std::vector<std::string> lines_;
};
