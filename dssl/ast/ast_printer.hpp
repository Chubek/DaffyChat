#pragma once
#include "ast.h"
#include <string>
#include <sstream>

namespace dssl::ast {

inline std::string TypeToString(const TypeRefPtr& t) {
    if (!t) return "null";
    switch (t->kind) {
        case TypeRef::Builtin: {
            switch (t->builtin) {
                case BuiltinKind::String: return "\"string\"";
                case BuiltinKind::Int32: return "\"int32\"";
                case BuiltinKind::Int64: return "\"int64\"";
                case BuiltinKind::Uint32: return "\"uint32\"";
                case BuiltinKind::Uint64: return "\"uint64\"";
                case BuiltinKind::Float32: return "\"float32\"";
                case BuiltinKind::Float64: return "\"float64\"";
                case BuiltinKind::Bool: return "\"bool\"";
                case BuiltinKind::Bytes: return "\"bytes\"";
                case BuiltinKind::Timestamp: return "\"timestamp\"";
                case BuiltinKind::Duration: return "\"duration\"";
                case BuiltinKind::Void: return "\"void\"";
            }
            return "\"unknown\"";
        }
        case TypeRef::Named: return "{\"named\":\"" + t->name + "\"}";
        case TypeRef::Optional: return "{\"optional\":" + TypeToString(t->element_type) + "}";
        case TypeRef::Repeated: return "{\"repeated\":" + TypeToString(t->element_type) + "}";
        case TypeRef::Map: return "{\"map\":{\"key\":" + TypeToString(t->key_type) + ",\"value\":" + TypeToString(t->value_type) + "}}";
        case TypeRef::Stream: return "{\"stream\":" + TypeToString(t->element_type) + "}";
    }
    return "null";
}

inline std::string EscapeJson(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out;
}

inline std::string PrintFile(const File& file) {
    std::ostringstream os;
    os << "{";
    os << "\"filename\":\"" << EscapeJson(file.filename) << "\"";

    if (file.service) {
        os << ",\"service\":{\"name\":\"" << EscapeJson(file.service->name)
           << "\",\"version\":\"" << EscapeJson(file.service->version) << "\"";
        if (!file.service->doc.empty())
            os << ",\"doc\":\"" << EscapeJson(file.service->doc) << "\"";
        os << "}";
    }

    os << ",\"imports\":[";
    for (size_t i = 0; i < file.imports.size(); ++i) {
        if (i) os << ",";
        os << "\"" << EscapeJson(file.imports[i].path) << "\"";
    }
    os << "]";

    os << ",\"items\":[";
    bool first_item = true;
    for (auto& item : file.items) {
        if (!first_item) os << ",";
        first_item = false;

        std::visit([&](auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, StructDecl>) {
                os << "{\"kind\":\"struct\",\"name\":\"" << EscapeJson(v.name) << "\"";
                if (!v.doc.empty()) os << ",\"doc\":\"" << EscapeJson(v.doc) << "\"";
                os << ",\"fields\":[";
                for (size_t i = 0; i < v.fields.size(); ++i) {
                    if (i) os << ",";
                    os << "{\"name\":\"" << EscapeJson(v.fields[i].name)
                       << "\",\"type\":" << TypeToString(v.fields[i].type);
                    if (v.fields[i].deprecated) os << ",\"deprecated\":true";
                    if (!v.fields[i].doc.empty()) os << ",\"doc\":\"" << EscapeJson(v.fields[i].doc) << "\"";
                    os << "}";
                }
                os << "]}";
            } else if constexpr (std::is_same_v<T, EnumDecl>) {
                os << "{\"kind\":\"enum\",\"name\":\"" << EscapeJson(v.name) << "\"";
                if (!v.doc.empty()) os << ",\"doc\":\"" << EscapeJson(v.doc) << "\"";
                os << ",\"variants\":[";
                for (size_t i = 0; i < v.variants.size(); ++i) {
                    if (i) os << ",";
                    os << "{\"name\":\"" << EscapeJson(v.variants[i].name) << "\"";
                    if (v.variants[i].value) os << ",\"value\":" << *v.variants[i].value;
                    os << "}";
                }
                os << "]}";
            } else if constexpr (std::is_same_v<T, UnionDecl>) {
                os << "{\"kind\":\"union\",\"name\":\"" << EscapeJson(v.name) << "\"";
                if (!v.doc.empty()) os << ",\"doc\":\"" << EscapeJson(v.doc) << "\"";
                os << ",\"variants\":[";
                for (size_t i = 0; i < v.variants.size(); ++i) {
                    if (i) os << ",";
                    os << "{\"name\":\"" << EscapeJson(v.variants[i].name)
                       << "\",\"type\":" << TypeToString(v.variants[i].type) << "}";
                }
                os << "]}";
            } else if constexpr (std::is_same_v<T, RpcDecl>) {
                os << "{\"kind\":\"rpc\",\"name\":\"" << EscapeJson(v.name) << "\"";
                if (!v.doc.empty()) os << ",\"doc\":\"" << EscapeJson(v.doc) << "\"";
                if (v.deprecated) os << ",\"deprecated\":true";
                os << ",\"params\":[";
                for (size_t i = 0; i < v.params.size(); ++i) {
                    if (i) os << ",";
                    os << "{\"name\":\"" << EscapeJson(v.params[i].name)
                       << "\",\"type\":" << TypeToString(v.params[i].type) << "}";
                }
                os << "]";
                if (v.return_type) os << ",\"returns\":" << TypeToString(v.return_type);
                os << "}";
            } else if constexpr (std::is_same_v<T, RestDecl>) {
                os << "{\"kind\":\"rest\",\"method\":\"";
                switch (v.method) {
                    case HttpMethod::Get: os << "GET"; break;
                    case HttpMethod::Post: os << "POST"; break;
                    case HttpMethod::Put: os << "PUT"; break;
                    case HttpMethod::Patch: os << "PATCH"; break;
                    case HttpMethod::Delete: os << "DELETE"; break;
                }
                os << "\",\"path\":\"" << EscapeJson(v.name_path) << "\"";
                os << ",\"params\":[";
                for (size_t i = 0; i < v.params.size(); ++i) {
                    if (i) os << ",";
                    os << "{\"name\":\"" << EscapeJson(v.params[i].name)
                       << "\",\"type\":" << TypeToString(v.params[i].type) << "}";
                }
                os << "]";
                if (v.return_type) os << ",\"returns\":" << TypeToString(v.return_type);
                os << "}";
            } else if constexpr (std::is_same_v<T, ExecDecl>) {
                os << "{\"kind\":\"exec\",\"name\":\"" << EscapeJson(v.name)
                   << "\",\"command\":\"" << EscapeJson(v.command) << "\"}";
            } else if constexpr (std::is_same_v<T, ConstDecl>) {
                os << "{\"kind\":\"const\",\"name\":\"" << EscapeJson(v.name)
                   << "\",\"type\":" << TypeToString(v.type) << ",\"value\":";
                std::visit([&](auto& cv) {
                    using CV = std::decay_t<decltype(cv)>;
                    if constexpr (std::is_same_v<CV, std::string>)
                        os << "\"" << EscapeJson(cv) << "\"";
                    else if constexpr (std::is_same_v<CV, bool>)
                        os << (cv ? "true" : "false");
                    else
                        os << cv;
                }, v.value);
                os << "}";
            } else if constexpr (std::is_same_v<T, MetaDecl>) {
                os << "{\"kind\":\"meta\",\"fields\":{";
                for (size_t i = 0; i < v.fields.size(); ++i) {
                    if (i) os << ",";
                    os << "\"" << EscapeJson(v.fields[i].key) << "\":";
                    std::visit([&](auto& mv) {
                        using MV = std::decay_t<decltype(mv)>;
                        if constexpr (std::is_same_v<MV, std::string>)
                            os << "\"" << EscapeJson(mv) << "\"";
                        else if constexpr (std::is_same_v<MV, bool>)
                            os << (mv ? "true" : "false");
                        else
                            os << mv;
                    }, v.fields[i].value);
                }
                os << "}}";
            }
        }, item);
    }
    os << "]}";
    return os.str();
}

} // namespace dssl::ast
