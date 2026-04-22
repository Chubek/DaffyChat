#pragma once
#include "../../ast/ast.h"
#include "../../ast/ast_printer.hpp"
#include <cctype>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_set>
#include <variant>

namespace dssl::codegen {

class CppGenerator {
public:
    explicit CppGenerator(const std::string& ns = "") : namespace_(ns) {}

    bool generate(const ast::File& file, const std::string& out_dir) {
        std::string stem = basename(file.filename);
        std::string ns = namespace_.empty() ? stem : namespace_;
        const auto unsupported_rpcs = unsupportedAdapterReasons(file);

        if (!unsupported_rpcs.empty()) {
            for (const auto& reason : unsupported_rpcs) {
                std::cerr << "Error: " << reason << "\n";
            }
            return false;
        }

        if (!writeHeader(file, out_dir, stem, ns)) return false;
        if (!writeSkeleton(file, out_dir, stem, ns)) return false;
        if (!writeServiceHeader(file, out_dir, stem, ns)) return false;
        if (!writeServiceSource(file, out_dir, stem, ns)) return false;
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

    static std::string serviceClassName(const ast::File& file, const std::string& stem) {
        std::string base = stem;
        if (file.service.has_value() && !file.service->name.empty()) {
            base = file.service->name;
        }
        if (!base.empty()) {
            base[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(base[0])));
        }
        return base + "GeneratedService";
    }

    static std::string serviceTopic(const ast::File& file, const std::string& stem) {
        const std::string name = file.service.has_value() && !file.service->name.empty() ? file.service->name : stem;
        return "service." + name;
    }

    static std::string jsonExtractExpr(const ast::TypeRefPtr& t, const std::string& value_expr) {
        if (!t) return "{}";
        const std::string wrapped = "(" + value_expr + ")";
        switch (t->kind) {
            case ast::TypeRef::Builtin:
                switch (t->builtin) {
                    case ast::BuiltinKind::String: return wrapped + ".AsString()";
                    case ast::BuiltinKind::Bool: return wrapped + ".AsBool()";
                    case ast::BuiltinKind::Int32: return "static_cast<int32_t>(" + wrapped + ".AsNumber())";
                    case ast::BuiltinKind::Int64: return "static_cast<int64_t>(" + wrapped + ".AsNumber())";
                    case ast::BuiltinKind::Uint32: return "static_cast<uint32_t>(" + wrapped + ".AsNumber())";
                    case ast::BuiltinKind::Uint64: return "static_cast<uint64_t>(" + wrapped + ".AsNumber())";
                    case ast::BuiltinKind::Float32: return "static_cast<float>(" + wrapped + ".AsNumber())";
                    case ast::BuiltinKind::Float64: return wrapped + ".AsNumber()";
                    default: return "{}";
                }
            case ast::TypeRef::Named: return "Parse" + t->name + "(" + wrapped + ").value()";
            default: return "{}";
        }
    }

    static std::string jsonStoreExpr(const ast::TypeRefPtr& t, const std::string& value_expr) {
        if (!t) return "daffy::util::json::Value()";
        switch (t->kind) {
            case ast::TypeRef::Builtin:
                return "daffy::util::json::Value(" + value_expr + ")";
            case ast::TypeRef::Named:
                return t->name + "ToJson(" + value_expr + ")";
            default:
                return "daffy::util::json::Value()";
        }
    }

    static bool supportsAdapter(const ast::RpcDecl& rpc) {
        for (const auto& param : rpc.params) {
            if (!supportsAdapterType(param.type)) {
                return false;
            }
        }
        return rpc.return_type && supportsAdapterType(rpc.return_type);
    }

    static bool supportsAdapterType(const ast::TypeRefPtr& t) {
        if (!t) return false;
        if (t->kind == ast::TypeRef::Builtin) {
            return t->builtin == ast::BuiltinKind::String || t->builtin == ast::BuiltinKind::Bool ||
                   t->builtin == ast::BuiltinKind::Int32 || t->builtin == ast::BuiltinKind::Int64 ||
                   t->builtin == ast::BuiltinKind::Uint32 || t->builtin == ast::BuiltinKind::Uint64 ||
                   t->builtin == ast::BuiltinKind::Float32 || t->builtin == ast::BuiltinKind::Float64;
        }
        return t->kind == ast::TypeRef::Named;
    }

