#include "interpreter.hpp"
#include "node.hpp"

namespace paracl::interpreter {

Status Interpreter::Visit(const ValueNode& node) {
    result_ = node.GetValue();
    return Status::kOk;
}

Status Interpreter::Visit(const VariableNode& node) {
    BaseValue* value = env_.Lookup(node.GetVariable());
    if (!value) {
        errors_.push_back("Undefined variable: " + node.GetVariable());
        return Status::kError;
    }

    result_ = *value;
    return Status::kOk;
}

Status Interpreter::Visit(const OpNode& node) {
    if (Status res = node.GetLeft()->Accept(*this); res != Status::kOk) {
        return res;
    }
    BaseValue lhs = result_;

    if (Status res = node.GetRight()->Accept(*this); res != Status::kOk) {
        return res;
    }
    BaseValue rhs = result_;

    if ((node.GetType() == OpType::kDiv || node.GetType() == OpType::kMod) && rhs == 0) {
        throw std::runtime_error("division by zero");
    }

    switch (node.GetType()) {
    case OpType::kAdd:
        result_ = lhs + rhs;
        break;
    case OpType::kSub:
        result_ = lhs - rhs;
        break;
    case OpType::kMul:
        result_ = lhs * rhs;
        break;
    case OpType::kDiv:
        result_ = lhs / rhs;
        break;
    case OpType::kMod:
        result_ = lhs % rhs;
        break;

    case OpType::kL:
        result_ = lhs < rhs;
        break;
    case OpType::kG:
        result_ = lhs > rhs;
        break;
    case OpType::kLE:
        result_ = lhs <= rhs;
        break;
    case OpType::kGE:
        result_ = lhs >= rhs;
        break;
    case OpType::kEQ:
        result_ = lhs == rhs;
        break;
    case OpType::kNE:
        result_ = lhs != rhs;
        break;

    default:
        std::unreachable();
    }

    return Status::kOk;
}

Status Interpreter::Visit(const AssignNode& node) {
    if (Status res = node.GetExpr()->Accept(*this); res != Status::kOk) {
        return res;
    }

    env_.AssignOrCreate(node.GetName(), result_);
    return Status::kOk;
}

Status Interpreter::Visit(const ScanNode& node) {
    BaseValue input { };
    std::cin >> input;

    env_.AssignOrCreate(node.GetVariable()->GetVariable(), input);
    return Status::kOk;
}

Status Interpreter::Visit(const PrintNode& node) {
    if (Status res = node.GetExpr()->Accept(*this); res != Status::kOk) {
        return res;
    }

    std::cout << result_;
    return Status::kOk;
}

Status Interpreter::Visit(const ReturnNode& node) {
    result_ = 0; // for return;
    if (node.GetExpr()) {
        if (Status res = node.GetExpr()->Accept(*this); res != Status::kOk) {
            return res;
        }
    }

    return Status::kReturn;
}

Status Interpreter::Visit(const FunctionNode&) {
    return Status::kOk;
}

Status Interpreter::Visit(const CallNode& node) {
    auto it = functionTable_.find(node.GetName());
    if (it == functionTable_.end()) {
        errors_.push_back("Function not found: " + node.GetName());
        return Status::kError;
    }
    const FunctionNode& func = *it->second;

    if (func.GetParams().size() != node.GetArgs().size()) {
        errors_.push_back("Function " + node.GetName() + " expects " + std::to_string(func.GetParams().size()) + " arguments, got " + std::to_string(node.GetArgs().size()));
        return Status::kError;
    }

    std::vector<BaseValue> args;
    args.reserve(node.GetArgs().size());
    for (const auto& arg : node.GetArgs()) {
        if (Status res = arg->Accept(*this); res != Status::kOk) {
            return res;
        }

        args.push_back(result_);
    }

    CallFrameGuard frame(env_);
    for (std::size_t i = 0; i < args.size(); ++i) {
        env_.Define(func.GetParams()[i], args[i]);
    }

    Status body_status = func.GetBody()->Accept(*this);
    if (body_status == Status::kError) {
        return Status::kError;
    }

    if (body_status == Status::kOk) {
        result_ = 0;
    }

    return Status::kOk;
}

Status Interpreter::Visit(const ProgramNode& node) {
    for (const auto& func : node.GetFunctions()) {
        if (!functionTable_.emplace(func->GetName(), func.get()).second) {
            errors_.push_back("Redefinition of function: " + func->GetName());
            return Status::kError;
        }
    }

    auto it = functionTable_.find("main"); // now it works like this. Maybe sometime it will change
    if (it == functionTable_.end()) {
        errors_.push_back("Function 'main' not found");
        return Status::kError;
    }
    if (it->second->GetParamsSize() != 0) {
        errors_.push_back("Function 'main' must have no parameters");
        return Status::kError;
    }

    Status res = it->second->GetBody()->Accept(*this);
    return res == Status::kError ? res : Status::kOk;
}

Status Interpreter::Visit(const ScopeNode& node) {
    ScopeGuard guard { env_ };

    for (const auto& stmt : node.GetStatements()) {
        if (Status res = stmt->Accept(*this); res != Status::kOk) {
            return res;
        }
    }

    return Status::kOk;
}

Status Interpreter::Visit(const IfNode& node) {
    if (Status res = node.GetCondition()->Accept(*this); res != Status::kOk) {
        return res;
    }

    if (result_ != 0) {
        return node.GetBody()->Accept(*this);
    }

    if (node.GetElse()) {
        return node.GetElse()->Accept(*this);
    }

    return Status::kOk;
}

Status Interpreter::Visit(const WhileNode& node) {
    while (true) {
        if (Status res = node.GetCondition()->Accept(*this); res != Status::kOk) {
            return res;
        }

        if (result_ == 0) {
            break;
        }

        if (Status res = node.GetBody()->Accept(*this); res != Status::kOk) {
            return res;
        }
    }

    return Status::kOk;
}

} // namespace paracl::interpreter
