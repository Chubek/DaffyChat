#include "ast/ast.h"
#include "ast/ast_builder.hpp"
#include "ast/ast_printer.hpp"
#include "sema/sema.hpp"
#include "codegen/wasm_emitter.hpp"
#include "diag/diagnostic.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>

static std::string read_file(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs) return "";
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

static std::string stem_of(const std::string& path) {
    auto slash = path.rfind('/');
    auto name = (slash == std::string::npos) ? path : path.substr(slash + 1);
    auto dot = name.rfind('.');
    return (dot == std::string::npos) ? name : name.substr(0, dot);
}

static void print_usage() {
    std::cerr << "Usage: daffyscript [options] <source-file>\n"
              << "Options:\n"
              << "  --target <module|program|recipe>  Override file-type detection\n"
              << "  --emit-ast                        Dump AST as JSON to stdout\n"
              << "  --opt <0|1|2|3>                   Optimization level (default: 1)\n"
              << "  --out <file>                      Output path (default: <stem>.wasm)\n"
              << "  --validate                        Validate without emitting\n"
              << "  --no-stdlib                       Suppress standard library linking\n";
}

int main(int argc, char** argv) {
    std::string source_file;
    std::string out_path;
    std::string target_override;
    bool emit_ast = false;
    bool validate_only = false;
    int opt_level = 1;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--target") == 0 && i + 1 < argc) {
            target_override = argv[++i];
        } else if (std::strcmp(argv[i], "--emit-ast") == 0) {
            emit_ast = true;
        } else if (std::strcmp(argv[i], "--opt") == 0 && i + 1 < argc) {
            opt_level = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        } else if (std::strcmp(argv[i], "--validate") == 0) {
            validate_only = true;
        } else if (std::strcmp(argv[i], "--no-stdlib") == 0) {
            (void)0;
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        } else if (argv[i][0] != '-') {
            source_file = argv[i];
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            print_usage();
            return 1;
        }
    }

    if (source_file.empty()) {
        std::cerr << "Error: no source file specified\n";
        print_usage();
        return 1;
    }

    auto source = read_file(source_file);
    if (source.empty()) {
        std::cerr << "Error: cannot read '" << source_file << "'\n";
        return 1;
    }

    if (out_path.empty()) {
        out_path = stem_of(source_file) + ".wasm";
    }

    daffyscript::DiagEngine diag;
    daffyscript::ast::File file;

    if (!daffyscript::ast::ParseFile(source_file, source, file, diag)) {
        diag.print_all(std::cerr);
        return 1;
    }

    if (!target_override.empty()) {
        if (target_override == "module") file.file_type = daffyscript::ast::FileType::Module;
        else if (target_override == "program") file.file_type = daffyscript::ast::FileType::Program;
        else if (target_override == "recipe") file.file_type = daffyscript::ast::FileType::Recipe;
        else {
            std::cerr << "Error: unknown target '" << target_override << "'\n";
            return 1;
        }
    }

    if (emit_ast) {
        std::cout << daffyscript::ast::PrintFile(file) << "\n";
        return 0;
    }

    daffyscript::sema::SemanticAnalyzer sema(diag);
    if (!sema.analyze(file)) {
        diag.print_all(std::cerr);
        return 1;
    }

    if (validate_only) {
        std::cout << "Validation passed.\n";
        return 0;
    }

    (void)opt_level;

    daffyscript::codegen::WasmEmitter emitter(diag);
    if (!emitter.emit(file, out_path)) {
        diag.print_all(std::cerr);
        return 1;
    }

    std::cout << "Compiled " << source_file << " -> " << out_path << "\n";
    return 0;
}