    static std::string adapterTypeDiagnostic(const ast::TypeRefPtr& t) {
        if (!t) {
            return "missing type";
        }
        switch (t->kind) {
            case ast::TypeRef::Builtin:
                switch (t->builtin) {
                    case ast::BuiltinKind::String:
                    case ast::BuiltinKind::Bool:
                    case ast::BuiltinKind::Int32:
                    case ast::BuiltinKind::Int64:
                    case ast::BuiltinKind::Uint32:
                    case ast::BuiltinKind::Uint64:
                    case ast::BuiltinKind::Float32:
                    case ast::BuiltinKind::Float64:
                        return "";
                    default:
                        return "builtin type `" + typeStr(t) + "` is not adapter-compatible yet";
                }
            case ast::TypeRef::Named:
                return "";
            case ast::TypeRef::Optional:
                return "optional type `" + typeStr(t) + "` is not adapter-compatible yet";
            case ast::TypeRef::Repeated:
                return "repeated type `" + typeStr(t) + "` is not adapter-compatible yet";
            case ast::TypeRef::Map:
                return "map type `" + typeStr(t) + "` is not adapter-compatible yet";
            case ast::TypeRef::Stream:
                return "stream type `" + typeStr(t) + "` is not adapter-compatible yet";
        }
        return "unknown type is not adapter-compatible yet";
    }

    static std::vector<std::string> unsupportedAdapterReasons(const ast::File& file) {
        std::vector<std::string> reasons;
        for (const auto& item : file.items) {
            const auto* rpc = std::get_if<ast::RpcDecl>(&item);
            if (rpc == nullptr) {
                continue;
            }

            for (const auto& param : rpc->params) {
                const auto reason = adapterTypeDiagnostic(param.type);
                if (!reason.empty()) {
                    reasons.push_back("RPC `" + rpc->name + "` parameter `" + param.name + "` uses unsupported adapter type: " + reason);
                }
            }

            if (rpc->return_type == nullptr) {
                reasons.push_back("RPC `" + rpc->name + "` is missing a return type, so no service adapter can be generated");
                continue;
            }

            const auto return_reason = adapterTypeDiagnostic(rpc->return_type);
            if (!return_reason.empty()) {
                reasons.push_back("RPC `" + rpc->name + "` return type is unsupported for the generated adapter: " + return_reason);
            }
        }
        return reasons;
    }

    static std::vector<const ast::RpcDecl*> adapterCompatibleRpcs(const ast::File& file) {
        std::vector<const ast::RpcDecl*> rpcs;
        for (const auto& item : file.items) {
            if (const auto* rpc = std::get_if<ast::RpcDecl>(&item); rpc != nullptr && supportsAdapter(*rpc)) {
                rpcs.push_back(rpc);
            }
        }
        return rpcs;
    }

