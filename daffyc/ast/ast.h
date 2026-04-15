#pragma once
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <variant>
#include <cstdint>

namespace daffyscript::ast {

enum class FileType { Module, Program, Recipe };

enum class BuiltinType { Str, Int, Float, Bool, Bytes };

struct TypeRef;
using TypeRefPtr = std::shared_ptr<TypeRef>;

struct TypeRef {
    enum Kind { Builtin, Named, List, Map, Optional };
    Kind kind;
    BuiltinType builtin = BuiltinType::Str;
    std::string name;
    TypeRefPtr element_type;
    TypeRefPtr key_type;
    TypeRefPtr value_type;
};

inline TypeRefPtr MakeBuiltin(BuiltinType bt) {
    auto t = std::make_shared<TypeRef>();
    t->kind = TypeRef::Builtin;
    t->builtin = bt;
    return t;
}

inline TypeRefPtr MakeNamed(const std::string& n) {
    auto t = std::make_shared<TypeRef>();
    t->kind = TypeRef::Named;
    t->name = n;
    return t;
}

inline TypeRefPtr MakeList(TypeRefPtr inner) {
    auto t = std::make_shared<TypeRef>();
    t->kind = TypeRef::List;
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

inline TypeRefPtr MakeOptional(TypeRefPtr inner) {
    auto t = std::make_shared<TypeRef>();
    t->kind = TypeRef::Optional;
    t->element_type = std::move(inner);
    return t;
}

struct Expr;
using ExprPtr = std::shared_ptr<Expr>;
struct Stmt;
using StmtPtr = std::shared_ptr<Stmt>;

struct Param {
    std::string name;
    TypeRefPtr type;
};

enum class BinOp {
    Add, Sub, Mul, Div, Mod,
    Eq, Neq, Lt, Gt, Lte, Gte,
    And, Or, Concat, NullCoal
};

enum class UnaryOp { Neg, Not };

struct IntLit { int64_t value; };
struct FloatLit { double value; };
struct StringLit { std::string value; };
struct BoolLit { bool value; };
struct NoneLit {};
struct IdentExpr { std::string name; };
struct DottedExpr { std::vector<std::string> parts; };
struct BinaryExpr { BinOp op; ExprPtr left, right; };
struct UnaryExpr { UnaryOp op; ExprPtr operand; };
struct CallExpr { ExprPtr callee; std::vector<ExprPtr> args; };
struct MethodCallExpr { ExprPtr object; std::string method; std::vector<ExprPtr> args; };
struct IndexExpr { ExprPtr object; ExprPtr index; };
struct FieldExpr { ExprPtr object; std::string field; };
struct ListExpr { std::vector<ExprPtr> elements; };
struct MapExpr { std::vector<std::pair<ExprPtr, ExprPtr>> entries; };
struct StructInitExpr { std::string name; std::vector<std::pair<std::string, ExprPtr>> fields; };

struct Expr {
    std::variant<
        IntLit, FloatLit, StringLit, BoolLit, NoneLit,
        IdentExpr, DottedExpr, BinaryExpr, UnaryExpr,
        CallExpr, MethodCallExpr, IndexExpr, FieldExpr,
        ListExpr, MapExpr, StructInitExpr
    > data;
};

struct LetStmt { std::string name; bool is_mutable; std::optional<TypeRefPtr> type_ann; ExprPtr value; };
struct AssignStmt { ExprPtr target; ExprPtr value; };
struct ReturnStmt { std::optional<ExprPtr> value; };
struct RaiseStmt { ExprPtr value; };
struct ExprStmt { ExprPtr expr; };
struct IfStmt { ExprPtr condition; std::vector<StmtPtr> then_block; std::vector<StmtPtr> else_block; };
struct ForStmt { std::optional<std::string> index_var; std::string value_var; ExprPtr iterable; std::vector<StmtPtr> body; };
struct WhileStmt { ExprPtr condition; std::vector<StmtPtr> body; };
struct MatchArm { ExprPtr pattern; std::vector<StmtPtr> body; };
struct MatchStmt { ExprPtr subject; std::vector<MatchArm> arms; };
struct TryStmt { std::vector<StmtPtr> try_block; std::string catch_var; std::vector<StmtPtr> catch_block; };
struct EmitStmt { std::string event_name; std::vector<std::pair<std::string, ExprPtr>> fields; };

struct Stmt {
    std::variant<
        LetStmt, AssignStmt, ReturnStmt, RaiseStmt, ExprStmt,
        IfStmt, ForStmt, WhileStmt, MatchStmt, TryStmt, EmitStmt
    > data;
};

struct FnDecl {
    std::string name;
    bool is_pub = false;
    std::vector<Param> params;
    std::optional<TypeRefPtr> return_type;
    std::vector<StmtPtr> body;
};

struct StructDecl {
    std::string name;
    std::vector<Param> fields;
};

struct EnumDecl {
    std::string name;
    std::vector<std::string> variants;
};

struct ImportDecl {
    std::string path;
    std::vector<std::string> specific;
};

struct CommandDecl {
    std::string pattern;
    std::vector<Param> params;
    std::vector<StmtPtr> body;
};

struct InterceptDecl {
    std::string pattern;
    std::vector<Param> params;
    std::vector<StmtPtr> body;
};

struct EveryDecl {
    int64_t interval;
    std::string unit;
    std::vector<StmtPtr> body;
};

struct AtDecl {
    std::string cron;
    std::string timezone;
    std::vector<StmtPtr> body;
};

struct OnEventDecl {
    std::string event_name;
    std::vector<Param> params;
    std::vector<StmtPtr> body;
};

struct OnHookDecl {
    std::string hook_name;
    std::vector<Param> params;
    std::vector<StmtPtr> body;
};

struct ExpectHookDecl {
    std::string hook_name;
};

struct ExportsDecl {
    std::vector<std::string> items;
};

struct RoomConfig {
    std::vector<std::pair<std::string, std::string>> entries;
};

struct ServiceConfig {
    std::string name;
    std::vector<std::pair<std::string, std::string>> entries;
    std::vector<std::pair<std::string, std::string>> config_entries;
};

struct RoleDecl {
    std::string name;
    std::vector<std::string> can;
    std::vector<std::string> cannot;
};

struct RolesDecl {
    std::vector<RoleDecl> roles;
    std::string default_role;
};

struct WebhookDecl {
    std::string event_name;
    std::string url;
};

struct WebhooksDecl {
    std::vector<WebhookDecl> hooks;
};

struct WhenBlock {
    std::string condition;
};

struct OnInitDecl {
    std::vector<StmtPtr> body;
};

using ModuleItem = std::variant<ImportDecl, FnDecl, StructDecl, EnumDecl, OnHookDecl, ExpectHookDecl, EmitStmt, ExportsDecl>;
using ProgramItem = std::variant<ImportDecl, FnDecl, StructDecl, EnumDecl, CommandDecl, InterceptDecl, EveryDecl, AtDecl, OnEventDecl>;
using RecipeItem = std::variant<RoomConfig, ServiceConfig, RolesDecl, WebhooksDecl, WhenBlock, OnInitDecl>;

struct File {
    std::string filename;
    FileType file_type;
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    std::vector<std::variant<ModuleItem, ProgramItem, RecipeItem>> items;
};

} // namespace daffyscript::ast
