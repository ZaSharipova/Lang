#include "interpreter.hpp"
#include "node.hpp"

#include <cassert>
#include <stdexcept>

namespace paracl::interpreter {

Status Interpreter::Visit(const ValueNode& node) {
    values_.push_back(node.GetValue());
    return Status::kOk;
}

Status Interpreter::Visit(const VariableNode& node) {
    BaseValue* value = env_.Lookup(node.GetVariable());
    assert(value && "undefined variable must be rejected by the parser");

    values_.push_back(*value);
    return Status::kOk;
}

Status Interpreter::Visit(const OpNode& node) {
    PushToTasks(TaskType::kFinishOp, &node);
    PushToTasks(TaskType::kEval, node.GetRight());
    PushToTasks(TaskType::kEval, node.GetLeft());
    return Status::kOk;
}

Status Interpreter::Visit(const AssignNode& node) {
    PushToTasks(TaskType::kFinishAssign, &node);
    PushToTasks(TaskType::kEval, node.GetExpr());
    return Status::kOk;
}

Status Interpreter::Visit(const ScanNode& node) {
    BaseValue input { };
    std::cin >> input;
    env_.AssignOrCreate(node.GetVariable()->GetVariable(), input);
    return Status::kOk;
}

Status Interpreter::Visit(const PrintNode& node) {
    PushToTasks(TaskType::kPrint, &node);
    PushToTasks(TaskType::kEval, node.GetExpr());
    return Status::kOk;
}

Status Interpreter::Visit(const ReturnNode& node) {
    PushToTasks(TaskType::kReturn, &node);
    if (node.GetExpr()) {
        PushToTasks(TaskType::kEval, node.GetExpr());
    } else {
        values_.push_back(0);
    }

    return Status::kOk;
}

Status Interpreter::Visit(const FunctionNode&) {
    return Status::kOk;
}

Status Interpreter::Visit(const CallNode& node) {
    PushToTasks(TaskType::kFinishCall, &node);
    const auto& args = node.GetArgs();
    for (auto arg = args.rbegin(); arg != args.rend(); ++arg) {
        PushToTasks(TaskType::kEval, arg->get());
    }

    return Status::kOk;
}

Status Interpreter::Visit(const ProgramNode& node) {
    for (const auto& func : node.GetFunctions()) {
        functionTable_.emplace(func->GetName(), func.get());
    }

    PushToTasks(TaskType::kEval, functionTable_.at("main")->GetBody());
    return Status::kOk;
}

Status Interpreter::Visit(const ScopeNode& node) {
    env_.PushScope();
    PushToTasks(TaskType::kLeaveScope);
    const auto& stmts = node.GetStatements();

    for (auto it = stmts.rbegin(); it != stmts.rend(); ++it) {
        PushToTasks(TaskType::kEval, it->get()); // TODO: values are not clean if value is unused
        // as I haven't found anything better than dynamic cast, then decided not to do it
    }

    return Status::kOk;
}

Status Interpreter::Visit(const IfNode& node) {
    PushToTasks(TaskType::kFinishIf, &node);
    PushToTasks(TaskType::kEval, node.GetCondition());
    return Status::kOk;
}

Status Interpreter::Visit(const WhileNode& node) {
    PushToTasks(TaskType::kFinishWhile, &node);
    PushToTasks(TaskType::kEval, node.GetCondition());
    return Status::kOk;
}

void Interpreter::ApplyTask(const Task& task) {
    switch (task.type) {
    case (TaskType::kEval):
        task.node->Accept(*this);
        break;

    case (TaskType::kFinishOp): {
        const auto& node = static_cast<const OpNode&>(*task.node);
        BaseValue rhs = PopFromValues();
        BaseValue lhs = PopFromValues();

        if ((node.GetType() == OpType::kDiv || node.GetType() == OpType::kMod) && rhs == 0) {
            SetError("division by zero");
            break;
        }

        values_.push_back(Apply(node, lhs, rhs));
        break;
    }

    case (TaskType::kReturn):
        return_value_ = PopFromValues();
        Unwind();
        break;

    case (TaskType::kPrint):
        std::cout << PopFromValues();
        break;

    case (TaskType::kFinishIf): {
        const auto& node = static_cast<const IfNode&>(*task.node);
        if (PopFromValues() != 0) {
            PushToTasks(TaskType::kEval, node.GetBody());
        } else { // TODO
            if (node.ElseExists()) {
                PushToTasks(TaskType::kEval, node.GetElse());
            }
        }

        break;
    }

    case (TaskType::kFinishWhile): {
        const auto& node = static_cast<const WhileNode&>(*task.node);
        if (PopFromValues() == 0) {
            break;
        }

        PushToTasks(TaskType::kEval, &node);
        PushToTasks(TaskType::kEval, node.GetBody()); // TODO: hash search will be perfect for this case
        break;
    }

    case (TaskType::kFinishCall): {
        auto& node = *static_cast<const CallNode*>(task.node);
        const FunctionNode& func = *functionTable_.at(node.GetName());
        const std::size_t argc = node.GetArgs().size();
        env_.PushFrame();
        for (std::size_t i = 0; i < argc; i++) {
            env_.Define(func.GetParams()[i], values_[values_.size() - argc + i]);
        }
        values_.erase(values_.end() - argc, values_.end());

        PushToTasks(TaskType::kBoundary);
        PushToTasks(TaskType::kEval, func.GetBody());
        break;
    }

    case (TaskType::kFinishAssign): {
        const auto& node = static_cast<const AssignNode&>(*task.node);
        env_.AssignOrCreate(node.GetName(), PopFromValues());
        break;
    }

    case (TaskType::kLeaveScope):
        env_.PopScope();
        break;

    case (TaskType::kBoundary):
        FinishFrame();
        break;

    default:
        std::unreachable();
    }
}

BaseValue Interpreter::Apply(const OpNode& node, const BaseValue& lhs, const BaseValue& rhs) {
    switch (node.GetType()) {
    case OpType::kAdd:
        return lhs + rhs;
    case OpType::kSub:
        return lhs - rhs;
    case OpType::kMul:
        return lhs * rhs;
    case OpType::kDiv:
        return lhs / rhs;
    case OpType::kMod:
        return lhs % rhs;

    case OpType::kL:
        return lhs < rhs;
    case OpType::kG:
        return lhs > rhs;
    case OpType::kLE:
        return lhs <= rhs;
    case OpType::kGE:
        return lhs >= rhs;
    case OpType::kEQ:
        return lhs == rhs;
    case OpType::kNE:
        return lhs != rhs;

    default:
        std::unreachable();
    }
}

void Interpreter::FinishFrame() {
    env_.PopFrame();
    values_.push_back(return_value_);
    return_value_ = 0;
}

void Interpreter::Unwind() {
    while (!tasks_.empty()) {
        Task task = tasks_.back();
        tasks_.pop_back();
        if (task.type == TaskType::kLeaveScope) {
            env_.PopScope();
        } else if (task.type == TaskType::kBoundary) {
            FinishFrame();
            return;
        }
    }
}

void Interpreter::SetError(std::string error_message) {
    if (!error_) {
        error_ = std::move(error_message);
    }
}

} // namespace paracl::interpreter