    bool writeHeader(const ast::File& file, const std::string& out_dir,
                     const std::string& stem, const std::string& ns) {
        std::string path = out_dir + "/" + stem + ".generated.hpp";
        std::ofstream os(path);
        if (!os) return false;

        os << "#pragma once\n";
        os << "#include <string>\n#include <vector>\n#include <optional>\n";
        os << "#include <unordered_map>\n#include <cstdint>\n\n";
        os << "#include \"daffy/core/error.hpp\"\n";
        os << "#include \"daffy/util/json.hpp\"\n\n";
        os << "namespace " << ns << " {\n\n";

        std::unordered_set<std::string> struct_names;
        for (const auto& item : file.items) {
            if (const auto* sd = std::get_if<ast::StructDecl>(&item)) {
                struct_names.insert(sd->name);
            }
        }

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
                    os << "daffy::util::json::Value " << v.name << "ToJson(const " << v.name << "& value);\n";
                    os << "daffy::core::Result<" << v.name << "> Parse" << v.name
                       << "(const daffy::util::json::Value& value);\n\n";
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
                } else if constexpr (std::is_same_v<T, ast::RpcDecl>) {
                    if (v.return_type != nullptr) {
                        os << typeStr(v.return_type) << " " << v.name << "(";
                        for (size_t i = 0; i < v.params.size(); ++i) {
                            if (i) os << ", ";
                            os << typeStr(v.params[i].type) << " " << v.params[i].name;
                        }
                        os << ");\n\n";
                    }
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

        for (const auto& item : file.items) {
            if (const auto* sd = std::get_if<ast::StructDecl>(&item)) {
                os << "daffy::util::json::Value " << sd->name << "ToJson(const " << sd->name << "& value) {\n";
                os << "    return daffy::util::json::Value::Object{";
                for (size_t i = 0; i < sd->fields.size(); ++i) {
                    if (i) os << ", ";
                    os << "{\"" << sd->fields[i].name << "\", " << jsonStoreExpr(sd->fields[i].type, "value." + sd->fields[i].name) << "}";
                }
                os << "};\n}\n\n";

                os << "daffy::core::Result<" << sd->name << "> Parse" << sd->name
                   << "(const daffy::util::json::Value& value) {\n";
                os << "    if (!value.IsObject()) {\n";
                os << "        return daffy::core::Error{daffy::core::ErrorCode::kParseError, \"" << sd->name
                   << " must be a JSON object\"};\n";
                os << "    }\n";
                os << "    " << sd->name << " parsed{};\n";
                for (const auto& field : sd->fields) {
                    os << "    const auto* " << field.name << "_value = value.Find(\"" << field.name << "\");\n";
                    os << "    if (" << field.name << "_value == nullptr) {\n";
                    os << "        return daffy::core::Error{daffy::core::ErrorCode::kParseError, \"Missing field `" << field.name
                       << "` in " << sd->name << "\"};\n";
                    os << "    }\n";
                    os << "    parsed." << field.name << " = " << jsonExtractExpr(field.type, "*" + field.name + "_value") << ";\n";
                }
                os << "    return parsed;\n}\n\n";
            }
        }

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
                    os << ") {\n";
                    if (v.return_type && v.return_type->kind == ast::TypeRef::Named) {
                        os << "    " << ret << " result{};\n";
                        const auto return_type = v.return_type->name;
                        for (const auto& tl_item : file.items) {
                            if (const auto* sd = std::get_if<ast::StructDecl>(&tl_item); sd != nullptr && sd->name == return_type) {
                                for (const auto& field : sd->fields) {
                                    bool assigned = false;
                                    for (const auto& param : v.params) {
                                        if (param.name == field.name && typeStr(param.type) == typeStr(field.type)) {
                                            os << "    result." << field.name << " = " << field.name << ";\n";
                                            assigned = true;
                                            break;
                                        }
                                    }
                                    if (!assigned) {
                                        if (field.type->kind == ast::TypeRef::Builtin && field.type->builtin == ast::BuiltinKind::String) {
                                            const std::string default_string =
                                                field.name == "service_name"
                                                    ? (file.service.has_value() && !file.service->name.empty() ? file.service->name : stem)
                                                    : field.name;
                                            os << "    result." << field.name << " = \"" << default_string << "\";\n";
                                        } else if (field.type->kind == ast::TypeRef::Builtin && field.type->builtin == ast::BuiltinKind::Bool) {
                                            os << "    result." << field.name << " = true;\n";
                                        } else {
                                            os << "    result." << field.name << " = {};\n";
                                        }
                                    }
                                }
                            }
                        }
                        os << "    return result;\n";
                    } else {
                        os << "    return {};\n";
                    }
                    os << "}\n\n";
                }
            }, item);
        }

        os << "} // namespace " << ns << "\n";
        return true;
    }

    bool writeServiceHeader(const ast::File& file, const std::string& out_dir,
                            const std::string& stem, const std::string& ns) {
        std::string path = out_dir + "/" + stem + ".service.hpp";
        std::ofstream os(path);
        if (!os) return false;

        const std::string class_name = serviceClassName(file, stem);
        os << "#pragma once\n";
        os << "#include <string>\n\n";
        os << "#include \"daffy/core/error.hpp\"\n";
        os << "#include \"daffy/ipc/nng_transport.hpp\"\n";
        os << "#include \"daffy/services/service_metadata.hpp\"\n";
        os << "#include \"" << stem << ".generated.hpp\"\n\n";
        os << "namespace " << ns << " {\n\n";
        os << "class " << class_name << " {\n";
        os << "public:\n";
        os << "    static daffy::services::ServiceMetadata Metadata();\n";
        os << "    daffy::core::Result<daffy::ipc::MessageEnvelope> Handle(const daffy::ipc::MessageEnvelope& request) const;\n";
        os << "    daffy::core::Status Bind(daffy::ipc::NngRequestReplyTransport& transport, std::string url) const;\n";
        os << "};\n\n";
        os << "} // namespace " << ns << "\n";
        return true;
    }

    bool writeServiceSource(const ast::File& file, const std::string& out_dir,
                            const std::string& stem, const std::string& ns) {
        std::string path = out_dir + "/" + stem + ".service.cpp";
        std::ofstream os(path);
        if (!os) return false;

        const std::string class_name = serviceClassName(file, stem);
        const std::string topic = serviceTopic(file, stem);
        os << "#include \"" << stem << ".service.hpp\"\n\n";
        os << "namespace " << ns << " {\n\n";
        os << "namespace {\n";
        os << "constexpr char kTopic[] = \"" << topic << "\";\n";
        os << "constexpr char kRequestType[] = \"request\";\n";
        os << "constexpr char kReplyType[] = \"reply\";\n";
        os << "} // namespace\n\n";
        os << "daffy::services::ServiceMetadata " << class_name << "::Metadata() {\n";
        os << "    return daffy::services::ServiceMetadata{\""
           << (file.service.has_value() ? file.service->name : stem) << "\", \""
           << (file.service.has_value() ? file.service->version : "0.1.0") << "\", \""
           << (file.service.has_value() ? file.service->doc : "Generated DSSL service") << "\", \"./" << stem
           << ".service.cpp\", {\"ipc\"}, true};\n";
        os << "}\n\n";

        os << "daffy::core::Result<daffy::ipc::MessageEnvelope> " << class_name
           << "::Handle(const daffy::ipc::MessageEnvelope& request) const {\n";
        os << "    if (request.topic != kTopic) {\n";
        os << "        return daffy::core::Error{daffy::core::ErrorCode::kInvalidArgument, \"Unexpected service topic\"};\n";
        os << "    }\n";
        os << "    if (request.type != kRequestType) {\n";
        os << "        return daffy::core::Error{daffy::core::ErrorCode::kInvalidArgument, \"Unexpected service message type\"};\n";
        os << "    }\n";

        const auto supported_rpcs = adapterCompatibleRpcs(file);
        if (supported_rpcs.size() > 1) {
            os << "    const auto* rpc_name_value = request.payload.Find(\"rpc\");\n";
            os << "    if (rpc_name_value == nullptr || !rpc_name_value->IsString()) {\n";
            os << "        return daffy::core::Error{daffy::core::ErrorCode::kParseError, \"Multi-RPC services require a string `rpc` field in the request payload\"};\n";
            os << "    }\n";
            os << "    const auto rpc_name = rpc_name_value->AsString();\n";
        }

        bool emitted_rpc = false;
        for (const auto* rpc : supported_rpcs) {
            emitted_rpc = true;
            if (supported_rpcs.size() > 1) {
                os << "    if (rpc_name == \"" << rpc->name << "\") {\n";
            } else {
                os << "    {\n";
            }
            for (const auto& param : rpc->params) {
                os << "        const auto* " << param.name << "_value = request.payload.Find(\"" << param.name << "\");\n";
                os << "        if (" << param.name << "_value == nullptr) {\n";
                os << "            return daffy::core::Error{daffy::core::ErrorCode::kParseError, \"Missing field `" << param.name << "` in request payload\"};\n";
                os << "        }\n";
                os << "        const auto " << param.name << " = " << jsonExtractExpr(param.type, "*" + param.name + "_value") << ";\n";
            }
            os << "        const auto result = " << rpc->name << "(";
            for (size_t i = 0; i < rpc->params.size(); ++i) {
                if (i) os << ", ";
                os << rpc->params[i].name;
            }
            os << ");\n";
            os << "        return daffy::ipc::MessageEnvelope{kTopic, kReplyType, "
               << jsonStoreExpr(rpc->return_type, "result") << "};\n";
            os << "    }\n";
        }
        if (!emitted_rpc) {
            os << "    return daffy::core::Error{daffy::core::ErrorCode::kUnavailable, \"No adapter-compatible RPC generated for this service\"};\n";
        } else if (supported_rpcs.size() > 1) {
            os << "    return daffy::core::Error{daffy::core::ErrorCode::kInvalidArgument, \"Unknown RPC requested for this service\"};\n";
        }
        os << "}\n\n";

        os << "daffy::core::Status " << class_name
           << "::Bind(daffy::ipc::NngRequestReplyTransport& transport, std::string url) const {\n";
        os << "    return transport.Bind(std::move(url), [this](const daffy::ipc::MessageEnvelope& request) { return Handle(request); });\n";
        os << "}\n\n";
        os << "} // namespace " << ns << "\n";
        return true;
    }
};

} // namespace dssl::codegen
