#pragma once

#include <string_view>
#include <vector>

#include "config.hpp"

namespace paracl {

using BaseValue = std::int64_t;

class Context {
public:
    void PushScope() { scopeStarts_.push_back(vars_.size()); }

    void PopScope() {
        vars_.erase(vars_.begin() + static_cast<std::ptrdiff_t>(scopeStarts_.back()), vars_.end());
        scopeStarts_.pop_back();
    }

    void Define(std::string_view name, const BaseValue& value) {
        if (scopeStarts_.empty()) {
            globals_.emplace_back(name, value);
        } else {
            vars_.emplace_back(name, value);
        }
    }

    BaseValue* Lookup(std::string_view name) {
        for (std::size_t i = vars_.size(); i > frameBase_; --i) {
            if (vars_[i - 1].first == name) {
                return &vars_[i - 1].second;
            }
        }

        for (auto& [possible_name, possible_value] : globals_) {
            if (possible_name == name) {
                return &possible_value;
            }
        }

        return nullptr;
    }

    void AssignOrCreate(std::string_view name, const BaseValue& value) {
        if (BaseValue* table_value = Lookup(name)) {
            *table_value = value;
        } else {
            Define(name, value);
        }
    }

    std::size_t GetFrameBase() const { return frameBase_; }
    void SetFrameBase(std::size_t base) { frameBase_ = base; }
    std::size_t Size() const { return vars_.size(); }

private:
    std::vector<std::pair<std::string_view, BaseValue>> vars_;
    std::vector<std::size_t> scopeStarts_;
    std::vector<std::pair<std::string_view, BaseValue>> globals_;
    std::size_t frameBase_ = 0;
};

class ScopeGuard {
public:
    explicit ScopeGuard(Context& env)
        : env_(env) { env_.PushScope(); }
    ~ScopeGuard() { env_.PopScope(); }
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard(ScopeGuard&&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;

private:
    Context& env_;
};

class CallFrameGuard {
public:
    explicit CallFrameGuard(Context& env)
        : env_(env)
        , oldBase_(env.GetFrameBase()) {
        env_.SetFrameBase(env_.Size());
        env_.PushScope();
    }

    ~CallFrameGuard() {
        env_.PopScope();
        env_.SetFrameBase(oldBase_);
    }
    CallFrameGuard(const CallFrameGuard&) = delete;
    CallFrameGuard(CallFrameGuard&&) = delete;
    CallFrameGuard& operator=(const CallFrameGuard&) = delete;

private:
    Context& env_;
    std::size_t oldBase_ = 0;
};

} // namespace paracl
