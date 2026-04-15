#include "../../ast/ast.h"
#include "../../ast/ast_builder.hpp"
#include "../../ast/ast_printer.hpp"
#include "../../diag/diagnostic.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>

static std::string read_file(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs) return "";
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

static std::string escape_json(const std::string& s) {
    return dssl::ast::EscapeJson(s);
}

int main(int argc, char** argv) {
    std::string source_file;
    bool as_yaml = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--yaml") == 0) {
            as_yaml = true;
        } else if (argv[i][0] != '-') {
            source_file = argv[i];
        }
    }

    if (source_file.empty()) {
        std::cerr << "Usage: dssl-docstrip [--yaml] <source.dssl>\n";
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

    if (as_yaml) {
        if (file.service) {
            std::cout << "service:\n";
            std::cout << "  name: " << file.service->name << "\n";
            if (!file.service->doc.empty())
                std::cout << "  doc: |\n    " << file.service->doc << "\n";
        }
        std::cout << "items:\n";
        for (auto& item : file.items) {
            std::visit([](auto& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, dssl::ast::StructDecl>) {
                    if (!v.doc.empty())
                        std::cout << "  - struct: " << v.name << "\n    doc: |\n      " << v.doc << "\n";
                } else if constexpr (std::is_same_v<T, dssl::ast::RpcDecl>) {
                    if (!v.doc.empty())
                        std::cout << "  - rpc: " << v.name << "\n    doc: |\n      " << v.doc << "\n";
                } else if constexpr (std::is_same_v<T, dssl::ast::EnumDecl>) {
                    if (!v.doc.empty())
                        std::cout << "  - enum: " << v.name << "\n    doc: |\n      " << v.doc << "\n";
                }
            }, item);
        }
    } else {
        std::cout << "{\"docs\":[";
        bool first = true;
        auto emit = [&](const std::string& kind, const std::string& name, const std::string& doc) {
            if (doc.empty()) return;
            if (!first) std::cout << ",";
            first = false;
            std::cout << "{\"kind\":\"" << kind << "\",\"name\":\"" << escape_json(name)
                      << "\",\"doc\":\"" << escape_json(doc) << "\"}";
        };
        if (file.service && !file.service->doc.empty()) {
            emit("service", file.service->name, file.service->doc);
        }
        for (auto& item : file.items) {
            std::visit([&](auto& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, dssl::ast::StructDecl>)
                    emit("struct", v.name, v.doc);
                else if constexpr (std::is_same_v<T, dssl::ast::EnumDecl>)
                    emit("enum", v.name, v.doc);
                else if constexpr (std::is_same_v<T, dssl::ast::UnionDecl>)
                    emit("union", v.name, v.doc);
                else if constexpr (std::is_same_v<T, dssl::ast::RpcDecl>)
                    emit("rpc", v.name, v.doc);
                else if constexpr (std::is_same_v<T, dssl::ast::ExecDecl>)
                    emit("exec", v.name, v.doc);
                else if constexpr (std::is_same_v<T, dssl::ast::ConstDecl>)
                    emit("const", v.name, v.doc);
            }, item);
        }
        std::cout << "]}\n";
    }

    return 0;
}
