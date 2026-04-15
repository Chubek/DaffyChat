#include "../../ast/ast.h"
#include "../../ast/ast_builder.hpp"
#include "../../ast/ast_printer.hpp"
#include "../../sema/sema.hpp"
#include "../../codegen/json/json_gen.hpp"
#include "../../codegen/cpp/cpp_gen.hpp"
#include "../../diag/diagnostic.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>

static std::string read_file(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs) return "";
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

static void print_usage() {
    std::cerr << "Usage: dssl-bindgen [options] <source.dssl>\n"
              << "Options:\n"
              << "  --target <cpp|json>  Code generation target (repeatable)\n"
              << "  --out-dir <dir>      Output directory (default: .)\n"
              << "  --gen-ast            Dump AST as JSON to stdout and exit\n"
              << "  --validate           Validate only, no output\n"
              << "  --namespace <ns>     Override namespace for generated code\n";
}

int main(int argc, char** argv) {
    std::string source_file;
    std::string out_dir = ".";
    std::string ns;
    std::vector<std::string> targets;
    bool gen_ast = false;
    bool validate_only = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--target") == 0 && i + 1 < argc) {
            targets.push_back(argv[++i]);
        } else if (std::strcmp(argv[i], "--out-dir") == 0 && i + 1 < argc) {
            out_dir = argv[++i];
        } else if (std::strcmp(argv[i], "--namespace") == 0 && i + 1 < argc) {
            ns = argv[++i];
        } else if (std::strcmp(argv[i], "--gen-ast") == 0) {
            gen_ast = true;
        } else if (std::strcmp(argv[i], "--validate") == 0) {
            validate_only = true;
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

    dssl::DiagEngine diag;
    dssl::ast::File file;

    if (!dssl::ast::ParseFile(source_file, source, file, diag)) {
        diag.print_all(std::cerr);
        return 1;
    }

    if (gen_ast) {
        std::cout << dssl::ast::PrintFile(file) << "\n";
        return 0;
    }

    dssl::sema::SemanticAnalyzer sema(diag);
    if (!sema.analyze(file)) {
        diag.print_all(std::cerr);
        return 1;
    }

    if (validate_only) {
        std::cout << "Validation passed.\n";
        return 0;
    }

    if (targets.empty()) {
        targets.push_back("json");
    }

    for (auto& target : targets) {
        if (target == "json") {
            dssl::codegen::JsonGenerator gen;
            if (!gen.generate(file, out_dir)) {
                std::cerr << "Error: JSON generation failed\n";
                return 1;
            }
        } else if (target == "cpp") {
            dssl::codegen::CppGenerator gen(ns);
            if (!gen.generate(file, out_dir)) {
                std::cerr << "Error: C++ generation failed\n";
                return 1;
            }
        } else {
            std::cerr << "Warning: unknown target '" << target << "', skipping\n";
        }
    }

    std::cout << "Generated " << targets.size() << " target(s) to " << out_dir << "\n";
    return 0;
}
