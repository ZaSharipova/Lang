#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "lexer.hpp"

namespace paracl::parser {

class NameChecker {
public:
    NameChecker() = default;

    bool BeginFunction(const std::string& name, const std::vector<std::string>& params, lexer::Token token) {
        if (!functionTable_.emplace(name, params.size()).second) {
            SetError("Redefinition of function: " + name, token);
            return false;
        }

        PushFrame();
        for (const auto& param : params) {
            Define(param);
        }

        return true;
    }

    void EndFunction() {
        PopFrame();
    }

    void PushToCalls(const std::string& name, std::size_t argc, lexer::Token token) {
        calls_.push_back({ .name = name, .argc = argc, .token = token });
    }

    struct Error {
        std::string message;
        lexer::Token token;
    };

    void TryCreate(const std::string& name) {
        if (!IsDefined(name)) {
            Define(name);
        }
    }

    bool CheckUse(const std::string& name, lexer::Token token) {
        if (!IsDefined(name)) {
            SetError("undeclared identifier " + name, token);
            return false;
        }

        return true;
    }

    bool CheckFunctions();

    std::optional<std::string> GetError() const {
        return error_;
    }

    std::optional<lexer::Token> GetErrorToken() const {
        return errorToken_;
    }

    void PushScope() { scopes_.emplace_back(); }
    void PopScope() { scopes_.pop_back(); }

private:
    struct PendingCall {
        std::string name;
        std::size_t argc = 0;
        lexer::Token token;
    };

    void PushFrame() {
        scopes_.clear();
        PushScope();
    }
    void PopFrame() { scopes_.clear(); }
    void Define(const std::string& name) { scopes_.back().insert(name); }

    bool IsDefined(const std::string& name) {
        for (const auto& scope : scopes_) {
            if (scope.contains(name)) {
                return true;
            }
        }

        return false;
    }

    void SetError(std::string error_message, std::optional<lexer::Token> token = std::nullopt) {
        if (!error_) {
            error_ = std::move(error_message);
            errorToken_ = token;
        }
    }

    std::unordered_map<std::string, std::size_t> functionTable_;
    std::vector<std::unordered_set<std::string>> scopes_;
    std::vector<PendingCall> calls_;
    std::optional<std::string> error_;
    std::optional<lexer::Token> errorToken_;

};

} // namespace paracl::parser
