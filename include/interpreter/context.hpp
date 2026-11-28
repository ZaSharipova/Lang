#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include "config.hpp"

namespace paracl {

class Context {
public:
    void PushScope() { scopeStarts_.push_back(vars_.size()); }

    void PopScope() {
        vars_.erase(vars_.begin() + static_cast<std::ptrdiff_t>(scopeStarts_.back()), vars_.end());
        scopeStarts_.pop_back();
    }

    void Define(std::string_view name, const BaseValue& value) {
        vars_.emplace_back(name, value);
    }

    BaseValue* Lookup(std::string_view name) {
        for (std::size_t i = vars_.size(); i > frameBase_; --i) {
            if (vars_[i - 1].first == name) {
                return &vars_[i - 1].second;
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

    void PushFrame() {
        frameBases_.push_back(frameBase_);
        frameBase_ = vars_.size();
        PushScope();
    }

    void PopFrame() {
        PopScope();
        frameBase_ = frameBases_.back();
        frameBases_.pop_back();
    }

    void ClearContextFields() {
        vars_.clear();
        scopeStarts_.clear();
        frameBase_ = 0;
        frameBases_.clear();
    }

private:
    std::vector<std::pair<std::string_view, BaseValue>> vars_;
    std::vector<std::size_t> scopeStarts_;
    std::size_t frameBase_ = 0;
    std::vector<std::size_t> frameBases_;
};

} // namespace paracl
