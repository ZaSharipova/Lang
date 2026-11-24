#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <vector>


class Value;
class User;

template <typename T>
class IList_node;

template <typename Node_t, typename Parent_t>
class IList_node_with_parent;

class Instruction;
class BasicBlock;


template <typename T>
class IList_node {
private:
    T* prev_ = nullptr;
    T* next_ = nullptr;

public:
    IList_node() = default;

    IList_node(const IList_node&)            = delete;
    IList_node& operator=(const IList_node&) = delete;
    IList_node(IList_node&&)                 = delete;
    IList_node& operator=(IList_node&&)      = delete;

    T*       get_prev()       noexcept { return prev_; }
    const T* get_prev() const noexcept { return prev_; }
    T*       get_next()       noexcept { return next_; }
    const T* get_next() const noexcept { return next_; }

    void set_prev(T* prev) noexcept { prev_ = prev; }
    void set_next(T* next) noexcept { next_ = next; }
};


template <typename Node_t, typename Parent_t>
class IList_node_with_parent : public IList_node<Node_t> {
private:
    Parent_t* parent_;

public:
    Parent_t*       get_parent()        { return parent_; }
    const Parent_t* get_parent() const  { return parent_; }

    explicit IList_node_with_parent(Parent_t* parent)
        : parent_(parent) 
    {
        assert(parent_);
    }
};


class Value {
private:
    std::vector<User*> users_;
    
    friend class User; //NOTE: это нужно для того, чтобы использовать add_user в add_op.
// наверное, лучше запретить добавление новых user через value. 
// да, можно использовать protected, но тогда все наследники могу юзать этот метод, а он по сути нужен только для User
    void add_user(User* usr)  { assert(usr); users_.push_back(usr); }

    void erase_user(User* usr) noexcept {
        assert(usr);
        auto it = std::find(users_.begin(), users_.end(), usr);

        if (it != users_.end()) {
            users_.erase(it);
        }
    }

public:
    Value()          = default;
    virtual ~Value() = default;

    Value(const Value&)            = delete;
    Value& operator=(const Value&) = delete;
    Value(Value&&)                 = delete;
    Value& operator=(Value&&)      = delete;

    User*       get_user(std::size_t idx) const  { return users_.at(idx); }
    std::size_t num_users()               const  { return users_.size();  }
    
    const std::vector<User*>& users()     const  { return users_; }

};


class User : public Value {
private:
        std::vector<Value*> operands_;

public:
    User() = default;
    explicit User(std::initializer_list<Value*> ops);
    ~User() override;

    Value* get_op(std::size_t idx) const  { return operands_.at(idx); }
    std::size_t num_operands()     const  { return operands_.size();  }

    const std::vector<Value*>& operands() const  { return operands_; }

    void add_op  (Value* op);
    void erase_op(std::size_t idx);
    void set_op  (std::size_t idx, Value* op);

    void drop_all_dependencies() noexcept; //NOTE: этот метод сделан для того, чтобы указатели на этого юзера
    // в операндах не провисали, после того как мы удаляем юзера.
};

inline User::User(std::initializer_list<Value*> ops) {
    operands_.reserve(ops.size());

    try {
        for (Value* op : ops) {
            add_op(op);
        }
    } catch (...) {
        drop_all_dependencies();
        throw;
    }
}

inline User::~User() {
    drop_all_dependencies();
}

inline void User::drop_all_dependencies() noexcept {
    for (Value* op : operands_) {
        op->erase_user(this);
    }
    operands_.clear();
}

inline void User::add_op(Value* op) {
    assert(op);
    operands_.push_back(op);

    try {
        op->add_user(this);
    } catch (...) {
        operands_.pop_back();
        throw;
    }
}

inline void User::erase_op(std::size_t idx) {
    Value* old = operands_.at(idx);

    old->erase_user(this);
    operands_.erase(operands_.begin() + idx);
}

inline void User::set_op(std::size_t idx, Value* op) {
    assert(op);
    Value*& old = operands_.at(idx);

    if (old == op) {
        return;
    }

    op->add_user(this);
    old->erase_user(this);
    old = op;
}


class Instruction : public User,
                    public IList_node_with_parent<Instruction, BasicBlock> {
public:
    explicit Instruction(
        BasicBlock* parent,
        std::initializer_list<Value*> ops = {}
    )
        : User(ops)
        , IList_node_with_parent<Instruction, BasicBlock>(parent) 
        {}
};
