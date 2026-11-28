#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace paracl::lexer {

enum class TokenType : uint32_t { // NOLINT чётатам хочется 4 байта
    kKeyWordIf = 0,
    kKeyWordElse,
    kKeyWordWhile,
    kKeyWordReturn,
    kKeyWordScan,
    kKeyWordPrint,

    kEof,
    kError,

    kIdentifier,
    kIntLiteral,

    kQuestion,

    kPlus,
    kMinus,
    kStar,
    kSlash,
    kPercent,

    kEqual,
    kEqualEqual,
    kNotEqual,
    kLess,
    kGreat,
    kLessEq,
    kGreatEq,

    kLParen,
    kRParen,
    kLBrace,
    kRBrace,

    kSemicolon,
    kComma,
};

struct Token {
    TokenType type;
    uint32_t offset;
    uint32_t len;
};

class SourceBuffer {
public:
    static SourceBuffer FromFile(const char* filepath) {
        return SourceBuffer(MakeBufferFromFileData(filepath));
    }

    static SourceBuffer FromString(const std::string& str) {
        return SourceBuffer(MakeBufferFromString(str));
    }

    const char* data() const { return buffer_.data(); }
    uint32_t size() const { return static_cast<uint32_t>(buffer_.size() - 1); }

    const char& operator[](std::size_t index) const { return buffer_[index]; }
    char& operator[](std::size_t index) { return buffer_[index]; }

    std::pair<std::size_t, std::size_t> GetLocation(uint32_t offset) const {
        if (newLines_.empty()) {
            return { 1, 1 };
        }

        auto iterator = std::ranges::upper_bound(newLines_, offset);
        std::size_t lineIdx = std::distance(newLines_.begin(), iterator);

        return { lineIdx, offset - newLines_[lineIdx - 1] + 1 };
    }

private:
    explicit SourceBuffer(std::vector<char> buffer)
        : buffer_ { std::move(buffer) } {
        BuildNewLines();
    }

    static std::vector<char> MakeBufferFromFileData(const char* filepath); // yes it can throw exception, but other solutions are even worse
    static std::vector<char> MakeBufferFromString(const std::string& str);

    void BuildNewLines() {
        newLines_.clear();
        newLines_.push_back(0);

        for (std::size_t i = 0; i < buffer_.size(); ++i) {
            if (buffer_[i] == '\n') {
                newLines_.push_back(static_cast<uint32_t>(i + 1));
            }
        }
    }

    std::vector<char> buffer_;
    std::vector<uint32_t> newLines_;
};

class Lexer {
public:
    explicit Lexer(const char* filepath)
        : buffer_(SourceBuffer::FromFile(filepath)) { }

    explicit Lexer(const std::string& name)
        : buffer_(SourceBuffer::FromString(name)) { }

    explicit Lexer(SourceBuffer source)
        : buffer_(std::move(source)) { }

    int64_t TokenToNum(Token token) const;
    std::string TokenToStr(Token token) const;
    void PrintError(Token token) const;

    std::vector<Token> TokenizeAll();

private:
    void SkipUselessText();
    Token NextToken();
    Token LexIdentifierOrKeyword();
    Token LexNumber();
    Token LexPunctuation();

    SourceBuffer buffer_;
    std::size_t cursor_ = 0;
};

} // namespace paracl::lexer
