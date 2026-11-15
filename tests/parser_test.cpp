#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "lexer.hpp"
#include "node.hpp"
#include "parser.hpp"

namespace paracl::parser::tests {

namespace {

std::unique_ptr<ProgramNode> ParseProgram(const std::string& code) {
    auto buffer = lexer::SourceBuffer::FromString(code);
    lexer::Lexer lexer(buffer);
    auto tokens = lexer.TokenizeAll();

    Parser parser(&lexer, std::move(tokens));
    NodePtr root = parser.Parse();
    if (!root) {
        return nullptr;
    }

    auto* prog = dynamic_cast<ProgramNode*>(root.get());
    if (!prog) {
        return nullptr;
    }
    root.release();
    return std::unique_ptr<ProgramNode>(prog);
}

template <typename NodeT>
const NodeT* As(const INode* node) {
    return dynamic_cast<const NodeT*>(node);
}

} // namespace

TEST(ParserTest, FunctionDeclarationsAndParams) {
    auto prog = ParseProgram(R"(
        empty() {}
        single(x) { return x; }
        multi(a, b, c) { return a; }
    )");

    ASSERT_NE(prog, nullptr);
    const auto& funcs = prog->GetFunctions();
    ASSERT_EQ(funcs.size(), 3u);

    const auto* fn0 = As<FunctionNode>(funcs[0].get());
    ASSERT_NE(fn0, nullptr);
    EXPECT_EQ(fn0->GetName(), "empty");
    EXPECT_TRUE(fn0->GetParams().empty());
    const auto* body0 = As<ScopeNode>(fn0->GetBody());
    ASSERT_NE(body0, nullptr);
    EXPECT_TRUE(body0->GetStatements().empty());

    const auto* fn1 = As<FunctionNode>(funcs[1].get());
    ASSERT_NE(fn1, nullptr);
    EXPECT_EQ(fn1->GetName(), "single");
    EXPECT_EQ(fn1->GetParams(), (std::vector<std::string>{"x"}));

    const auto* fn2 = As<FunctionNode>(funcs[2].get());
    ASSERT_NE(fn2, nullptr);
    EXPECT_EQ(fn2->GetName(), "multi");
    EXPECT_EQ(fn2->GetParams(), (std::vector<std::string>{"a", "b", "c"}));
}

TEST(ParserTest, OperatorPrecedence) {
    auto prog = ParseProgram("main() { return 1 + 2 * 3 - 8 / 4 * 2; }");
    ASSERT_NE(prog, nullptr);

    const auto* fn = As<FunctionNode>(prog->GetFunctions()[0].get());
    ASSERT_NE(fn, nullptr);
    const auto* body = As<ScopeNode>(fn->GetBody());
    ASSERT_NE(body, nullptr);
    ASSERT_EQ(body->GetStatements().size(), 1u);

    const auto* ret = As<ReturnNode>(body->GetStatements()[0].get());
    ASSERT_NE(ret, nullptr);

    const auto* sub = As<OpNode>(ret->GetExpr());
    ASSERT_NE(sub, nullptr);
    EXPECT_EQ(sub->GetType(), OpType::kSub);

    const auto* add = As<OpNode>(sub->GetLeft());
    ASSERT_NE(add, nullptr);
    EXPECT_EQ(add->GetType(), OpType::kAdd);

    const auto* val1 = As<ValueNode>(add->GetLeft());
    ASSERT_NE(val1, nullptr);
    EXPECT_EQ(val1->GetValue(), 1u);

    const auto* mul1 = As<OpNode>(add->GetRight());
    ASSERT_NE(mul1, nullptr);
    EXPECT_EQ(mul1->GetType(), OpType::kMul);
    EXPECT_EQ(As<ValueNode>(mul1->GetLeft())->GetValue(), 2u);
    EXPECT_EQ(As<ValueNode>(mul1->GetRight())->GetValue(), 3u);

    const auto* mul2 = As<OpNode>(sub->GetRight());
    ASSERT_NE(mul2, nullptr);
    EXPECT_EQ(mul2->GetType(), OpType::kMul);
    EXPECT_EQ(As<ValueNode>(mul2->GetRight())->GetValue(), 2u);

    const auto* div = As<OpNode>(mul2->GetLeft());
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->GetType(), OpType::kDiv);
    EXPECT_EQ(As<ValueNode>(div->GetLeft())->GetValue(), 8u);
    EXPECT_EQ(As<ValueNode>(div->GetRight())->GetValue(), 4u);
}

