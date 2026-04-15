#pragma once

#include "ast.h"
#include "../parser/grammar.hpp"
#include "../diag/diagnostic.hpp"

#include <tao/pegtl.hpp>
#include <stack>
#include <cassert>

namespace dssl::ast {

namespace pegtl = tao::pegtl;

struct BuildState {
    File file;
    DiagEngine& diag;

    std::vector<std::string> string_stack;
    std::vector<TypeRefPtr> type_stack;
    std::vector<int64_t> int_stack;
    std::vector<double> float_stack;
    std::vector<bool> bool_stack;

    std::string pending_doc;
    bool pending_deprecated = false;
    std::string pending_decl_doc;

    std::vector<Field> pending_fields;
    std::vector<EnumVariant> pending_enum_variants;
    std::vector<UnionVariant> pending_union_variants;
    std::vector<Param> pending_params;
    std::vector<MetaField> pending_meta_fields;

    explicit BuildState(DiagEngine& d) : diag(d) {}

    std::string pop_string() {
        assert(!string_stack.empty());
        auto s = std::move(string_stack.back());
        string_stack.pop_back();
        return s;
    }

    TypeRefPtr pop_type() {
        assert(!type_stack.empty());
        auto t = std::move(type_stack.back());
        type_stack.pop_back();
        return t;
    }

    int64_t pop_int() {
        assert(!int_stack.empty());
        auto v = int_stack.back();
        int_stack.pop_back();
        return v;
    }

    double pop_float() {
        assert(!float_stack.empty());
        auto v = float_stack.back();
        float_stack.pop_back();
        return v;
    }

    bool pop_bool() {
        assert(!bool_stack.empty());
        auto v = bool_stack.back();
        bool_stack.pop_back();
        return v;
    }

    std::string consume_doc() {
        auto d = std::move(pending_doc);
        pending_doc.clear();
        return d;
    }

    bool consume_deprecated() {
        bool d = pending_deprecated;
        pending_deprecated = false;
        return d;
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
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
            s = s.substr(1, s.size() - 2);
        }
        state.string_stack.push_back(std::move(s));
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
struct action<grammar::float_lit> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, BuildState& state) {
        state.float_stack.push_back(std::stod(in.string()));
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
struct action<grammar::kw_true> {
    template<typename ActionInput>
    static void apply(const ActionInput&, BuildState& state) {
        state.bool_stack.push_back(true);
    }
};

template<>
struct action<grammar::kw_false> {
    template<typename ActionInput>
    static void apply(const ActionInput&, BuildState& state) {
        state.bool_stack.push_back(false);
    }
};

template<>
struct action<grammar::docstring> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, BuildState& state) {
        auto text = in.string();
        auto pos = text.find("///");
        if (pos != std::string::npos) {
            text = text.substr(pos + 3);
        }
        while (!text.empty() && text.front() == ' ') text.erase(text.begin());
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
        if (!state.pending_doc.empty()) state.pending_doc += "\n";
        state.pending_doc += text;
    }
};

template<>
struct action<grammar::kw_deprecated> {
    template<typename ActionInput>
    static void apply(const ActionInput&, BuildState& state) {
        state.pending_deprecated = true;
    }
};

template<>
struct action<grammar::builtin_type> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, BuildState& state) {
        auto s = in.string();
        BuiltinKind bk = BuiltinKind::Void;
        if (s == "string")    bk = BuiltinKind::String;
        else if (s == "int32")     bk = BuiltinKind::Int32;
        else if (s == "int64")     bk = BuiltinKind::Int64;
        else if (s == "uint32")    bk = BuiltinKind::Uint32;
        else if (s == "uint64")    bk = BuiltinKind::Uint64;
        else if (s == "float32")   bk = BuiltinKind::Float32;
        else if (s == "float64")   bk = BuiltinKind::Float64;
        else if (s == "bool")      bk = BuiltinKind::Bool;
        else if (s == "bytes")     bk = BuiltinKind::Bytes;
        else if (s == "timestamp") bk = BuiltinKind::Timestamp;
        else if (s == "duration")  bk = BuiltinKind::Duration;
        else if (s == "void")      bk = BuiltinKind::Void;
        state.type_stack.push_back(MakeBuiltin(bk));
    }
};

template<>
struct action<grammar::named_type> {
    template<typename ActionInput>
    static void apply(const ActionInput&, BuildState& state) {
        auto name = state.pop_string();
        state.type_stack.push_back(MakeNamed(name));
    }
};

