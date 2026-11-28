#pragma once

#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

#include "lexer.hpp"
#include "node.hpp"
#include "name_checker.hpp"

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
    explicit Parser(const char* filepath)
            : lexer_(std::make_unique<lexer::Lexer>(filepath)) {
            tokens_ = lexer_->TokenizeAll();
    }

    explicit Parser(const std::string& name)
            : lexer_(std::make_unique<lexer::Lexer>(name)) {
            tokens_ = lexer_->TokenizeAll();
    }

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
    void ReportErrorAt(std::string_view message, std::optional<lexer::Token> token);
    void ReportError(std::string_view error_message);
    void ReportCheckerError();

    std::unique_ptr<lexer::Lexer> lexer_;
    std::vector<lexer::Token> tokens_;
    std::size_t pos_ = 0;
    bool reported_ = false;
    NameChecker checker_{};
};

} // namespace paracl::parser
