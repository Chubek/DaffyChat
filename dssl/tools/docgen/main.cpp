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

static std::string html_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '&': out += "&amp;"; break;
            case '"': out += "&quot;"; break;
            default: out += c;
        }
    }
    return out;
}

static std::string type_html(const dssl::ast::TypeRefPtr& t) {
    if (!t) return "<span class=\"type\">void</span>";
    switch (t->kind) {
        case dssl::ast::TypeRef::Builtin: {
            std::string name;
            switch (t->builtin) {
                case dssl::ast::BuiltinKind::String: name = "string"; break;
                case dssl::ast::BuiltinKind::Int32: name = "int32"; break;
                case dssl::ast::BuiltinKind::Int64: name = "int64"; break;
                case dssl::ast::BuiltinKind::Uint32: name = "uint32"; break;
                case dssl::ast::BuiltinKind::Uint64: name = "uint64"; break;
                case dssl::ast::BuiltinKind::Float32: name = "float32"; break;
                case dssl::ast::BuiltinKind::Float64: name = "float64"; break;
                case dssl::ast::BuiltinKind::Bool: name = "bool"; break;
                case dssl::ast::BuiltinKind::Bytes: name = "bytes"; break;
                case dssl::ast::BuiltinKind::Timestamp: name = "timestamp"; break;
                case dssl::ast::BuiltinKind::Duration: name = "duration"; break;
                case dssl::ast::BuiltinKind::Void: name = "void"; break;
            }
            return "<span class=\"type builtin\">" + name + "</span>";
        }
        case dssl::ast::TypeRef::Named:
            return "<a class=\"type named\" href=\"#" + t->name + "\">" + html_escape(t->name) + "</a>";
        case dssl::ast::TypeRef::Optional:
            return "optional&lt;" + type_html(t->element_type) + "&gt;";
        case dssl::ast::TypeRef::Repeated:
            return "repeated&lt;" + type_html(t->element_type) + "&gt;";
        case dssl::ast::TypeRef::Map:
            return "map&lt;" + type_html(t->key_type) + ", " + type_html(t->value_type) + "&gt;";
        case dssl::ast::TypeRef::Stream:
            return "stream&lt;" + type_html(t->element_type) + "&gt;";
    }
    return "void";
}

int main(int argc, char** argv) {
    std::string source_file;
    std::string out_path;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        } else if (argv[i][0] != '-') {
            source_file = argv[i];
        }
    }

    if (source_file.empty()) {
        std::cerr << "Usage: dssl-docgen [--out <file.html>] <source.dssl>\n";
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

    std::ostream* out = &std::cout;
    std::ofstream ofs;
    if (!out_path.empty()) {
        ofs.open(out_path);
        if (!ofs) { std::cerr << "Cannot write to " << out_path << "\n"; return 1; }
        out = &ofs;
    }

    *out << "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\">\n"
         << "<title>DSSL Documentation";
    if (file.service) *out << " &mdash; " << html_escape(file.service->name);
    *out << "</title>\n<style>\n"
         << "body { font-family: system-ui, sans-serif; max-width: 48rem; margin: 2rem auto; line-height: 1.6; }\n"
         << ".type { font-family: monospace; } .builtin { color: #0550ae; } .named { color: #8250df; }\n"
         << "h2 { border-bottom: 1px solid #d0d7de; padding-bottom: .3em; }\n"
         << "table { border-collapse: collapse; width: 100%; } td, th { border: 1px solid #d0d7de; padding: .4em .8em; text-align: left; }\n"
         << ".deprecated { text-decoration: line-through; opacity: 0.6; }\n"
         << "</style></head><body>\n";

    if (file.service) {
        *out << "<h1>" << html_escape(file.service->name) << " <small>v" << html_escape(file.service->version) << "</small></h1>\n";
        if (!file.service->doc.empty())
            *out << "<p>" << html_escape(file.service->doc) << "</p>\n";
    } else {
        *out << "<h1>DSSL Documentation</h1>\n";
    }

    for (auto& item : file.items) {
        std::visit([&](auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, dssl::ast::StructDecl>) {
                *out << "<h2 id=\"" << v.name << "\">struct " << html_escape(v.name) << "</h2>\n";
                if (!v.doc.empty()) *out << "<p>" << html_escape(v.doc) << "</p>\n";
                *out << "<table><tr><th>Field</th><th>Type</th><th>Description</th></tr>\n";
                for (auto& f : v.fields) {
                    *out << "<tr" << (f.deprecated ? " class=\"deprecated\"" : "") << ">"
                         << "<td>" << html_escape(f.name) << "</td>"
                         << "<td>" << type_html(f.type) << "</td>"
                         << "<td>" << html_escape(f.doc) << "</td></tr>\n";
                }
                *out << "</table>\n";
            } else if constexpr (std::is_same_v<T, dssl::ast::EnumDecl>) {
                *out << "<h2 id=\"" << v.name << "\">enum " << html_escape(v.name) << "</h2>\n";
                if (!v.doc.empty()) *out << "<p>" << html_escape(v.doc) << "</p>\n";
                *out << "<ul>\n";
                for (auto& ev : v.variants) {
                    *out << "<li><code>" << html_escape(ev.name) << "</code>";
                    if (ev.value) *out << " = " << *ev.value;
                    if (!ev.doc.empty()) *out << " &mdash; " << html_escape(ev.doc);
                    *out << "</li>\n";
                }
                *out << "</ul>\n";
            } else if constexpr (std::is_same_v<T, dssl::ast::RpcDecl>) {
                *out << "<h2 id=\"" << v.name << "\"" << (v.deprecated ? " class=\"deprecated\"" : "")
                     << ">rpc " << html_escape(v.name) << "</h2>\n";
                if (!v.doc.empty()) *out << "<p>" << html_escape(v.doc) << "</p>\n";
                if (!v.params.empty()) {
                    *out << "<table><tr><th>Parameter</th><th>Type</th></tr>\n";
                    for (auto& p : v.params) {
                        *out << "<tr><td>" << html_escape(p.name) << "</td>"
                             << "<td>" << type_html(p.type) << "</td></tr>\n";
                    }
                    *out << "</table>\n";
                }
                if (v.return_type) {
                    *out << "<p>Returns: " << type_html(v.return_type) << "</p>\n";
                }
            } else if constexpr (std::is_same_v<T, dssl::ast::RestDecl>) {
                std::string method;
                switch (v.method) {
                    case dssl::ast::HttpMethod::Get: method = "GET"; break;
                    case dssl::ast::HttpMethod::Post: method = "POST"; break;
                    case dssl::ast::HttpMethod::Put: method = "PUT"; break;
                    case dssl::ast::HttpMethod::Patch: method = "PATCH"; break;
                    case dssl::ast::HttpMethod::Delete: method = "DELETE"; break;
                }
                *out << "<h2>rest " << method << " " << html_escape(v.name_path) << "</h2>\n";
                if (!v.doc.empty()) *out << "<p>" << html_escape(v.doc) << "</p>\n";
            } else if constexpr (std::is_same_v<T, dssl::ast::ExecDecl>) {
                *out << "<h2 id=\"" << v.name << "\">exec " << html_escape(v.name) << "</h2>\n";
                *out << "<pre>" << html_escape(v.command) << "</pre>\n";
                if (!v.doc.empty()) *out << "<p>" << html_escape(v.doc) << "</p>\n";
            }
        }, item);
    }

    *out << "</body></html>\n";
    return 0;
}