template<>
struct action<grammar::optional_type> {
    template<typename ActionInput>
    static void apply(const ActionInput&, BuildState& state) {
        auto inner = state.pop_type();
        state.type_stack.push_back(MakeOptional(std::move(inner)));
    }
};

template<>
struct action<grammar::repeated_type> {
    template<typename ActionInput>
    static void apply(const ActionInput&, BuildState& state) {
        auto inner = state.pop_type();
        state.type_stack.push_back(MakeRepeated(std::move(inner)));
    }
};

template<>
struct action<grammar::map_type> {
    template<typename ActionInput>
    static void apply(const ActionInput&, BuildState& state) {
        auto val = state.pop_type();
        auto key = state.pop_type();
        state.type_stack.push_back(MakeMap(std::move(key), std::move(val)));
    }
};

template<>
struct action<grammar::stream_type> {
    template<typename ActionInput>
    static void apply(const ActionInput&, BuildState& state) {
        auto inner = state.pop_type();
        state.type_stack.push_back(MakeStream(std::move(inner)));
    }
};

template<>
struct action<grammar::field_decl> {
    template<typename ActionInput>
    static void apply(const ActionInput&, BuildState& state) {
        Field f;
        f.type = state.pop_type();
        f.name = state.pop_string();
        f.doc = state.consume_doc();
        f.deprecated = state.consume_deprecated();
        state.pending_fields.push_back(std::move(f));
    }
};

template<>
struct action<grammar::struct_decl> {
    template<typename ActionInput>
    static void apply(const ActionInput&, BuildState& state) {
        StructDecl sd;
        sd.name = state.pop_string();
        sd.doc = state.consume_doc();
        sd.fields = std::move(state.pending_fields);
        state.pending_fields.clear();
        state.file.items.push_back(std::move(sd));
    }
};

template<>
struct action<grammar::enum_variant> {
    template<typename ActionInput>
    static void apply(const ActionInput&, BuildState& state) {
        EnumVariant ev;
        ev.name = state.pop_string();
        ev.doc = state.consume_doc();
        if (!state.int_stack.empty()) {
            ev.value = state.pop_int();
        }
        state.pending_enum_variants.push_back(std::move(ev));
    }
};

template<>
struct action<grammar::enum_decl> {
    template<typename ActionInput>
    static void apply(const ActionInput&, BuildState& state) {
        EnumDecl ed;
        ed.name = state.pop_string();
        ed.doc = state.consume_doc();
        ed.variants = std::move(state.pending_enum_variants);
        state.pending_enum_variants.clear();
        state.file.items.push_back(std::move(ed));
    }
};

template<>
struct action<grammar::union_variant> {
    template<typename ActionInput>
    static void apply(const ActionInput&, BuildState& state) {
        UnionVariant uv;
        uv.type = state.pop_type();
        uv.name = state.pop_string();
        uv.doc = state.consume_doc();
        state.pending_union_variants.push_back(std::move(uv));
    }
};

template<>
struct action<grammar::union_decl> {
    template<typename ActionInput>
    static void apply(const ActionInput&, BuildState& state) {
        UnionDecl ud;
        ud.name = state.pop_string();
        ud.doc = state.consume_doc();
        ud.variants = std::move(state.pending_union_variants);
        state.pending_union_variants.clear();
        state.file.items.push_back(std::move(ud));
    }
};

template<>
struct action<grammar::rpc_param> {
    template<typename ActionInput>
    static void apply(const ActionInput&, BuildState& state) {
        Param p;
        p.type = state.pop_type();
        p.name = state.pop_string();
        state.pending_params.push_back(std::move(p));
    }
};

template<>
struct action<grammar::rpc_decl> {
    template<typename ActionInput>
    static void apply(const ActionInput&, BuildState& state) {
        RpcDecl rd;
        rd.name = state.pop_string();
        rd.doc = state.consume_doc();
        rd.deprecated = state.consume_deprecated();
        rd.params = std::move(state.pending_params);
        state.pending_params.clear();
        if (!state.type_stack.empty()) {
            rd.return_type = state.pop_type();
        }
        state.file.items.push_back(std::move(rd));
    }
};

template<>
struct action<grammar::http_method> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, BuildState& state) {
        auto s = in.string();
        if (s == "GET")         state.string_stack.push_back("GET");
        else if (s == "POST")   state.string_stack.push_back("POST");
        else if (s == "PUT")    state.string_stack.push_back("PUT");
        else if (s == "PATCH")  state.string_stack.push_back("PATCH");
        else if (s == "DELETE") state.string_stack.push_back("DELETE");
    }
};

