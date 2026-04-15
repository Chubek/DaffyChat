#pragma once
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <cstdint>
#include <variant>

namespace dssl::ast {

enum class BuiltinKind {
    String, Int32, Int64, Uint32, Uint64,
    Float32, Float64, Bool, Bytes,
    Timestamp, Duration, Void
};

struct TypeRef;
using TypeRefPtr = std::shared_ptr<TypeRef>;

struct TypeRef {
    enum Kind { Builtin, Named, Optional, Repeated, Map, Stream };
    Kind kind;
    BuiltinKind builtin = BuiltinKind::Void;
    std::string name;
    TypeRefPtr element_type;
    TypeRefPtr key_type;
    TypeRefPtr value_type;
};

inline TypeRefPtr MakeBuiltin(BuiltinKind bk) {
    auto t = std::make_shared<TypeRef>();
    t->kind = TypeRef::Builtin;
    t->builtin = bk;
    return t;
}

inline TypeRefPtr MakeNamed(const std::string& n) {
    auto t = std::make_shared<TypeRef>();
    t->kind = TypeRef::Named;
    t->name = n;
    return t;
}

inline TypeRefPtr MakeOptional(TypeRefPtr inner) {
    auto t = std::make_shared<TypeRef>();
    t->kind = TypeRef::Optional;
    t->element_type = std::move(inner);
    return t;
}

inline TypeRefPtr MakeRepeated(TypeRefPtr inner) {
    auto t = std::make_shared<TypeRef>();
    t->kind = TypeRef::Repeated;
    t->element_type = std::move(inner);
    return t;
}

inline TypeRefPtr MakeMap(TypeRefPtr key, TypeRefPtr val) {
    auto t = std::make_shared<TypeRef>();
    t->kind = TypeRef::Map;
    t->key_type = std::move(key);
    t->value_type = std::move(val);
    return t;
}

inline TypeRefPtr MakeStream(TypeRefPtr inner) {
    auto t = std::make_shared<TypeRef>();
    t->kind = TypeRef::Stream;
    t->element_type = std::move(inner);
    return t;
}

struct Field {
    std::string name;
    TypeRefPtr type;
    std::optional<int> number;
    std::string doc;
    bool deprecated = false;
    std::string default_value;
};

struct StructDecl {
    std::string name;
    std::string doc;
    std::vector<Field> fields;
};

struct EnumVariant {
    std::string name;
    std::optional<int64_t> value;
    std::string doc;
};

struct EnumDecl {
    std::string name;
    std::string doc;
    std::vector<EnumVariant> variants;
};

struct UnionVariant {
    std::string name;
    TypeRefPtr type;
    std::string doc;
};

struct UnionDecl {
    std::string name;
    std::string doc;
    std::vector<UnionVariant> variants;
};

struct Param {
    std::string name;
    TypeRefPtr type;
};

struct RpcDecl {
    std::string name;
    std::string doc;
    bool deprecated = false;
    std::vector<Param> params;
    TypeRefPtr return_type;
};

enum class HttpMethod { Get, Post, Put, Patch, Delete };

struct RestDecl {
    std::string name_path;
    HttpMethod method;
    std::string doc;
    std::vector<Param> params;
    TypeRefPtr return_type;
};

struct ExecDecl {
    std::string name;
    std::string command;
    std::string doc;
};

using ConstValue = std::variant<std::string, int64_t, double, bool>;

struct ConstDecl {
    std::string name;
    TypeRefPtr type;
    ConstValue value;
    std::string doc;
};

struct MetaField {
    std::string key;
    std::variant<std::string, int64_t, bool> value;
};

struct MetaDecl {
    std::vector<MetaField> fields;
};

struct ImportDecl {
    std::string path;
};

struct ServiceDecl {
    std::string name;
    std::string version;
    std::string doc;
};

using TopLevelItem = std::variant<
    StructDecl, EnumDecl, UnionDecl, RpcDecl,
    RestDecl, ExecDecl, ConstDecl, MetaDecl>;

struct File {
    std::string filename;
    std::optional<ServiceDecl> service;
    std::vector<ImportDecl> imports;
    std::vector<TopLevelItem> items;
};

} // namespace dssl::ast
