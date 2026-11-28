#include <gtest/gtest.h>

#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "interpreter.hpp"
#include "node.hpp"

using namespace paracl;
using namespace paracl::interpreter;

namespace {

template <class... Nodes>
std::vector<NodePtr> MakeNodeList(Nodes&&... nodes) {
    std::vector<NodePtr> node_list;
    (node_list.push_back(std::forward<Nodes>(nodes)), ...);
    return node_list;
}

auto MakeValue(uint64_t value) {
    return std::make_unique<ValueNode>(value);
}

auto MakeVariable(const std::string& name) {
    return std::make_unique<VariableNode>(name);
}

auto MakeOp(OpType type, NodePtr left, NodePtr right) {
    return std::make_unique<OpNode>(type, std::move(left), std::move(right));
}

// ValueNode is unsigned, so negative literals are built as (0 - |value|).
NodePtr MakeSigned(int64_t value) {
    if (value >= 0) {
        return MakeValue(static_cast<uint64_t>(value));
    }
    return MakeOp(OpType::kSub, MakeValue(0), MakeValue(static_cast<uint64_t>(-value)));
}

// The only runtime error the interpreter still detects on its own is division by zero.
// Undefined names, wrong call arity, etc. are rejected by the parser.
NodePtr MakeFail() {
    return MakeOp(OpType::kDiv, MakeValue(1), MakeValue(0));
}

auto MakeAssign(const std::string& name, NodePtr expression) {
    return std::make_unique<AssignNode>(name, std::move(expression));
}

auto MakePrint(NodePtr expression) {
    return std::make_unique<PrintNode>(std::move(expression));
}

auto MakeScan(const std::string& name) {
    return std::make_unique<ScanNode>(MakeVariable(name));
}

auto MakeReturn(NodePtr expression = nullptr) {
    return std::make_unique<ReturnNode>(std::move(expression));
}

auto MakeIf(NodePtr condition, NodePtr body, NodePtr else_body = nullptr) {
    return std::make_unique<IfNode>(std::move(condition), std::move(body), std::move(else_body));
}

auto MakeWhile(NodePtr condition, NodePtr body) {
    return std::make_unique<WhileNode>(std::move(condition), std::move(body));
}

template <class... Statements>
std::unique_ptr<ScopeNode> MakeScope(Statements&&... statements) {
    auto scope = std::make_unique<ScopeNode>();
    (scope->AddStatement(std::forward<Statements>(statements)), ...);
    return scope;
}

template <class... Arguments>
auto MakeCall(const std::string& function_name, Arguments&&... arguments) {
    return std::make_unique<CallNode>(function_name,
        MakeNodeList(std::forward<Arguments>(arguments)...));
}

std::unique_ptr<FunctionNode> MakeFunction(const std::string& function_name,
    std::vector<std::string> parameters,
    NodePtr body) {
    return std::make_unique<FunctionNode>(function_name, std::move(parameters), std::move(body));
}

class InterpreterTest : public ::testing::Test {
protected:
    void SetUp() override {
        original_cout_buffer_ = std::cout.rdbuf(output_stream_.rdbuf());
        original_cin_buffer_ = std::cin.rdbuf(input_stream_.rdbuf());
        std::cin.clear();
    }

    void TearDown() override {
        std::cout.rdbuf(original_cout_buffer_);
        std::cin.rdbuf(original_cin_buffer_);
    }

    void SetInput(const std::string& input_text) {
        input_stream_.str(input_text);
        input_stream_.clear();
    }

    std::string Output() const {
        return output_stream_.str();
    }

    bool OutputContains(const std::string& expected_text) const {
        return Output().find(expected_text) != std::string::npos;
    }

    template <class... Functions>
    void BuildProgram(Functions&&... functions) {
        program_ = std::make_unique<ProgramNode>();
        (program_->AddFunction(std::forward<Functions>(functions)), ...);
    }

    template <class... Functions>
    Status RunProgram(Functions&&... functions) {
        BuildProgram(std::forward<Functions>(functions)...);
        return interpreter_.RunInterpreter(*program_) ? Status::kOk : Status::kError;
    }

