#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace paracl {

class ValueNode;
class VariableNode;
class OpNode;
class AssignNode;
class ScanNode;
class PrintNode;
class ReturnNode;
class FunctionNode;
class CallNode;
class ProgramNode;
class ScopeNode;
class IfNode;
class WhileNode;

enum class Status : uint8_t {
    kOk,
    kReturn,
    kError,
};

class IVisitor {
public:
    virtual ~IVisitor() = default;

    virtual Status Visit(const ValueNode& node) = 0;
    virtual Status Visit(const VariableNode& node) = 0;
    virtual Status Visit(const OpNode& node) = 0;
    virtual Status Visit(const AssignNode& node) = 0;
    virtual Status Visit(const ScanNode& node) = 0;
    virtual Status Visit(const PrintNode& node) = 0;
    virtual Status Visit(const ReturnNode& node) = 0;
    virtual Status Visit(const FunctionNode& node) = 0;
    virtual Status Visit(const CallNode& node) = 0;
    virtual Status Visit(const ProgramNode& node) = 0;
    virtual Status Visit(const ScopeNode& node) = 0;
    virtual Status Visit(const IfNode& node) = 0;
    virtual Status Visit(const WhileNode& node) = 0;
};

class INode {
public:
    INode() = default;
    virtual ~INode() = default;
    INode(const INode&) = delete;
    INode& operator=(const INode&) = delete;

    virtual Status Accept(IVisitor& visitor) const = 0;
};

template <typename Derived>
class NodeBase : public INode {
public:
    Status Accept(IVisitor& visitor) const override {
        return visitor.Visit(static_cast<const Derived&>(*this));
    }

private:
    NodeBase() = default;
    friend Derived;
};

using NodePtr = std::unique_ptr<INode>;

class ValueNode : public NodeBase<ValueNode> {
public:
    explicit ValueNode(uint64_t value)
        : value_(value) { }
    uint64_t GetValue() const { return value_; }

private:
    uint64_t value_;
};

class VariableNode : public NodeBase<VariableNode> {
public:
    explicit VariableNode(std::string variable)
        : variable_(std::move(variable)) { }
    const std::string& GetVariable() const { return variable_; }

private:
    std::string variable_;
};

enum class OpType : std::uint8_t {
    kAdd,
    kSub,
    kMul,
    kDiv,
    kMod,

    kL,
    kG,
    kLE,
    kGE,
    kEQ,
    kNE,
};

class OpNode : public NodeBase<OpNode> {
public:
    OpNode(OpType type, NodePtr left, NodePtr right)
        : type_(type)
        , left_(std::move(left))
        , right_(std::move(right)) { }

    OpType GetType() const { return type_; }
    INode* GetLeft() const { return left_.get(); }
    INode* GetRight() const { return right_.get(); }

private:
    OpType type_;
    NodePtr left_;
    NodePtr right_;
};

class AssignNode : public NodeBase<AssignNode> {
public:
    AssignNode(std::string name, NodePtr expr)
        : name_(std::move(name))
        , expr_(std::move(expr)) { }

    const std::string& GetName() const { return name_; }
    INode* GetExpr() const { return expr_.get(); }

private:
    std::string name_;
    NodePtr expr_;
};

class ScanNode : public NodeBase<ScanNode> {
public:
    explicit ScanNode(std::unique_ptr<VariableNode> variable_node)
        : variable_(std::move(variable_node)) { }

    VariableNode* GetVariable() const { return variable_.get(); }

private:
    std::unique_ptr<VariableNode> variable_;
};

class PrintNode : public NodeBase<PrintNode> {
public:
    explicit PrintNode(NodePtr expr)
        : expr_(std::move(expr)) { }

    INode* GetExpr() const { return expr_.get(); }

private:
    NodePtr expr_;
};

class ReturnNode : public NodeBase<ReturnNode> {
public:
    explicit ReturnNode(NodePtr expr)
        : expr_(std::move(expr)) { }

    INode* GetExpr() const { return expr_.get(); }

private:
    NodePtr expr_;
};

class FunctionNode : public NodeBase<FunctionNode> {
public:
    FunctionNode(std::string name, std::vector<std::string> params, NodePtr body)
        : name_(std::move(name))
        , params_(std::move(params))
        , body_(std::move(body)) { }

    const std::string& GetName() const { return name_; }
    const std::vector<std::string>& GetParams() const { return params_; }
    std::size_t GetParamsSize() const { return params_.size(); }
    INode* GetBody() const { return body_.get(); }

private:
    std::string name_;
    std::vector<std::string> params_;
    NodePtr body_;
};

class CallNode : public NodeBase<CallNode> {
public:
    CallNode(std::string name, std::vector<NodePtr> args)
        : name_(std::move(name))
        , args_(std::move(args)) { }

    const std::string& GetName() const { return name_; }
    const std::vector<NodePtr>& GetArgs() const { return args_; }
    std::size_t GetArgsSize() const { return args_.size(); }

private:
    std::string name_;
    std::vector<NodePtr> args_;
};

class ProgramNode : public NodeBase<ProgramNode> {
public:
    void AddFunction(std::unique_ptr<FunctionNode> function) { functions_.push_back(std::move(function)); }
    const std::vector<std::unique_ptr<FunctionNode>>& GetFunctions() const { return functions_; }

private:
    std::vector<std::unique_ptr<FunctionNode>> functions_;
};

class ScopeNode : public NodeBase<ScopeNode> {
public:
    void AddStatement(NodePtr stmt) { statements_.push_back(std::move(stmt)); }
    const std::vector<NodePtr>& GetStatements() const { return statements_; }

private:
    std::vector<NodePtr> statements_;
};

class IfNode : public NodeBase<IfNode> {
public:
    IfNode(NodePtr cond, NodePtr body, NodePtr else_body = nullptr)
        : cond_(std::move(cond))
        , body_(std::move(body))
        , else_body_(std::move(else_body)) { }

    INode* GetCondition() const { return cond_.get(); }
    INode* GetBody() const { return body_.get(); }
    bool ElseExists() const { return else_body_.get() != nullptr; }
    INode* GetElse() const { return else_body_.get(); }

private:
    NodePtr cond_;
    NodePtr body_;
    NodePtr else_body_;
};

class WhileNode : public NodeBase<WhileNode> {
public:
    WhileNode(NodePtr cond, NodePtr body)
        : cond_(std::move(cond))
        , body_(std::move(body)) { }

    INode* GetCondition() const { return cond_.get(); }
    INode* GetBody() const { return body_.get(); }

private:
    NodePtr cond_;
    NodePtr body_;
};

} // namespace paracl