template<>
struct action<grammar::rest_decl> {
    template<typename ActionInput>
    static void apply(const ActionInput&, BuildState& state) {
        RestDecl rd;
        rd.doc = state.consume_doc();
        rd.params = std::move(state.pending_params);
        state.pending_params.clear();
        if (!state.type_stack.empty()) {
            rd.return_type = state.pop_type();
        }
        rd.name_path = state.pop_string();
        auto method_str = state.pop_string();
        if (method_str == "GET")         rd.method = HttpMethod::Get;
        else if (method_str == "POST")   rd.method = HttpMethod::Post;
        else if (method_str == "PUT")    rd.method = HttpMethod::Put;
        else if (method_str == "PATCH")  rd.method = HttpMethod::Patch;
        else if (method_str == "DELETE") rd.method = HttpMethod::Delete;
        state.file.items.push_back(std::move(rd));
    }
};

template<>
struct action<grammar::exec_decl> {
    template<typename ActionInput>
    static void apply(const ActionInput&, BuildState& state) {
        ExecDecl ed;
        ed.command = state.pop_string();
        ed.name = state.pop_string();
        ed.doc = state.consume_doc();
        state.file.items.push_back(std::move(ed));
    }
};

template<>
struct action<grammar::const_decl> {
    template<typename ActionInput>
    static void apply(const ActionInput&, BuildState& state) {
        ConstDecl cd;
        cd.doc = state.consume_doc();
        cd.type = state.pop_type();
        if (!state.bool_stack.empty()) {
            cd.value = state.pop_bool();
        } else if (!state.float_stack.empty()) {
            cd.value = state.pop_float();
        } else if (!state.int_stack.empty()) {
            cd.value = state.pop_int();
        } else if (!state.string_stack.empty()) {
            // string value was pushed after name; pop value first
            cd.value = state.pop_string();
        }
        cd.name = state.pop_string();
        state.file.items.push_back(std::move(cd));
    }
};

template<>
struct action<grammar::meta_field> {
    template<typename ActionInput>
    static void apply(const ActionInput&, BuildState& state) {
        MetaField mf;
        if (!state.bool_stack.empty()) {
            mf.value = state.pop_bool();
            mf.key = state.pop_string();
        } else if (!state.int_stack.empty()) {
            mf.value = state.pop_int();
            mf.key = state.pop_string();
        } else {
            // string value: value was pushed after key
            mf.value = state.pop_string();
            mf.key = state.pop_string();
        }
        state.pending_meta_fields.push_back(std::move(mf));
    }
};

template<>
struct action<grammar::meta_decl> {
    template<typename ActionInput>
    static void apply(const ActionInput&, BuildState& state) {
        MetaDecl md;
        md.fields = std::move(state.pending_meta_fields);
        state.pending_meta_fields.clear();
        state.file.items.push_back(std::move(md));
    }
};

template<>
struct action<grammar::dotted_ident> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, BuildState& state) {
        auto count = std::count(in.begin(), in.end(), '.');
        std::string joined;
        std::vector<std::string> parts;
        for (int64_t i = 0; i <= count; ++i) {
            parts.push_back(state.pop_string());
        }
        for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
            if (!joined.empty()) joined += ".";
            joined += *it;
        }
        state.string_stack.push_back(std::move(joined));
    }
};

template<>
struct action<grammar::import_decl> {
    template<typename ActionInput>
    static void apply(const ActionInput&, BuildState& state) {
        ImportDecl id;
        id.path = state.pop_string();
        state.file.imports.push_back(std::move(id));
    }
};

template<>
struct action<grammar::service_decl> {
    template<typename ActionInput>
    static void apply(const ActionInput&, BuildState& state) {
        ServiceDecl sd;
        sd.version = state.pop_string();
        sd.name = state.pop_string();
        sd.doc = state.consume_doc();
        state.file.service = std::move(sd);
    }
};

inline bool ParseFile(const std::string& filename, const std::string& source, File& out, DiagEngine& diag) {
    BuildState state(diag);
    state.file.filename = filename;
    pegtl::memory_input input(source, filename);
    try {
        bool ok = pegtl::parse<grammar::dssl_file, action>(input, state);
        if (ok) {
            out = std::move(state.file);
            return true;
        }
        diag.emit(Diagnostic{DiagLevel::Error, "E001", "Parse failed", {filename, 0, 0}, {}});
        return false;
    } catch (const pegtl::parse_error& e) {
        diag.emit(Diagnostic{DiagLevel::Error, "E001", e.what(), {filename, 0, 0}, {}});
        return false;
    }
}

} // namespace dssl::ast
