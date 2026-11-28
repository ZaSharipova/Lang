#include "name_checker.hpp"

namespace paracl::parser {

bool NameChecker::CheckFunctions() {
    for (const auto& call : calls_) {
        auto it = functionTable_.find(call.name);
        if (it == functionTable_.end()) {
            SetError("Undefined reference to " + call.name, call.token);
            return false;
        }

        if (it->second != call.argc) {
            SetError("Function " + call.name + " expects " + std::to_string(it->second) + " arguments, got " + std::to_string(call.argc));
            return false;
        }
    }

    auto it = functionTable_.find("main");
    if (it == functionTable_.end()) {
        SetError("Function 'main' not found");
        return false;
    }

    if (it->second != 0) {
        SetError("Function 'main' must have no parameters");
        return false;
    }

    return true;
}

} // namespace paracl::parser