    template <class... Functions>
    bool RunWithInterpreterEntryPoint(Functions&&... functions) {
        BuildProgram(std::forward<Functions>(functions)...);
        return interpreter_.RunInterpreter(*program_);
    }

    template <class... Statements>
    Status RunMain(Statements&&... statements) {
        return RunProgram(
            MakeFunction("main", { }, MakeScope(std::forward<Statements>(statements)...)));
    }

    Interpreter interpreter_;
    std::unique_ptr<ProgramNode> program_;

private:
    std::ostringstream output_stream_;
    std::istringstream input_stream_;
    std::streambuf* original_cout_buffer_ = nullptr;
    std::streambuf* original_cin_buffer_ = nullptr;
};

TEST_F(InterpreterTest, ValuePrinted) {
    EXPECT_EQ(RunMain(MakePrint(MakeValue(42))), Status::kOk);
    EXPECT_EQ(Output(), "42");
}

TEST_F(InterpreterTest, DefinedVariableReadable) {
    EXPECT_EQ(RunMain(MakeAssign("x", MakeValue(7)), MakePrint(MakeVariable("x"))), Status::kOk);
    EXPECT_EQ(Output(), "7");
}

struct OpTestCase {
    OpType type;
    int64_t left;
    int64_t right;
    int64_t expected;
    const char* name;
};

class OpTest : public InterpreterTest, public ::testing::WithParamInterface<OpTestCase> { };

TEST_P(OpTest, Evaluates) {
    const OpTestCase& test_case = GetParam();
    const Status status = RunMain(
        MakePrint(MakeOp(test_case.type, MakeSigned(test_case.left), MakeSigned(test_case.right))));
    ASSERT_EQ(status, Status::kOk);
    EXPECT_EQ(Output(), std::to_string(test_case.expected));
}

INSTANTIATE_TEST_SUITE_P(
    AllOps, OpTest,
    ::testing::Values(
        OpTestCase { OpType::kAdd, 2, 3, 5, "Add" },
        OpTestCase { OpType::kAdd, -2, 3, 1, "AddNegative" },
        OpTestCase { OpType::kSub, 2, 5, -3, "Sub" },
        OpTestCase { OpType::kMul, 4, 5, 20, "Mul" },
        OpTestCase { OpType::kMul, 4, 0, 0, "MulZero" },
        OpTestCase { OpType::kDiv, 10, 3, 3, "Div" },
        OpTestCase { OpType::kDiv, -9, 3, -3, "DivNegative" },
        OpTestCase { OpType::kMod, 10, 3, 1, "Mod" },
        OpTestCase { OpType::kL, 1, 2, 1, "LessTrue" },
        OpTestCase { OpType::kL, 2, 2, 0, "LessFalse" },
        OpTestCase { OpType::kG, 3, 2, 1, "GreaterTrue" },
        OpTestCase { OpType::kG, 2, 2, 0, "GreaterFalse" },
        OpTestCase { OpType::kLE, 2, 2, 1, "LessEqualTrue" },
        OpTestCase { OpType::kLE, 3, 2, 0, "LessEqualFalse" },
        OpTestCase { OpType::kGE, 2, 2, 1, "GreaterEqualTrue" },
        OpTestCase { OpType::kGE, 1, 2, 0, "GreaterEqualFalse" },
        OpTestCase { OpType::kEQ, 5, 5, 1, "EqualTrue" },
        OpTestCase { OpType::kEQ, 5, 6, 0, "EqualFalse" },
        OpTestCase { OpType::kNE, 5, 5, 0, "NotEqualFalse" },
        OpTestCase { OpType::kNE, 5, 6, 1, "NotEqualTrue" }),
    [](const ::testing::TestParamInfo<OpTestCase>& param_info) {
        return std::string(param_info.param.name);
    });

TEST_F(InterpreterTest, DivisionByZeroIsError) {
    EXPECT_EQ(RunMain(MakePrint(MakeOp(OpType::kDiv, MakeValue(1), MakeValue(0)))),
        Status::kError);
    EXPECT_EQ(Output(), "division by zero\n");
}

TEST_F(InterpreterTest, ModuloByZeroIsError) {
    EXPECT_EQ(RunMain(MakePrint(MakeOp(OpType::kMod, MakeValue(1), MakeValue(0)))),
        Status::kError);
    EXPECT_EQ(Output(), "division by zero\n");
}

TEST_F(InterpreterTest, ErrorInLeftOperandPropagates) {
    EXPECT_EQ(RunMain(MakeAssign("result", MakeOp(OpType::kAdd, MakeFail(), MakeValue(1)))),
        Status::kError);
}

TEST_F(InterpreterTest, ErrorInRightOperandPropagates) {
    EXPECT_EQ(RunMain(MakeAssign("result", MakeOp(OpType::kAdd, MakeValue(1), MakeFail()))),
        Status::kError);
}

TEST_F(InterpreterTest, NestedExpression) {
    auto expression = MakeOp(
        OpType::kSub,
        MakeOp(OpType::kMul, MakeOp(OpType::kAdd, MakeValue(2), MakeValue(3)), MakeValue(4)),
        MakeValue(1));
    EXPECT_EQ(RunMain(MakePrint(std::move(expression))), Status::kOk);
    EXPECT_EQ(Output(), "19");
}

TEST_F(InterpreterTest, AssignCreatesVariable) {
    EXPECT_EQ(RunMain(MakeAssign("a", MakeValue(1)), MakePrint(MakeVariable("a"))), Status::kOk);
    EXPECT_EQ(Output(), "1");
}

TEST_F(InterpreterTest, AssignOverwritesVariable) {
    EXPECT_EQ(RunMain(MakeAssign("a", MakeValue(1)),
                  MakeAssign("a", MakeValue(2)),
                  MakePrint(MakeVariable("a"))),
        Status::kOk);
    EXPECT_EQ(Output(), "2");
}

TEST_F(InterpreterTest, AssignFromExpression) {
    EXPECT_EQ(RunMain(MakeAssign("a", MakeValue(3)),
                  MakeAssign("b", MakeOp(OpType::kMul, MakeVariable("a"), MakeValue(2))),
                  MakePrint(MakeVariable("b"))),
        Status::kOk);
    EXPECT_EQ(Output(), "6");
}

TEST_F(InterpreterTest, ScanReadsValue) {
    SetInput("17");
    EXPECT_EQ(RunMain(MakeScan("x"), MakePrint(MakeVariable("x"))), Status::kOk);
    EXPECT_EQ(Output(), "17");
}

TEST_F(InterpreterTest, ScanThenArithmetic) {
    SetInput("4");
    EXPECT_EQ(RunMain(MakeScan("x"),
                  MakePrint(MakeOp(OpType::kMul, MakeVariable("x"), MakeValue(5)))),
        Status::kOk);
    EXPECT_EQ(Output(), "20");
}

TEST_F(InterpreterTest, PrintMultipleValues) {
    EXPECT_EQ(RunMain(MakePrint(MakeValue(1)), MakePrint(MakeValue(2)), MakePrint(MakeValue(3))),
        Status::kOk);
    EXPECT_EQ(Output(), "123");
}

TEST_F(InterpreterTest, InnerScopeSeesOuterVariable) {
    EXPECT_EQ(RunMain(MakeAssign("x", MakeValue(5)), MakeScope(MakePrint(MakeVariable("x")))),
        Status::kOk);
    EXPECT_EQ(Output(), "5");
}

TEST_F(InterpreterTest, InnerScopeModifiesOuterVariable) {
    EXPECT_EQ(RunMain(MakeAssign("x", MakeValue(1)),
                  MakeScope(MakeAssign("x", MakeValue(2))),
                  MakePrint(MakeVariable("x"))),
        Status::kOk);
    EXPECT_EQ(Output(), "2");
}

TEST_F(InterpreterTest, ReturnStopsScope) {
    EXPECT_EQ(RunMain(MakePrint(MakeValue(1)), MakeReturn(), MakePrint(MakeValue(2))),
        Status::kOk);
    EXPECT_EQ(Output(), "1");
}

TEST_F(InterpreterTest, ErrorStopsScope) {
    EXPECT_EQ(RunMain(MakePrint(MakeValue(1)),
                  MakeAssign("a", MakeFail()),
                  MakePrint(MakeValue(2))),
        Status::kError);
    EXPECT_EQ(Output(), "1division by zero\n");
}

TEST_F(InterpreterTest, IfTrueRunsBody) {
    EXPECT_EQ(RunMain(MakeIf(MakeValue(1), MakeScope(MakePrint(MakeValue(1))))), Status::kOk);
    EXPECT_EQ(Output(), "1");
}

TEST_F(InterpreterTest, IfFalseWithoutElseDoesNothing) {
    EXPECT_EQ(RunMain(MakeIf(MakeValue(0), MakeScope(MakePrint(MakeValue(1))))), Status::kOk);
    EXPECT_EQ(Output(), "");
}

TEST_F(InterpreterTest, IfFalseRunsElse) {
    EXPECT_EQ(RunMain(MakeIf(MakeValue(0),
                  MakeScope(MakePrint(MakeValue(1))),
                  MakeScope(MakePrint(MakeValue(2))))),
        Status::kOk);
    EXPECT_EQ(Output(), "2");
}

TEST_F(InterpreterTest, IfTrueSkipsElse) {
    EXPECT_EQ(RunMain(MakeIf(MakeValue(1),
                  MakeScope(MakePrint(MakeValue(1))),
                  MakeScope(MakePrint(MakeValue(2))))),
        Status::kOk);
    EXPECT_EQ(Output(), "1");
}

TEST_F(InterpreterTest, IfConditionError) {
    EXPECT_EQ(RunMain(MakeIf(MakeFail(), MakeScope(MakePrint(MakeValue(1))))),
        Status::kError);
    EXPECT_EQ(Output(), "division by zero\n");
}

TEST_F(InterpreterTest, ReturnInsideIfStopsExecution) {
    EXPECT_EQ(RunMain(MakeIf(MakeValue(1), MakeScope(MakeReturn())), MakePrint(MakeValue(9))),
        Status::kOk);
    EXPECT_EQ(Output(), "");
}

TEST_F(InterpreterTest, WhileZeroIterations) {
    EXPECT_EQ(RunMain(MakeWhile(MakeValue(0), MakeScope(MakePrint(MakeValue(1))))), Status::kOk);
    EXPECT_EQ(Output(), "");
}

TEST_F(InterpreterTest, WhileCounts) {
    auto loop = MakeWhile(
        MakeOp(OpType::kL, MakeVariable("i"), MakeValue(3)),
        MakeScope(MakePrint(MakeVariable("i")),
            MakeAssign("i", MakeOp(OpType::kAdd, MakeVariable("i"), MakeValue(1)))));
    EXPECT_EQ(RunMain(MakeAssign("i", MakeValue(0)), std::move(loop)), Status::kOk);
    EXPECT_EQ(Output(), "012");
}

TEST_F(InterpreterTest, ReturnInsideWhileExitsLoop) {
    EXPECT_EQ(RunMain(MakeWhile(MakeValue(1), MakeScope(MakePrint(MakeValue(7)), MakeReturn())),
                  MakePrint(MakeValue(8))),
        Status::kOk);
    EXPECT_EQ(Output(), "7");
}

TEST_F(InterpreterTest, WhileConditionError) {
    EXPECT_EQ(RunMain(MakeWhile(MakeFail(), MakeScope())), Status::kError);
}

TEST_F(InterpreterTest, WhileBodyError) {
    EXPECT_EQ(RunMain(MakeWhile(MakeValue(1),
                  MakeScope(MakeAssign("a", MakeFail())))),
        Status::kError);
}

TEST_F(InterpreterTest, CallReturnsValue) {
    auto subtract_function = MakeFunction(
        "subtract", { "lhs", "rhs" },
        MakeScope(MakeReturn(MakeOp(OpType::kSub, MakeVariable("lhs"), MakeVariable("rhs")))));
    auto main_function = MakeFunction(
        "main", { }, MakeScope(MakePrint(MakeCall("subtract", MakeValue(5), MakeValue(3)))));
    EXPECT_EQ(RunProgram(std::move(subtract_function), std::move(main_function)), Status::kOk);
    EXPECT_EQ(Output(), "2");
}

TEST_F(InterpreterTest, CallWithoutReturnYieldsZero) {
    auto noop_function = MakeFunction("noop", { }, MakeScope(MakeAssign("x", MakeValue(1))));
    auto main_function = MakeFunction("main", { }, MakeScope(MakePrint(MakeCall("noop"))));
    EXPECT_EQ(RunProgram(std::move(noop_function), std::move(main_function)), Status::kOk);
    EXPECT_EQ(Output(), "0");
}

TEST_F(InterpreterTest, BareReturnYieldsZero) {
    auto empty_return_function = MakeFunction("empty_return", { }, MakeScope(MakeReturn()));
    auto main_function = MakeFunction("main", { }, MakeScope(MakePrint(MakeCall("empty_return"))));
    EXPECT_EQ(RunProgram(std::move(empty_return_function), std::move(main_function)),
        Status::kOk);
    EXPECT_EQ(Output(), "0");
}

TEST_F(InterpreterTest, CallArgumentError) {
    auto identity_function = MakeFunction("identity", { "value" }, MakeScope(MakeReturn(MakeVariable("value"))));
    auto main_function = MakeFunction(
        "main", { }, MakeScope(MakeAssign("result", MakeCall("identity", MakeFail()))));
    EXPECT_EQ(RunProgram(std::move(identity_function), std::move(main_function)), Status::kError);
}

TEST_F(InterpreterTest, ErrorInsideCalleeBodyPropagates) {
    auto broken_function = MakeFunction("broken", { }, MakeScope(MakeReturn(MakeFail())));
    auto main_function = MakeFunction("main", { }, MakeScope(MakeAssign("result", MakeCall("broken"))));
    EXPECT_EQ(RunProgram(std::move(broken_function), std::move(main_function)), Status::kError);
}

TEST_F(InterpreterTest, RecursionFactorial) {
    auto factorial_function = MakeFunction(
        "factorial", { "n" },
        MakeScope(
            MakeIf(MakeOp(OpType::kLE, MakeVariable("n"), MakeValue(1)),
                MakeScope(MakeReturn(MakeValue(1)))),
            MakeReturn(MakeOp(
                OpType::kMul, MakeVariable("n"),
                MakeCall("factorial", MakeOp(OpType::kSub, MakeVariable("n"), MakeValue(1)))))));
    auto main_function = MakeFunction("main", { }, MakeScope(MakePrint(MakeCall("factorial", MakeValue(5)))));
    EXPECT_EQ(RunProgram(std::move(factorial_function), std::move(main_function)), Status::kOk);
    EXPECT_EQ(Output(), "120");
}

TEST_F(InterpreterTest, ParameterDoesNotModifyCallerVariable) {
    auto overwrite_function = MakeFunction(
        "overwrite", { "x" },
        MakeScope(MakeAssign("x", MakeValue(99)), MakeReturn(MakeVariable("x"))));
    auto main_function = MakeFunction(
        "main", { },
        MakeScope(MakeAssign("x", MakeValue(10)),
            MakePrint(MakeCall("overwrite", MakeVariable("x"))),
            MakePrint(MakeVariable("x"))));
    EXPECT_EQ(RunProgram(std::move(overwrite_function), std::move(main_function)), Status::kOk);
    EXPECT_EQ(Output(), "9910");
}

TEST_F(InterpreterTest, ReturnInsideLoopInsideFunction) {
    auto find_function = MakeFunction(
        "find_three", { },
        MakeScope(
            MakeAssign("i", MakeValue(0)),
            MakeWhile(
                MakeValue(1),
                MakeScope(
                    MakeIf(MakeOp(OpType::kEQ, MakeVariable("i"), MakeValue(3)),
                        MakeScope(MakeReturn(MakeVariable("i")))),
                    MakeAssign("i", MakeOp(OpType::kAdd, MakeVariable("i"), MakeValue(1)))))));
    auto main_function = MakeFunction("main", { }, MakeScope(MakePrint(MakeCall("find_three"))));
    EXPECT_EQ(RunProgram(std::move(find_function), std::move(main_function)), Status::kOk);
    EXPECT_EQ(Output(), "3");
}

TEST_F(InterpreterTest, MainReturnIsOk) {
    EXPECT_EQ(RunMain(MakeReturn(MakeValue(5))), Status::kOk);
}

TEST_F(InterpreterTest, EmptyMainIsOk) {
    EXPECT_EQ(RunMain(), Status::kOk);
    EXPECT_EQ(Output(), "");
}

TEST_F(InterpreterTest, RunInterpreterReturnsTrueOnSuccess) {
    auto main_function = MakeFunction("main", { }, MakeScope(MakePrint(MakeValue(1))));
    EXPECT_TRUE(RunWithInterpreterEntryPoint(std::move(main_function)));
    EXPECT_EQ(Output(), "1");
}

TEST_F(InterpreterTest, RunInterpreterReturnsTrueOnMainReturn) {
    auto main_function = MakeFunction("main", { }, MakeScope(MakeReturn(MakeValue(5))));
    EXPECT_TRUE(RunWithInterpreterEntryPoint(std::move(main_function)));
}

TEST_F(InterpreterTest, RunInterpreterReportsDivisionByZero) {
    auto main_function = MakeFunction("main", { },
        MakeScope(MakePrint(MakeOp(OpType::kDiv, MakeValue(1), MakeValue(0)))));
    EXPECT_FALSE(RunWithInterpreterEntryPoint(std::move(main_function)));
    EXPECT_TRUE(OutputContains("division by zero"));
}

// Iterative interpreter: tests for the explicit task stack

TEST_F(InterpreterTest, DeepRecursionDoesNotOverflowStack) {
    auto sum_function = MakeFunction(
        "sum", { "n" },
        MakeScope(
            MakeIf(MakeOp(OpType::kEQ, MakeVariable("n"), MakeValue(0)),
                MakeScope(MakeReturn(MakeValue(0)))),
            MakeReturn(MakeOp(
                OpType::kAdd, MakeVariable("n"),
                MakeCall("sum", MakeOp(OpType::kSub, MakeVariable("n"), MakeValue(1)))))));
    auto main_function = MakeFunction("main", { }, MakeScope(MakePrint(MakeCall("sum", MakeValue(100000)))));
    EXPECT_EQ(RunProgram(std::move(sum_function), std::move(main_function)), Status::kOk);
    EXPECT_EQ(Output(), "5000050000");
}

TEST_F(InterpreterTest, ErrorInDeepRecursionStopsExecution) {
    auto fail_function = MakeFunction(
        "fail", { "n" },
        MakeScope(
            MakeIf(MakeOp(OpType::kEQ, MakeVariable("n"), MakeValue(0)),
                MakeScope(MakeReturn(MakeFail()))),
            MakeReturn(MakeCall("fail", MakeOp(OpType::kSub, MakeVariable("n"), MakeValue(1))))));
    auto main_function = MakeFunction("main", { },
        MakeScope(MakePrint(MakeCall("fail", MakeValue(1000))), MakePrint(MakeValue(1))));
    EXPECT_EQ(RunProgram(std::move(fail_function), std::move(main_function)), Status::kError);
    EXPECT_EQ(Output(), "division by zero\n");
}

TEST_F(InterpreterTest, MainReturnZeroDoesNotHideError) {
    auto main_function = MakeFunction("main", { },
        MakeScope(MakePrint(MakeFail()), MakeReturn(MakeValue(0))));
    EXPECT_FALSE(RunWithInterpreterEntryPoint(std::move(main_function)));
    EXPECT_EQ(Output(), "division by zero\n");
}

TEST_F(InterpreterTest, OnlyFirstErrorIsReported) {
    EXPECT_EQ(RunMain(MakePrint(MakeFail()), MakePrint(MakeFail())), Status::kError);
    EXPECT_EQ(Output(), "division by zero\n");
}

TEST_F(InterpreterTest, ArgumentsEvaluatedLeftToRight) {
    auto show_function = MakeFunction(
        "show", { "x" }, MakeScope(MakePrint(MakeVariable("x")), MakeReturn(MakeVariable("x"))));
    auto pair_function = MakeFunction(
        "pair", { "a", "b" },
        MakeScope(MakeReturn(MakeOp(OpType::kAdd,
            MakeOp(OpType::kMul, MakeVariable("a"), MakeValue(10)),
            MakeVariable("b")))));
    auto main_function = MakeFunction(
        "main", { },
        MakeScope(MakePrint(MakeCall("pair",
            MakeCall("show", MakeValue(1)),
            MakeCall("show", MakeValue(2))))));
    EXPECT_EQ(RunProgram(std::move(show_function), std::move(pair_function), std::move(main_function)),
        Status::kOk);
    EXPECT_EQ(Output(), "1212");
}

TEST_F(InterpreterTest, BinaryOperandsEvaluatedLeftToRight) {
    auto show_function = MakeFunction(
        "show", { "x" }, MakeScope(MakePrint(MakeVariable("x")), MakeReturn(MakeVariable("x"))));
    auto main_function = MakeFunction(
        "main", { },
        MakeScope(MakePrint(MakeOp(OpType::kSub,
            MakeCall("show", MakeValue(5)),
            MakeCall("show", MakeValue(3))))));
    EXPECT_EQ(RunProgram(std::move(show_function), std::move(main_function)), Status::kOk);
    EXPECT_EQ(Output(), "532");
}

TEST_F(InterpreterTest, CallAsStatementRuns) {
    auto hello_function = MakeFunction("hello", { }, MakeScope(MakePrint(MakeValue(1))));
    auto main_function = MakeFunction(
        "main", { }, MakeScope(MakeCall("hello"), MakeCall("hello"), MakePrint(MakeValue(2))));
    EXPECT_EQ(RunProgram(std::move(hello_function), std::move(main_function)), Status::kOk);
    EXPECT_EQ(Output(), "112");
}

TEST_F(InterpreterTest, CallAsStatementInsideLoop) {
    auto hello_function = MakeFunction("hello", { }, MakeScope(MakePrint(MakeValue(1))));
    auto loop = MakeWhile(
        MakeOp(OpType::kL, MakeVariable("i"), MakeValue(3)),
        MakeScope(MakeCall("hello"),
            MakeAssign("i", MakeOp(OpType::kAdd, MakeVariable("i"), MakeValue(1)))));
    auto main_function = MakeFunction(
        "main", { }, MakeScope(MakeAssign("i", MakeValue(0)), std::move(loop), MakePrint(MakeVariable("i"))));
    EXPECT_EQ(RunProgram(std::move(hello_function), std::move(main_function)), Status::kOk);
    EXPECT_EQ(Output(), "1113");
}

TEST_F(InterpreterTest, ReturnFromNestedScopesRestoresCallerScope) {
    auto nested_function = MakeFunction(
        "nested", { },
        MakeScope(
            MakeAssign("a", MakeValue(5)),
            MakeScope(
                MakeAssign("b", MakeValue(6)),
                MakeIf(MakeValue(1), MakeScope(MakeReturn(MakeVariable("b")))))));
    auto main_function = MakeFunction(
        "main", { },
        MakeScope(
            MakeAssign("x", MakeValue(10)),
            MakePrint(MakeCall("nested")),
            MakePrint(MakeVariable("x")),
            MakeAssign("y", MakeValue(1)),
            MakePrint(MakeVariable("y"))));
    EXPECT_EQ(RunProgram(std::move(nested_function), std::move(main_function)), Status::kOk);
    EXPECT_EQ(Output(), "6101");
}

TEST_F(InterpreterTest, ReturnInCalleeDoesNotStopCaller) {
    auto early_function = MakeFunction(
        "early", { }, MakeScope(MakeReturn(MakeValue(1)), MakePrint(MakeValue(9))));
    auto main_function = MakeFunction(
        "main", { },
        MakeScope(MakePrint(MakeCall("early")), MakePrint(MakeCall("early")), MakePrint(MakeValue(2))));
    EXPECT_EQ(RunProgram(std::move(early_function), std::move(main_function)), Status::kOk);
    EXPECT_EQ(Output(), "112");
}

TEST_F(InterpreterTest, ReturnValueUsedInExpression) {
    auto five_function = MakeFunction("five", { }, MakeScope(MakeReturn(MakeValue(5))));
    auto main_function = MakeFunction(
        "main", { },
        MakeScope(MakePrint(MakeOp(OpType::kAdd,
            MakeCall("five"),
            MakeOp(OpType::kMul, MakeCall("five"), MakeValue(2))))));
    EXPECT_EQ(RunProgram(std::move(five_function), std::move(main_function)), Status::kOk);
    EXPECT_EQ(Output(), "15");
}

TEST_F(InterpreterTest, MutualRecursion) {
    auto even_function = MakeFunction(
        "is_even", { "n" },
        MakeScope(
            MakeIf(MakeOp(OpType::kEQ, MakeVariable("n"), MakeValue(0)),
                MakeScope(MakeReturn(MakeValue(1)))),
            MakeReturn(MakeCall("is_odd", MakeOp(OpType::kSub, MakeVariable("n"), MakeValue(1))))));
    auto odd_function = MakeFunction(
        "is_odd", { "n" },
        MakeScope(
            MakeIf(MakeOp(OpType::kEQ, MakeVariable("n"), MakeValue(0)),
                MakeScope(MakeReturn(MakeValue(0)))),
            MakeReturn(MakeCall("is_even", MakeOp(OpType::kSub, MakeVariable("n"), MakeValue(1))))));
    auto main_function = MakeFunction(
        "main", { },
        MakeScope(MakePrint(MakeCall("is_even", MakeValue(10))),
            MakePrint(MakeCall("is_even", MakeValue(7)))));
    EXPECT_EQ(RunProgram(std::move(even_function), std::move(odd_function), std::move(main_function)),
        Status::kOk);
    EXPECT_EQ(Output(), "10");
}

TEST_F(InterpreterTest, FunctionDefinedAfterMainIsCallable) {
    auto main_function = MakeFunction("main", { }, MakeScope(MakePrint(MakeCall("late"))));
    auto late_function = MakeFunction("late", { }, MakeScope(MakeReturn(MakeValue(4))));
    EXPECT_EQ(RunProgram(std::move(main_function), std::move(late_function)), Status::kOk);
    EXPECT_EQ(Output(), "4");
}

TEST_F(InterpreterTest, InterpreterCanBeReused) {
    auto first_main = MakeFunction("main", { }, MakeScope(MakePrint(MakeValue(1))));
    EXPECT_TRUE(RunWithInterpreterEntryPoint(std::move(first_main)));
    auto second_main = MakeFunction("main", { }, MakeScope(MakePrint(MakeValue(2))));
    EXPECT_TRUE(RunWithInterpreterEntryPoint(std::move(second_main)));
    EXPECT_EQ(Output(), "12");
}

TEST_F(InterpreterTest, ErrorIsResetBetweenRuns) {
    auto broken_main = MakeFunction("main", { }, MakeScope(MakePrint(MakeFail())));
    EXPECT_FALSE(RunWithInterpreterEntryPoint(std::move(broken_main)));
    auto good_main = MakeFunction("main", { }, MakeScope(MakePrint(MakeValue(1))));
    EXPECT_TRUE(RunWithInterpreterEntryPoint(std::move(good_main)));
    EXPECT_EQ(Output(), "division by zero\n1");
}

} // namespace
