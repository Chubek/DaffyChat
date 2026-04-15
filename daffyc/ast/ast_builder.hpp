#pragma once

#include "ast.h"
#include "../parser/grammar.hpp"
#include "../diag/diagnostic.hpp"

#include <tao/pegtl.hpp>
#include <fstream>
#include <sstream>

namespace daffyscript::ast {

namespace pegtl = tao::pegtl;

struct BuildState {
    File file;
    DiagEngine& diag;

    std::vector<std::string> string_stack;
    std::vector<int64_t> int_stack;

    explicit BuildState(DiagEngine& d) : diag(d) {}

    std::string pop_string() {
        if (string_stack.empty()) return "";
        auto s = std::move(string_stack.back());
        string_stack.pop_back();
        return s;
    }
};

template<typename Rule>
struct action : pegtl::nothing<Rule> {};

template<>
struct action<grammar::identifier> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, BuildState& state) {
        state.string_stack.push_back(in.string());
    }
};

template<>
struct action<grammar::string_lit> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, BuildState& state) {
        auto s = in.string();
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
            s = s.substr(1, s.size() - 2);
        state.string_stack.push_back(std::move(s));
    }
};

template<>
struct action<grammar::version_lit> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, BuildState& state) {
        state.string_stack.push_back(in.string());
    }
};

template<>
struct action<grammar::integer_lit> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, BuildState& state) {
        state.int_stack.push_back(std::stoll(in.string()));
    }
};

template<>
struct action<grammar::module_header> {
    template<typename ActionInput>
    static void apply(const ActionInput&, BuildState& state) {
        state.file.file_type = FileType::Module;
        state.file.name = state.pop_string();
    }
};

template<>
struct action<grammar::program_header> {
    template<typename ActionInput>
    static void apply(const ActionInput&, BuildState& state) {
        state.file.file_type = FileType::Program;
        state.file.name = state.pop_string();
    }
};

template<>
struct action<grammar::recipe_header> {
    template<typename ActionInput>
    static void apply(const ActionInput&, BuildState& state) {
        state.file.file_type = FileType::Recipe;
        state.file.name = state.pop_string();
    }
};

template<>
struct action<grammar::version_decl> {
    template<typename ActionInput>
    static void apply(const ActionInput&, BuildState& state) {
        state.file.version = state.pop_string();
    }
};

enum class DetectedFileType { Unknown, Module, Program, Recipe };

inline DetectedFileType DetectFileType(const std::string& source) {
    size_t pos = 0;
    while (pos < source.size() && (source[pos] == ' ' || source[pos] == '\t' ||
           source[pos] == '\n' || source[pos] == '\r')) pos++;
    if (pos + 2 < source.size() && source[pos] == '-' && source[pos+1] == '-') {
        while (pos < source.size() && source[pos] != '\n') pos++;
        while (pos < source.size() && (source[pos] == ' ' || source[pos] == '\t' ||
               source[pos] == '\n' || source[pos] == '\r')) pos++;
    }
    auto remaining = source.substr(pos);
    if (remaining.substr(0, 6) == "module") return DetectedFileType::Module;
    if (remaining.substr(0, 7) == "program") return DetectedFileType::Program;
    if (remaining.substr(0, 6) == "recipe") return DetectedFileType::Recipe;
    return DetectedFileType::Unknown;
}

inline bool ParseFile(const std::string& filename, const std::string& source,
                      File& out, DiagEngine& diag) {
    BuildState state(diag);
    state.file.filename = filename;

    pegtl::memory_input input(source, filename);

    auto file_type = DetectFileType(source);

    try {
        bool ok = false;
        switch (file_type) {
            case DetectedFileType::Module:
                ok = pegtl::parse<grammar::module_file, action>(input, state);
                break;
            case DetectedFileType::Program:
                ok = pegtl::parse<grammar::program_file, action>(input, state);
                break;
            case DetectedFileType::Recipe:
                ok = pegtl::parse<grammar::recipe_file, action>(input, state);
                break;
            case DetectedFileType::Unknown:
                diag.emit(Diagnostic{DiagLevel::Error, "E001",
                    "Cannot determine file type: expected 'module', 'program', or 'recipe' header",
                    {filename, 1, 1}, {}});
                return false;
        }
        if (ok) {
            out = std::move(state.file);
            return true;
        }
        diag.emit(Diagnostic{DiagLevel::Error, "E002", "Parse failed",
            {filename, 0, 0}, {}});
        return false;
    } catch (const pegtl::parse_error& e) {
        diag.emit(Diagnostic{DiagLevel::Error, "E002", e.what(),
            {filename, 0, 0}, {}});
        return false;
    }
}

} // namespace daffyscript::ast
