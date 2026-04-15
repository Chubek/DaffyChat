#pragma once
#include "../ast/ast.h"
#include "../diag/diagnostic.hpp"
#include "symbol_table.hpp"

namespace daffyscript::sema {

class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(DiagEngine& diag) : diag_(diag) {}

    bool analyze(const ast::File& file) {
        symbols_.push_scope();

        if (file.file_type == ast::FileType::Module) {
            validateModule(file);
        } else if (file.file_type == ast::FileType::Program) {
            validateProgram(file);
        } else if (file.file_type == ast::FileType::Recipe) {
            validateRecipe(file);
        }

        if (file.version.empty()) {
            diag_.emit(Diagnostic{DiagLevel::Error, "E010",
                "Missing version declaration",
                {file.filename, 0, 0}, {}});
        }

        symbols_.pop_scope();
        return !diag_.has_errors();
    }

    const SymbolTable& symbols() const { return symbols_; }

private:
    DiagEngine& diag_;
    SymbolTable symbols_;

    void validateModule(const ast::File& file) {
        if (file.name.empty()) {
            diag_.emit(Diagnostic{DiagLevel::Error, "E100",
                "Module must have a name", {file.filename, 0, 0}, {}});
        }
    }

    void validateProgram(const ast::File& file) {
        if (file.name.empty()) {
            diag_.emit(Diagnostic{DiagLevel::Error, "E110",
                "Program must have a name", {file.filename, 0, 0}, {}});
        }
    }

    void validateRecipe(const ast::File& file) {
        if (file.name.empty()) {
            diag_.emit(Diagnostic{DiagLevel::Error, "E120",
                "Recipe must have a name", {file.filename, 0, 0}, {}});
        }
    }
};

} // namespace daffyscript::sema