TEST(ParserTest, ParenthesesOverridePrecedence) {
    auto prog = ParseProgram("main() { return (1 + 2) * (5 - 3); }");
    ASSERT_NE(prog, nullptr);

    const auto* fn = As<FunctionNode>(prog->GetFunctions()[0].get());
    const auto* body = As<ScopeNode>(fn->GetBody());
    const auto* ret = As<ReturnNode>(body->GetStatements()[0].get());

    const auto* mul = As<OpNode>(ret->GetExpr());
    ASSERT_NE(mul, nullptr);
    EXPECT_EQ(mul->GetType(), OpType::kMul);

    const auto* add = As<OpNode>(mul->GetLeft());
    ASSERT_NE(add, nullptr);
    EXPECT_EQ(add->GetType(), OpType::kAdd);
    EXPECT_EQ(As<ValueNode>(add->GetLeft())->GetValue(), 1u);
    EXPECT_EQ(As<ValueNode>(add->GetRight())->GetValue(), 2u);

    const auto* sub = As<OpNode>(mul->GetRight());
    ASSERT_NE(sub, nullptr);
    EXPECT_EQ(sub->GetType(), OpType::kSub);
    EXPECT_EQ(As<ValueNode>(sub->GetLeft())->GetValue(), 5u);
    EXPECT_EQ(As<ValueNode>(sub->GetRight())->GetValue(), 3u);
}

TEST(ParserTest, LeftAssociativity) {
    auto prog = ParseProgram("main() { return 10 - 3 - 2; }");
    ASSERT_NE(prog, nullptr);

    const auto* fn = As<FunctionNode>(prog->GetFunctions()[0].get());
    const auto* body = As<ScopeNode>(fn->GetBody());
    const auto* ret = As<ReturnNode>(body->GetStatements()[0].get());

    const auto* outer_sub = As<OpNode>(ret->GetExpr());
    ASSERT_NE(outer_sub, nullptr);
    EXPECT_EQ(outer_sub->GetType(), OpType::kSub);

    const auto* right_val = As<ValueNode>(outer_sub->GetRight());
    ASSERT_NE(right_val, nullptr);
    EXPECT_EQ(right_val->GetValue(), 2u);

    const auto* inner_sub = As<OpNode>(outer_sub->GetLeft());
    ASSERT_NE(inner_sub, nullptr);
    EXPECT_EQ(inner_sub->GetType(), OpType::kSub);
    EXPECT_EQ(As<ValueNode>(inner_sub->GetLeft())->GetValue(), 10u);
    EXPECT_EQ(As<ValueNode>(inner_sub->GetRight())->GetValue(), 3u);
}

TEST(ParserTest, AssignAndFunctionCalls) {
    auto prog = ParseProgram(R"(
        foo(a, b) { return a / b; }
        main() {
            x = foo(10, 3 + 1);
            foo(x, 2);
            return x;
        }
    )");
    ASSERT_NE(prog, nullptr);
    ASSERT_EQ(prog->GetFunctions().size(), 2u);

    const auto* main_fn = As<FunctionNode>(prog->GetFunctions()[1].get());
    const auto* body = As<ScopeNode>(main_fn->GetBody());
    const auto& stmts = body->GetStatements();
    ASSERT_EQ(stmts.size(), 3u);

    const auto* assign = As<AssignNode>(stmts[0].get());
    ASSERT_NE(assign, nullptr);
    EXPECT_EQ(assign->GetName(), "x");

    const auto* call_in_expr = As<CallNode>(assign->GetExpr());
    ASSERT_NE(call_in_expr, nullptr);
    EXPECT_EQ(call_in_expr->GetName(), "foo");
    ASSERT_EQ(call_in_expr->GetArgs().size(), 2u);
    EXPECT_NE(As<ValueNode>(call_in_expr->GetArgs()[0].get()), nullptr);
    EXPECT_NE(As<OpNode>(call_in_expr->GetArgs()[1].get()), nullptr);

    const auto* standalone_call = As<CallNode>(stmts[1].get());
    ASSERT_NE(standalone_call, nullptr);
    EXPECT_EQ(standalone_call->GetName(), "foo");
    ASSERT_EQ(standalone_call->GetArgs().size(), 2u);
    EXPECT_EQ(As<VariableNode>(standalone_call->GetArgs()[0].get())->GetVariable(), "x");
}

