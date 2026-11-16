#pragma once

#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

#include "lexer.hpp"
#include "node.hpp"

/* G          ::= FUNCTION_D+
   FUNCTION_D ::= V "(" [ E {"," E}* ] ")" "{" OP* "}"

   OP         ::= RETURN | SCANF | PRINTF | WHILE | IF | CALL

   RETURN     ::= "return" E ";"
   SCANF      ::= "read" "(" V ")" ";"
   PRINTF     ::= "print" "(" (V | N) ")" ";"
   CALL       ::= V "(" E [ E {"," E}* ] ")" ";"
   WHILE      ::= "while" "(" CONDITION ")" BODY +
   IF         ::= "if" "(" CONDITION ")" BODY [ "else" BODY ] +
   CONDITION  ::= E [ ("<=" | "<" | ">=" | ">" | "==" | "!=") E ]
   BODY       ::= "{" OP* "}"

   E          ::= T ([+-] T)* +
   T          ::= P ([*:/%] P)* +
   P          ::= '(' E ')' | CALL | V | N

   V (Identifier) ::= Variable
   N (IntLiteral) ::= Number
*/

namespace paracl::parser {

using NodePtr = std::unique_ptr<INode>;

class Parser {
public:
    Parser(const lexer::Lexer* lexer, std::vector<lexer::Token> tokens)
        : lexer_(lexer)
        , tokens_(std::move(tokens)) { }

    NodePtr Parse();

private:
    enum class FunctionCallModes {
        kWithSemicolon,
        kWithoutSemicolon,
    };

    std::unique_ptr<FunctionNode> GetFunctionDeclare();
    NodePtr GetFunctionCall(FunctionCallModes mode);
    NodePtr GetWhile();
    NodePtr GetIf();
    NodePtr GetElse();
    NodePtr GetCondition();
    NodePtr GetBody();
    NodePtr GetExpression();
    NodePtr GetTerm();
    NodePtr GetPrimary();
    std::unique_ptr<VariableNode> GetIdentifier();
    NodePtr GetIntLiteral();
    NodePtr GetReturn();
    NodePtr GetScan();
    NodePtr GetPrint();
    NodePtr GetOp();
    NodePtr GetAssign();

    static bool IsComparisonSign(lexer::Token token);
    static OpType ToOpType(lexer::Token token);

    lexer::Token GetToken() const;
    lexer::Token SeeNextToken() const;
    lexer::Token GetNextToken();
    bool IsThatTokenType(lexer::TokenType type);
    void IncreaseTokensPos();
    void ReportError(std::string_view error_message) const;

    const lexer::Lexer* lexer_ = nullptr;
    std::vector<lexer::Token> tokens_;
    std::size_t pos_ = 0;
};

} // namespace paracl::parser
