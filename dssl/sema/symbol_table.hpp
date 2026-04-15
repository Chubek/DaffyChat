#pragma once
#include "../ast/ast.h"
#include <string>
#include <unordered_map>
#include <optional>

namespace dssl::sema {

enum class SymbolKind { Struct, Enum, Union, Rpc, Const };

struct Symbol {
    SymbolKind kind;
    std::string name;
    size_t item_index;
};

class SymbolTable {
public:
    bool define(const std::string& name, Symbol sym) {
        auto [it, inserted] = symbols_.emplace(name, std::move(sym));
        return inserted;
    }

    std::optional<Symbol> lookup(const std::string& name) const {
        auto it = symbols_.find(name);
        if (it != symbols_.end()) return it->second;
        return std::nullopt;
    }

    bool contains(const std::string& name) const {
        return symbols_.count(name) > 0;
    }

    const std::unordered_map<std::string, Symbol>& all() const { return symbols_; }

private:
    std::unordered_map<std::string, Symbol> symbols_;
};

} // namespace dssl::sema