TEST(ParserTest, IfElseStatement) {
    auto prog = ParseProgram(R"(
        main() {
            if (a <= b) {
                print(a);
            } else {
                print(b);
            }
            if (a == 0) {
                return 1;
            }
        }
    )");
    ASSERT_NE(prog, nullptr);

    const auto* fn = As<FunctionNode>(prog->GetFunctions()[0].get());
    const auto* body = As<ScopeNode>(fn->GetBody());
    ASSERT_EQ(body->GetStatements().size(), 2u);

    const auto* if_else = As<IfNode>(body->GetStatements()[0].get());
    ASSERT_NE(if_else, nullptr);

    const auto* cond = As<OpNode>(if_else->GetCondition());
    ASSERT_NE(cond, nullptr);
    EXPECT_EQ(cond->GetType(), OpType::kLE);

    const auto* then_scope = As<ScopeNode>(if_else->GetBody());
    ASSERT_NE(then_scope, nullptr);
    ASSERT_EQ(then_scope->GetStatements().size(), 1u);
    const auto* print_a = As<PrintNode>(then_scope->GetStatements()[0].get());
    ASSERT_NE(print_a, nullptr);
    EXPECT_EQ(As<VariableNode>(print_a->GetExpr())->GetVariable(), "a");

    const auto* else_scope = As<ScopeNode>(if_else->GetElse());
    ASSERT_NE(else_scope, nullptr);
    ASSERT_EQ(else_scope->GetStatements().size(), 1u);
    const auto* print_b = As<PrintNode>(else_scope->GetStatements()[0].get());
    ASSERT_NE(print_b, nullptr);
    EXPECT_EQ(As<VariableNode>(print_b->GetExpr())->GetVariable(), "b");

    const auto* if_only = As<IfNode>(body->GetStatements()[1].get());
    ASSERT_NE(if_only, nullptr);
    EXPECT_EQ(if_only->GetElse(), nullptr);
    EXPECT_EQ(As<OpNode>(if_only->GetCondition())->GetType(), OpType::kEq);
}

TEST(ParserTest, WhileAndComparisons) {
    auto prog = ParseProgram(R"(
        main() {
            while (i != 10) {
                if (i < 5) { i = i + 1; }
                if (i > 5) { i = i + 2; }
                if (i >= 8) { return i; }
            }
        }
    )");
    ASSERT_NE(prog, nullptr);

    const auto* fn = As<FunctionNode>(prog->GetFunctions()[0].get());
    const auto* body = As<ScopeNode>(fn->GetBody());
    ASSERT_EQ(body->GetStatements().size(), 1u);

    const auto* while_node = As<WhileNode>(body->GetStatements()[0].get());
    ASSERT_NE(while_node, nullptr);
    EXPECT_EQ(As<OpNode>(while_node->GetCondition())->GetType(), OpType::kNe);

    const auto* while_body = As<ScopeNode>(while_node->GetBody());
    ASSERT_NE(while_body, nullptr);
    ASSERT_EQ(while_body->GetStatements().size(), 3u);

    EXPECT_EQ(As<OpNode>(As<IfNode>(while_body->GetStatements()[0].get())->GetCondition())->GetType(), OpType::kL);
    EXPECT_EQ(As<OpNode>(As<IfNode>(while_body->GetStatements()[1].get())->GetCondition())->GetType(), OpType::kG);
    EXPECT_EQ(As<OpNode>(As<IfNode>(while_body->GetStatements()[2].get())->GetCondition())->GetType(), OpType::kGE);
}

TEST(ParserTest, SyntaxErrors) {
    const std::vector<std::string> invalid_inputs = {
        "main() { return 1 }",
        "main( { return 1; }",
        "main() { return 1;",
        "main() { if (x < 1) x = 2; }",
        "main() { while () { x = 1; } }",
        "main() { x = 1 + * 2; }",
        "main() { x = (1 + 2; }",
        "main(123) { return 0; }",
        "main() { else { return 0; } }",
        "main() { 123 = x; }"
    };

    for (const auto& bad_code : invalid_inputs) {
        EXPECT_EQ(ParseProgram(bad_code), nullptr);
    }
}

} // namespace paracl::parser::tests