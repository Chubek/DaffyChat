#pragma once
#include "../../ast/ast.h"
#include "../../ast/ast_printer.hpp"
#include <string>
#include <fstream>
#include <sstream>
#include <variant>

namespace dssl::codegen {

class CppGenerator {
public:
    explicit CppGenerator(const std::string& ns = "") : namespace_(ns) {}

    bool generate(const ast::File& file, const std::string& out_dir) {
        std::string stem = basename(file.filename);
        std::string ns = namespace_.empty() ? stem : namespace_;

        if (!writeHeader(file, out_dir, stem, ns)) return false;
        if (!writeSkeleton(file, out_dir, stem, ns)) return false;
        return true;
    }

private:
    std::string namespace_;

    static std::string basename(const std::string& path) {
        auto pos = path.rfind('/');
        auto name = (pos == std::string::npos) ? path : path.substr(pos + 1);
        auto dot = name.rfind('.');
        return (dot == std::string::npos) ? name : name.substr(0, dot);
    }

    static std::string typeStr(const ast::TypeRefPtr& t) {
        if (!t) return "void";
        switch (t->kind) {
            case ast::TypeRef::Builtin:
                switch (t->builtin) {
                    case ast::BuiltinKind::String: return "std::string";
                    case ast::BuiltinKind::Int32: return "int32_t";
                    case ast::BuiltinKind::Int64: return "int64_t";
                    case ast::BuiltinKind::Uint32: return "uint32_t";
                    case ast::BuiltinKind::Uint64: return "uint64_t";
                    case ast::BuiltinKind::Float32: return "float";
                    case ast::BuiltinKind::Float64: return "double";
                    case ast::BuiltinKind::Bool: return "bool";
                    case ast::BuiltinKind::Bytes: return "std::vector<uint8_t>";
                    case ast::BuiltinKind::Timestamp: return "int64_t";
                    case ast::BuiltinKind::Duration: return "int64_t";
                    case ast::BuiltinKind::Void: return "void";
                }
                return "void";
            case ast::TypeRef::Named: return t->name;
            case ast::TypeRef::Optional: return "std::optional<" + typeStr(t->element_type) + ">";
            case ast::TypeRef::Repeated: return "std::vector<" + typeStr(t->element_type) + ">";
            case ast::TypeRef::Map: return "std::unordered_map<" + typeStr(t->key_type) + ", " + typeStr(t->value_type) + ">";
            case ast::TypeRef::Stream: return "std::vector<" + typeStr(t->element_type) + ">";
        }
        return "void";
    }

    bool writeHeader(const ast::File& file, const std::string& out_dir,
                     const std::string& stem, const std::string& ns) {
        std::string path = out_dir + "/" + stem + ".generated.hpp";
        std::ofstream os(path);
        if (!os) return false;

        os << "#pragma once\n";
        os << "#include <string>\n#include <vector>\n#include <optional>\n";
        os << "#include <unordered_map>\n#include <cstdint>\n\n";
        os << "namespace " << ns << " {\n\n";

        for (auto& item : file.items) {
            std::visit([&](auto& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, ast::StructDecl>) {
                    if (!v.doc.empty()) os << "/** " << v.doc << " */\n";
                    os << "struct " << v.name << " {\n";
                    for (auto& f : v.fields) {
                        if (!f.doc.empty()) os << "    /** " << f.doc << " */\n";
                        os << "    " << typeStr(f.type) << " " << f.name << ";\n";
                    }
                    os << "};\n\n";
                } else if constexpr (std::is_same_v<T, ast::EnumDecl>) {
                    if (!v.doc.empty()) os << "/** " << v.doc << " */\n";
                    os << "enum class " << v.name << " {\n";
                    for (size_t i = 0; i < v.variants.size(); ++i) {
                        os << "    " << v.variants[i].name;
                        if (v.variants[i].value) os << " = " << *v.variants[i].value;
                        if (i + 1 < v.variants.size()) os << ",";
                        os << "\n";
                    }
                    os << "};\n\n";
                } else if constexpr (std::is_same_v<T, ast::UnionDecl>) {
                    if (!v.doc.empty()) os << "/** " << v.doc << " */\n";
                    os << "using " << v.name << " = std::variant<";
                    for (size_t i = 0; i < v.variants.size(); ++i) {
                        if (i) os << ", ";
                        os << typeStr(v.variants[i].type);
                    }
                    os << ">;\n\n";
                } else if constexpr (std::is_same_v<T, ast::ConstDecl>) {
                    os << "inline constexpr auto " << v.name << " = ";
                    std::visit([&](auto& cv) {
                        using CV = std::decay_t<decltype(cv)>;
                        if constexpr (std::is_same_v<CV, std::string>)
                            os << "\"" << cv << "\"";
                        else if constexpr (std::is_same_v<CV, bool>)
                            os << (cv ? "true" : "false");
                        else
                            os << cv;
                    }, v.value);
                    os << ";\n\n";
                }
            }, item);
        }

        os << "} // namespace " << ns << "\n";
        return true;
    }

    bool writeSkeleton(const ast::File& file, const std::string& out_dir,
                       const std::string& stem, const std::string& ns) {
        std::string path = out_dir + "/" + stem + ".skeleton.cpp";
        std::ofstream os(path);
        if (!os) return false;

        os << "#include \"" << stem << ".generated.hpp\"\n\n";
        os << "namespace " << ns << " {\n\n";

        for (auto& item : file.items) {
            std::visit([&](auto& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, ast::RpcDecl>) {
                    std::string ret = v.return_type ? typeStr(v.return_type) : "void";
                    os << ret << " " << v.name << "(";
                    for (size_t i = 0; i < v.params.size(); ++i) {
                        if (i) os << ", ";
                        os << typeStr(v.params[i].type) << " " << v.params[i].name;
                    }
                    os << ") {\n    // TODO: implement\n}\n\n";
                }
            }, item);
        }

        os << "} // namespace " << ns << "\n";
        return true;
    }
};

} // namespace dssl::codegen
