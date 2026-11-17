#include <gtest/gtest.h>
#include <cstddef>
#include <vector>
#include <string>
#include <utility>

#include "lexer.hpp"


namespace paracl::lexer::tests {

namespace {
    
void ExpectTokenTypes(const std::string& code, const std::vector<TokenType>& expected) {
    auto buffer = SourceBuffer::FromString(code);
    Lexer lexer(buffer);
    auto tokens = lexer.TokenizeAll();

    ASSERT_EQ(tokens.size(), expected.size()) << "Code: " << code;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(tokens[i].type, expected[i]) << "Index: " << i;
    }
}

} // namespace anonymous

TEST(LexerTest, EmptyAndWhitespaces) {
    ExpectTokenTypes("", {TokenType::kEof});
    ExpectTokenTypes("   \t\n  ", {TokenType::kEof});
}

TEST(LexerTest, KeywordsAndIdentifiers) {
    ExpectTokenTypes(
        "if else while return print",
        {
            TokenType::kKeyWordIf,
            TokenType::kKeyWordElse,
            TokenType::kKeyWordWhile,
            TokenType::kKeyWordReturn,
            TokenType::kKeyWordPrint,
            TokenType::kEof
        }
    );

    std::string code = "ifelse while1 _return print_var myVar123";
    auto buffer = SourceBuffer::FromString(code);
    Lexer lexer(buffer);
    auto tokens = lexer.TokenizeAll();

    ASSERT_EQ(tokens.size(), 6);
    for (std::size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(tokens[i].type, TokenType::kIdentifier);
    }
    EXPECT_EQ(lexer.TokenToStr(tokens[0]), "ifelse");
    EXPECT_EQ(lexer.TokenToStr(tokens[1]), "while1");
    EXPECT_EQ(lexer.TokenToStr(tokens[2]), "_return");
    EXPECT_EQ(lexer.TokenToStr(tokens[3]), "print_var");
    EXPECT_EQ(lexer.TokenToStr(tokens[4]), "myVar123");
}

TEST(LexerTest, IntegerLiterals) {
    std::string code = "0 42 1234567890";
    auto buffer = SourceBuffer::FromString(code);
    Lexer lexer(buffer);
    auto tokens = lexer.TokenizeAll();

    ASSERT_EQ(tokens.size(), 4);
    
    EXPECT_EQ(tokens[0].type, TokenType::kIntLiteral);
    EXPECT_EQ(lexer.TokenToNum(tokens[0]), 0);

    EXPECT_EQ(tokens[1].type, TokenType::kIntLiteral);
    EXPECT_EQ(lexer.TokenToNum(tokens[1]), 42);

    EXPECT_EQ(tokens[2].type, TokenType::kIntLiteral);
    EXPECT_EQ(lexer.TokenToNum(tokens[2]), 1234567890LL);
}

TEST(LexerTest, OperatorsAndPunctuation) {
    ExpectTokenTypes(
        "+ - * / % ? ( ) { } ;",
        {
            TokenType::kPlus,
            TokenType::kMinus,
            TokenType::kStar,
            TokenType::kSlash,
            TokenType::kPercent,
            TokenType::kQuestion,
            TokenType::kLParen,
            TokenType::kRParen,
            TokenType::kLBrace,
            TokenType::kRBrace,
            TokenType::kSemicolon,
            TokenType::kEof
        }
    );
}

TEST(LexerTest, ComparisonAndAssignmentOperators) {
    ExpectTokenTypes(
        "= == != < > <= >=",
        {
            TokenType::kEqual,
            TokenType::kEqualEqual,
            TokenType::kNotEqual,
            TokenType::kLess,
            TokenType::kGreat,
            TokenType::kLessEq,
            TokenType::kGreatEq,
            TokenType::kEof
        }
    );

    ExpectTokenTypes(
        "===!=<=>=",
        {
            TokenType::kEqualEqual,
            TokenType::kEqual,
            TokenType::kNotEqual,
            TokenType::kLessEq,
            TokenType::kGreatEq,
            TokenType::kEof
        }
    );
}

TEST(LexerTest, NoWhitespaceExpression) {
    std::string code = "x=?;if(x>=10){print(x+1);}";
    ExpectTokenTypes(
        code,
        {
            TokenType::kIdentifier, // x
            TokenType::kEqual,      // =
            TokenType::kQuestion,   // ?
            TokenType::kSemicolon,  // ;
            TokenType::kKeyWordIf,  // if
            TokenType::kLParen,     // (
            TokenType::kIdentifier, // x
            TokenType::kGreatEq,    // >=
            TokenType::kIntLiteral, // 10
            TokenType::kRParen,     // )
            TokenType::kLBrace,     // {
            TokenType::kKeyWordPrint,// print
            TokenType::kLParen,     // (
            TokenType::kIdentifier, // x
            TokenType::kPlus,       // +
            TokenType::kIntLiteral, // 1
            TokenType::kRParen,     // )
            TokenType::kSemicolon,  // ;
            TokenType::kRBrace,     // }
            TokenType::kEof
        }
    );
}

TEST(LexerTest, SourceBufferLocationsAndOffsets) {
    std::string code = "int a = 5;\nwhile (a > 0) {\n    a = a - 1;\n}";
    auto buffer = SourceBuffer::FromString(code);
    Lexer lexer(buffer);
    auto tokens = lexer.TokenizeAll();

    EXPECT_EQ(tokens[0].offset, 0u);
    EXPECT_EQ(tokens[0].len, 3u);
    EXPECT_EQ(buffer.GetLocation(tokens[0].offset), std::make_pair(1uz, 1uz));

    EXPECT_EQ(lexer.TokenToStr(tokens[5]), "while");
    EXPECT_EQ(buffer.GetLocation(tokens[5].offset), std::make_pair(2uz, 1uz));

    EXPECT_EQ(lexer.TokenToStr(tokens[12]), "a");
    EXPECT_EQ(buffer.GetLocation(tokens[12].offset), std::make_pair(3uz, 5uz));
}

} // namespace paracl::lexer::tests