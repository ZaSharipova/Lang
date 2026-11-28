#include "lexer.hpp"

#include <cctype>
#include <charconv>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace paracl::lexer {

static inline std::unordered_map<std::string, TokenType> g_kKeyWords = {
    { "if", TokenType::kKeyWordIf },
    { "else", TokenType::kKeyWordElse },
    { "while", TokenType::kKeyWordWhile },
    { "return", TokenType::kKeyWordReturn },
    { "print", TokenType::kKeyWordPrint },
};

static const char g_kOpenCommentChars[] = "/*";
static const char g_kCloseCommentChars[] = "*/";

namespace {

bool IsSpace(char character) {
    return std::isspace(static_cast<unsigned char>(character)) != 0;
}

bool IsDigit(char character) {
    return std::isdigit(static_cast<unsigned char>(character)) != 0;
}

bool IsAlpha(char character) {
    return (std::isalpha(static_cast<unsigned char>(character)) != 0) || character == '_';
}

bool IsAlnum(char character) {
    return IsAlpha(character) || IsDigit(character);
}

} // namespace anonymous

std::vector<char> SourceBuffer::MakeBufferFromFileData(const char* filepath) {
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error(std::string("file") + filepath + "didnt open"); // TODO
    }

    // Запромчено
    // ==================================
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    // ==================================

    std::vector<char> buffer(size + 1);

    if (size > 0) {
        if (!file.read(buffer.data(), size)) {
            throw std::runtime_error("file read error");
        }
    }

    buffer[size] = '\0';
    return buffer;
}

std::vector<char> SourceBuffer::MakeBufferFromString(const std::string& str) {
    std::vector<char> buffer(str.begin(), str.end());
    buffer.push_back('\0');

    return buffer;
}

int64_t Lexer::TokenToNum(Token token) const {
    int64_t value = 0;
    auto [ptr, ec] = std::from_chars(buffer_.data() + token.offset, buffer_.data() + token.offset + token.len, value);
    if (ec == std::errc { }) {
        return value;
    }

    return -1;
}

std::string Lexer::TokenToStr(Token token) const {
    std::string str(buffer_.data() + token.offset, token.len);
    return str;
}

void Lexer::SkipUselessText() {
    while (buffer_[cursor_] != '\0') {
        if (IsSpace(buffer_[cursor_]) != 0) {
            cursor_++;
            continue;
        }

        if (buffer_[cursor_] == '/' && buffer_[cursor_ + 1] == '/') {
            cursor_ += sizeof(g_kCloseCommentChars) - 1;
            while (buffer_[cursor_] != '\n' && buffer_[cursor_] != '\0') {
                cursor_++;
            }
            continue;
        }

        if (buffer_[cursor_] == g_kOpenCommentChars[0] && buffer_[cursor_ + 1] == g_kOpenCommentChars[1]) {
            cursor_ += sizeof(g_kOpenCommentChars) - 1;
            while (cursor_ < buffer_.size()) {
                if (buffer_[cursor_] == g_kCloseCommentChars[0] && buffer_[cursor_ + 1] == g_kCloseCommentChars[1]) {
                    cursor_ += sizeof(g_kCloseCommentChars) - 1;
                    break;
                }
                cursor_++;
            }
            continue;
        }

        break;
    }
}

Token Lexer::NextToken() {
    SkipUselessText();

    if (cursor_ == static_cast<std::size_t>(buffer_.size())) {
        return Token(TokenType::kEof, static_cast<uint32_t>(cursor_), 0);
    }

    if (static_cast<int>(IsAlpha(buffer_[cursor_])) != 0) {
        return LexIdentifierOrKeyword();
    }

    if (IsDigit(buffer_[cursor_]) != 0) {
        return LexNumber();
    }

    return LexPunctuation();
}

Token Lexer::LexIdentifierOrKeyword() {
    std::size_t start = cursor_;
    while (IsAlnum(buffer_[cursor_]) != 0) {
        cursor_++;
    }

    auto offset = static_cast<uint32_t>(start);
    auto len = static_cast<uint32_t>(cursor_ - start);

    std::string identifier(buffer_.data() + offset, len);
    TokenType type = TokenType::kIdentifier;
    if (g_kKeyWords.contains(identifier)) {
        type = g_kKeyWords[identifier];
    }

    return Token(type, offset, len);
}

