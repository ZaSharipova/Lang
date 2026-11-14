#include "lexer.hpp"

#include <iostream>
#include <fstream>
#include <vector>
#include <charconv>
#include <cctype>
#include <unordered_map>
#include <string>
#include <string_view>

namespace paracl::lexer {

static inline std::unordered_map<std::string, TokenType> g_kKeyWords = {
    {"if", TokenType::kKeyWordIf},
    {"else", TokenType::kKeyWordElse},
    {"while", TokenType::kKeyWordWhile},
    {"return", TokenType::kKeyWordReturn},
    {"print", TokenType::kKeyWordPrint},
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

SourceBuffer SourceBuffer::FromFile(const char* filepath) {
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error(std::string("file") + filepath + "didnt open");
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

    SourceBuffer sourceBuf(buffer);
    sourceBuf.BuildNewLines();

    return sourceBuf;
}

SourceBuffer SourceBuffer::FromString(const std::string& str) {
    std::vector<char> buffer(str.begin(), str.end());
    buffer.push_back('\0');

    SourceBuffer sourceBuf(buffer);
    sourceBuf.BuildNewLines();

    return sourceBuf;
}

int64_t Lexer::TokenToNum(Token token) const {
    int64_t value = 0;
    auto [ptr, ec] = std::from_chars(buffer_.data() + token.offset, buffer_.data() + token.offset + token.len, value);
    if (ec == std::errc{}) {
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

    if (IsAlpha(buffer_[cursor_]) != 0) {
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
    uint32_t len = 1;
    TokenType type = TokenType::kError;

    char character = buffer_[cursor_++];
    switch (character) {
        case '?': type = TokenType::kQuestion; break;
        case '+': type = TokenType::kPlus; break;
        case '-': type = TokenType::kMinus; break;
        case '*': type = TokenType::kStar; break;
        case '/': type = TokenType::kSlash; break;
        case '%': type = TokenType::kPercent; break;
        case '(': type = TokenType::kLParen; break;
        case ')': type = TokenType::kRParen; break;
        case '{': type = TokenType::kLBrace; break;
        case '}': type = TokenType::kRBrace; break;
        case ';': type = TokenType::kSemicolon; break;

        case '=': 
            if (buffer_[cursor_] == '=') {
                type = TokenType::kEqualEqual;
                len = 2;
                cursor_++;
                break;
            }
            type = TokenType::kEqual;
            break;
        
        case '!': 
            if (buffer_[cursor_] == '=') {
                type = TokenType::kNotEqual;
                len = 2;
                cursor_++;
                break;
            }
            type = TokenType::kError;
            break;

        case '<':
            if (buffer_[cursor_] == '=') {
                type = TokenType::kLessEq;
                len = 2;
                cursor_++;
                break;
            }
            type = TokenType::kLess;
            break;

        case '>':
            if (buffer_[cursor_] == '=') {
                type = TokenType::kGreatEq;
                len = 2;
                cursor_++;
                break;
            }
            type = TokenType::kGreat;
            break;

        default: break;
    }

    return Token(type, offset, len);
}

std::vector<Token> Lexer::TokenizeAll() {
    std::vector<Token> res;
    while (true) {
        Token tok = NextToken();
        if (tok.type == TokenType::kError) {
            res.clear();
            auto [line, col] = buffer_.GetLocation(tok.offset);
            std::cerr << "Token fignya\n" << line << ":" << col << ": " << std::string_view(buffer_.data() + tok.offset, tok.len) << '\n';;
            return res;
        }
        res.push_back(tok);
        if (tok.type == TokenType::kEof) {
            break;
        }
    }

    return res;
}

} // namespace paracl::lexer