#pragma once

#include "context.hpp"
#include "node.hpp"

#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace paracl::interpreter {
enum class TaskType : uint8_t {
    kEval,
    kFinishOp,
    kReturn,
    kPrint,
    kFinishIf,
    kFinishWhile,
    kFinishCall,
    kFinishAssign,
    kLeaveScope,
    kBoundary,
};

struct Task {
    TaskType type;
    const INode* node = nullptr;
};

class Interpreter : public IVisitor {
public:
    Interpreter() = default;

    bool RunInterpreter(const INode& root) {
        ClearInterpreterFields();
        PushToTasks(TaskType::kEval, &root);
        while (!tasks_.empty() && !error_) {
            const Task task = tasks_.back();
            tasks_.pop_back();
            ApplyTask(task);
        }

        if (error_) {
            std::cout << *error_ << '\n';
            return false;
        }

        return true;
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
        functionTable_.clear();
        tasks_.clear();
        values_.clear();
        env_.ClearContextFields();
        error_.reset();
        return_value_ = 0;
    }

    void PushToTasks(TaskType type, const INode* node = nullptr) {
        tasks_.push_back({ .type = type, .node = node });
    }

    BaseValue PopFromValues() {
        const BaseValue val = values_.back();
        values_.pop_back();
        return val;
    }

    void ApplyTask(const Task& task);
    BaseValue Apply(const OpNode& node, const BaseValue& lhs, const BaseValue& rhs);
    void FinishFrame();
    void Unwind();
    void SetError(std::string error_message);

    Context env_;
    std::unordered_map<std::string_view, const paracl::FunctionNode*> functionTable_;
    std::optional<std::string> error_;
    std::vector<Task> tasks_;
    std::vector<BaseValue> values_;
    BaseValue return_value_ = 0;
};

} // namespace paracl::interpreter