Token Lexer::LexNumber() {
    std::size_t start = cursor_;
    while (IsDigit(buffer_[cursor_]) != 0) {
        cursor_++;
    }

    auto offset = static_cast<uint32_t>(start);
    auto len = static_cast<uint32_t>(cursor_ - start);

    if (IsAlpha(buffer_[cursor_]) != 0) {
        while (IsAlnum(buffer_[cursor_]) != 0) {
            cursor_++;
        }
        len = static_cast<uint32_t>(cursor_ - start);
        return Token(TokenType::kError, offset, len);
    }

    return Token(TokenType::kIntLiteral, offset, len);
}

Token Lexer::LexPunctuation() {
    uint32_t offset = cursor_;
    TokenType type = TokenType::kError;

    auto doesMatch = [this](char expected, TokenType matchType, TokenType defaultType) {
        if (buffer_[cursor_] == expected) {
            cursor_++;
            return matchType;
        }
        return defaultType;
    };

    char character = buffer_[cursor_++];
    switch (character) {
    case '?':
        type = TokenType::kQuestion;
        break;
    case '+':
        type = TokenType::kPlus;
        break;
    case '-':
        type = TokenType::kMinus;
        break;
    case '*':
        type = TokenType::kStar;
        break;
    case '/':
        type = TokenType::kSlash;
        break;
    case '%':
        type = TokenType::kPercent;
        break;
    case '(':
        type = TokenType::kLParen;
        break;
    case ')':
        type = TokenType::kRParen;
        break;
    case '{':
        type = TokenType::kLBrace;
        break;
    case '}':
        type = TokenType::kRBrace;
        break;
    case ';':
        type = TokenType::kSemicolon;
        break;
    case ',':
        type = TokenType::kComma;
        break;

    case '=':
        type = doesMatch('=', TokenType::kEqualEqual, TokenType::kEqual);
        break;
    case '!':
        type = doesMatch('=', TokenType::kNotEqual, TokenType::kError);
        break;
    case '<':
        type = doesMatch('=', TokenType::kLessEq, TokenType::kLess);
        break;
    case '>':
        type = doesMatch('=', TokenType::kGreatEq, TokenType::kGreat);
        break;

    default:
        break;
    }

    auto len = static_cast<uint32_t>(cursor_ - offset);

    return Token(type, offset, len);
}

std::vector<Token> Lexer::TokenizeAll() {
    std::vector<Token> res;
    while (true) {
        Token tok = NextToken();
        if (tok.type == TokenType::kError) {
            res.clear();
            PrintError(tok);
            return res;
        }
        res.push_back(tok);
        if (tok.type == TokenType::kEof) {
            break;
        }
    }

    return res;
}

void Lexer::PrintError(Token token) const {
    auto [line, col] = buffer_.GetLocation(token.offset);

    const char* bufStart = buffer_.data();
    const char* bufEnd = bufStart + buffer_.size();

    const char* lineStart = bufStart + token.offset - (col - 1);

    const char* lineEnd = lineStart;
    while (lineEnd < bufEnd && *lineEnd != '\n' && *lineEnd != '\r') {
        ++lineEnd;
    }

    std::string_view lineStr(lineStart, lineEnd - lineStart);
    std::string_view badToken(bufStart + token.offset, token.len);

    std::cerr << "Syntax error at " << line << ":" << col << " bad token '" << badToken << "'\n";

    std::string linePrefix = " " + std::to_string(line) + " | ";
    std::cerr << linePrefix << lineStr << "\n";

    std::cerr << std::string(linePrefix.size() - 2, ' ') << "| ";

    for (std::size_t i = 0; i < col - 1; ++i) {
        std::cerr << (lineStr[i] == '\t' ? '\t' : ' ');
    }

    std::cerr << '^';
    if (token.len > 1) {
        std::cerr << std::string(token.len - 1, '~');
    }
    std::cerr << "\n";
}

} // namespace paracl::lexer
