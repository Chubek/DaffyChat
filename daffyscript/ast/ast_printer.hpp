#pragma once
#include "ast.h"
#include <string>
#include <sstream>

namespace daffyscript::ast {

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

inline std::string FileTypeStr(FileType ft) {
    switch (ft) {
        case FileType::Module: return "module";
        case FileType::Program: return "program";
        case FileType::Recipe: return "recipe";
    }
    return "unknown";
}

inline std::string PrintFile(const File& file) {
    std::ostringstream os;
    os << "{";
    os << "\"filename\":\"" << EscapeJson(file.filename) << "\"";
    os << ",\"type\":\"" << FileTypeStr(file.file_type) << "\"";
    os << ",\"name\":\"" << EscapeJson(file.name) << "\"";
    os << ",\"version\":\"" << EscapeJson(file.version) << "\"";
    if (!file.author.empty())
        os << ",\"author\":\"" << EscapeJson(file.author) << "\"";
    if (!file.description.empty())
        os << ",\"description\":\"" << EscapeJson(file.description) << "\"";
    os << ",\"item_count\":" << file.items.size();
    os << "}";
    return os.str();
}

} // namespace daffyscript::ast
