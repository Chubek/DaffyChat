#pragma once
#include <string>
#include <vector>
#include <iostream>
#include "../lexer/token.h"

namespace daffyscript {

enum class DiagLevel { Note, Warning, Error, Fatal };

struct Diagnostic {
    DiagLevel level;
    std::string code;
    std::string message;
    SourceLocation location;
    std::vector<std::string> notes;
};

class DiagEngine {
public:
    void emit(Diagnostic d) {
        if (d.level == DiagLevel::Error || d.level == DiagLevel::Fatal) {
            error_count_++;
        }
        diagnostics_.push_back(std::move(d));
    }

    bool has_errors() const { return error_count_ > 0; }
    int error_count() const { return error_count_; }
    const std::vector<Diagnostic>& diagnostics() const { return diagnostics_; }

    void print_all(std::ostream& out) const {
        for (auto& d : diagnostics_) {
            switch (d.level) {
                case DiagLevel::Note:    out << "note"; break;
                case DiagLevel::Warning: out << "warning"; break;
                case DiagLevel::Error:   out << "error"; break;
                case DiagLevel::Fatal:   out << "fatal"; break;
            }
            out << "[" << d.code << "]: " << d.message << "\n";
            if (!d.location.file.empty()) {
                out << "  --> " << d.location.file
                    << ":" << d.location.line
                    << ":" << d.location.column << "\n";
            }
            for (auto& note : d.notes) {
                out << "  note: " << note << "\n";
            }
        }
    }

private:
    std::vector<Diagnostic> diagnostics_;
    int error_count_ = 0;
};

} // namespace daffyscript
