#pragma once
#include <string>
#include <unordered_map>
#include <optional>
#include <vector>

namespace daffyscript::sema {

enum class SymbolKind { Function, Struct, Enum, Variable, Import, Command, Hook };

struct Symbol {
    SymbolKind kind;
    std::string name;
    bool is_mutable = false;
    bool is_pub = false;
};

class SymbolTable {
public:
    void push_scope() { scopes_.emplace_back(); }
    void pop_scope() { if (!scopes_.empty()) scopes_.pop_back(); }

    bool define(const std::string& name, Symbol sym) {
        if (scopes_.empty()) push_scope();
        auto& scope = scopes_.back();
        auto [it, inserted] = scope.emplace(name, std::move(sym));
        return inserted;
    }

    std::optional<Symbol> lookup(const std::string& name) const {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) return found->second;
        }
        return std::nullopt;
    }

    bool contains(const std::string& name) const {
        return lookup(name).has_value();
    }

private:
    std::vector<std::unordered_map<std::string, Symbol>> scopes_;
};

} // namespace daffyscript::sema
