#pragma once

#include "context.hpp"
#include "node.hpp"

#include <iostream>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace paracl::interpreter {

class Interpreter : public IVisitor {
public:
    Interpreter() = default;

    bool RunInterpreter(const INode& root) {
        ClearInterpreterFields();

        const Status res = root.Accept(*this);
        if (res != Status::kOk) {
            HandleErrors();
        }

        return res == Status::kOk;
    }

    Status Visit(const ValueNode& node) override;
    Status Visit(const VariableNode& node) override;
    Status Visit(const OpNode& node) override;
    Status Visit(const AssignNode& node) override;
    Status Visit(const ScanNode& node) override;
    Status Visit(const PrintNode& node) override;
    Status Visit(const ReturnNode& node) override;
    Status Visit(const FunctionNode& node) override;
    Status Visit(const CallNode& node) override;
    Status Visit(const ProgramNode& node) override;
    Status Visit(const ScopeNode& node) override;
    Status Visit(const IfNode& node) override;
    Status Visit(const WhileNode& node) override;

private:
    void ClearInterpreterFields() {
        errors_.clear();
        functionTable_.clear();
        result_ = 0;
    }

    void HandleErrors() const {
        for (const auto& error : errors_) {
            std::cout << error << '\n';
        }
    }

    Context env_;
    std::unordered_map<std::string_view, const paracl::FunctionNode*> functionTable_;
    BaseValue result_ = 0;
    std::vector<std::string> errors_;
};

} // namespace paracl::interpreter
