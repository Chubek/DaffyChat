#pragma once
#include "../ast/ast.h"
#include "../diag/diagnostic.hpp"
#include "symbol_table.hpp"
#include <variant>

namespace dssl::sema {

class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(DiagEngine& diag) : diag_(diag) {}

    bool analyze(const ast::File& file) {
        for (size_t i = 0; i < file.items.size(); ++i) {
            std::visit([&](auto& item) {
                using T = std::decay_t<decltype(item)>;
                if constexpr (std::is_same_v<T, ast::StructDecl>) {
                    registerSymbol(item.name, SymbolKind::Struct, i, file.filename);
                } else if constexpr (std::is_same_v<T, ast::EnumDecl>) {
                    registerSymbol(item.name, SymbolKind::Enum, i, file.filename);
                } else if constexpr (std::is_same_v<T, ast::UnionDecl>) {
                    registerSymbol(item.name, SymbolKind::Union, i, file.filename);
                } else if constexpr (std::is_same_v<T, ast::RpcDecl>) {
                    registerSymbol(item.name, SymbolKind::Rpc, i, file.filename);
                } else if constexpr (std::is_same_v<T, ast::ConstDecl>) {
                    registerSymbol(item.name, SymbolKind::Const, i, file.filename);
                }
            }, file.items[i]);
        }

        for (auto& item : file.items) {
            std::visit([&](auto& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, ast::StructDecl>) {
                    checkStruct(v, file.filename);
                } else if constexpr (std::is_same_v<T, ast::EnumDecl>) {
                    checkEnum(v, file.filename);
                } else if constexpr (std::is_same_v<T, ast::RpcDecl>) {
                    checkRpc(v, file.filename);
                } else if constexpr (std::is_same_v<T, ast::RestDecl>) {
                    checkRest(v, file.filename);
                }
            }, item);
        }

        return !diag_.has_errors();
    }

    const SymbolTable& symbols() const { return symbols_; }

private:
    DiagEngine& diag_;
    SymbolTable symbols_;

    void registerSymbol(const std::string& name, SymbolKind kind, size_t idx, const std::string& file) {
        if (!symbols_.define(name, Symbol{kind, name, idx})) {
            diag_.emit(Diagnostic{DiagLevel::Error, "E050",
                "Duplicate definition of '" + name + "'",
                {file, 0, 0}, {}});
        }
    }

    void checkTypeRef(const ast::TypeRefPtr& t, const std::string& file) {
        if (!t) return;
        switch (t->kind) {
            case ast::TypeRef::Builtin: break;
            case ast::TypeRef::Named:
                if (!symbols_.contains(t->name)) {
                    diag_.emit(Diagnostic{DiagLevel::Error, "E051",
                        "Unknown type '" + t->name + "'",
                        {file, 0, 0}, {}});
                }
                break;
            case ast::TypeRef::Optional:
            case ast::TypeRef::Repeated:
            case ast::TypeRef::Stream:
                checkTypeRef(t->element_type, file);
                break;
            case ast::TypeRef::Map:
                checkTypeRef(t->key_type, file);
                checkTypeRef(t->value_type, file);
                break;
        }
    }

    void checkStruct(const ast::StructDecl& s, const std::string& file) {
        std::unordered_map<std::string, bool> seen;
        for (auto& f : s.fields) {
            if (seen.count(f.name)) {
                diag_.emit(Diagnostic{DiagLevel::Error, "E052",
                    "Duplicate field '" + f.name + "' in struct '" + s.name + "'",
                    {file, 0, 0}, {}});
            }
            seen[f.name] = true;
            checkTypeRef(f.type, file);
        }
    }

    void checkEnum(const ast::EnumDecl& e, const std::string& file) {
        std::unordered_map<std::string, bool> seen;
        for (auto& v : e.variants) {
            if (seen.count(v.name)) {
                diag_.emit(Diagnostic{DiagLevel::Error, "E053",
                    "Duplicate variant '" + v.name + "' in enum '" + e.name + "'",
                    {file, 0, 0}, {}});
            }
            seen[v.name] = true;
        }
    }

    void checkRpc(const ast::RpcDecl& r, const std::string& file) {
        for (auto& p : r.params) {
            checkTypeRef(p.type, file);
        }
        if (r.return_type) checkTypeRef(r.return_type, file);
    }

    void checkRest(const ast::RestDecl& r, const std::string& file) {
        for (auto& p : r.params) {
            checkTypeRef(p.type, file);
        }
        if (r.return_type) checkTypeRef(r.return_type, file);
    }
};

} // namespace dssl::sema
