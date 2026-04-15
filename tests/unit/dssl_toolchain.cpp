#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

#include "../../dssl/ast/ast.h"
#include "../../dssl/ast/ast_builder.hpp"
#include "../../dssl/ast/ast_printer.hpp"
#include "../../dssl/sema/sema.hpp"
#include "../../dssl/diag/diagnostic.hpp"

static void test_parse_service() {
    std::string source = R"(
/// Echo service
service echo 1.0.0;
)";
    dssl::DiagEngine diag;
    dssl::ast::File file;
    bool ok = dssl::ast::ParseFile("test.dssl", source, file, diag);
    assert(ok);
    assert(file.service.has_value());
    assert(file.service->name == "echo");
    assert(file.service->version == "1.0.0");
    assert(file.service->doc == "Echo service");
    std::cout << "  PASS: parse_service\n";
}

static void test_parse_struct() {
    std::string source = R"(
struct Message {
    id: string;
    content: string;
    count: int32;
    tags: repeated<string>;
}
)";
    dssl::DiagEngine diag;
    dssl::ast::File file;
    bool ok = dssl::ast::ParseFile("test.dssl", source, file, diag);
    assert(ok);
    assert(file.items.size() == 1);
    auto* sd = std::get_if<dssl::ast::StructDecl>(&file.items[0]);
    assert(sd != nullptr);
    assert(sd->name == "Message");
    assert(sd->fields.size() == 4);
    assert(sd->fields[0].name == "id");
    assert(sd->fields[0].type->kind == dssl::ast::TypeRef::Builtin);
    assert(sd->fields[0].type->builtin == dssl::ast::BuiltinKind::String);
    assert(sd->fields[3].name == "tags");
    assert(sd->fields[3].type->kind == dssl::ast::TypeRef::Repeated);
    std::cout << "  PASS: parse_struct\n";
}

static void test_parse_enum() {
    std::string source = R"(
enum Status {
    Pending = 0;
    Active = 1;
    Closed = 2;
}
)";
    dssl::DiagEngine diag;
    dssl::ast::File file;
    bool ok = dssl::ast::ParseFile("test.dssl", source, file, diag);
    assert(ok);
    assert(file.items.size() == 1);
    auto* ed = std::get_if<dssl::ast::EnumDecl>(&file.items[0]);
    assert(ed != nullptr);
    assert(ed->name == "Status");
    assert(ed->variants.size() == 3);
    assert(ed->variants[0].name == "Pending");
    assert(ed->variants[0].value.has_value());
    assert(*ed->variants[0].value == 0);
    std::cout << "  PASS: parse_enum\n";
}

static void test_parse_rpc() {
    std::string source = R"(
struct Message {
    id: string;
}
rpc SendMessage(content: string, room: string) returns Message;
)";
    dssl::DiagEngine diag;
    dssl::ast::File file;
    bool ok = dssl::ast::ParseFile("test.dssl", source, file, diag);
    assert(ok);
    assert(file.items.size() == 2);
    auto* rd = std::get_if<dssl::ast::RpcDecl>(&file.items[1]);
    assert(rd != nullptr);
    assert(rd->name == "SendMessage");
    assert(rd->params.size() == 2);
    assert(rd->return_type != nullptr);
    assert(rd->return_type->kind == dssl::ast::TypeRef::Named);
    assert(rd->return_type->name == "Message");
    std::cout << "  PASS: parse_rpc\n";
}

static void test_parse_rest() {
    std::string source = R"(
rest GET "/api/rooms" () returns repeated<string>;
)";
    dssl::DiagEngine diag;
    dssl::ast::File file;
    bool ok = dssl::ast::ParseFile("test.dssl", source, file, diag);
    assert(ok);
    assert(file.items.size() == 1);
    auto* rd = std::get_if<dssl::ast::RestDecl>(&file.items[0]);
    assert(rd != nullptr);
    assert(rd->method == dssl::ast::HttpMethod::Get);
    assert(rd->name_path == "/api/rooms");
    std::cout << "  PASS: parse_rest\n";
}

static void test_parse_exec() {
    std::string source = R"(
exec healthcheck "curl -sf http://localhost/health";
)";
    dssl::DiagEngine diag;
    dssl::ast::File file;
    bool ok = dssl::ast::ParseFile("test.dssl", source, file, diag);
    assert(ok);
    assert(file.items.size() == 1);
    auto* ed = std::get_if<dssl::ast::ExecDecl>(&file.items[0]);
    assert(ed != nullptr);
    assert(ed->name == "healthcheck");
    assert(ed->command == "curl -sf http://localhost/health");
    std::cout << "  PASS: parse_exec\n";
}

static void test_parse_const() {
    std::string source = R"(
const MAX_LEN: int32 = 4096;
)";
    dssl::DiagEngine diag;
    dssl::ast::File file;
    bool ok = dssl::ast::ParseFile("test.dssl", source, file, diag);
    assert(ok);
    assert(file.items.size() == 1);
    auto* cd = std::get_if<dssl::ast::ConstDecl>(&file.items[0]);
    assert(cd != nullptr);
    assert(cd->name == "MAX_LEN");
    assert(std::holds_alternative<int64_t>(cd->value));
    assert(std::get<int64_t>(cd->value) == 4096);
    std::cout << "  PASS: parse_const\n";
}

static void test_parse_meta() {
    std::string source = R"(
meta {
    author: "daffychat";
    license: "MIT";
}
)";
    dssl::DiagEngine diag;
    dssl::ast::File file;
    bool ok = dssl::ast::ParseFile("test.dssl", source, file, diag);
    assert(ok);
    assert(file.items.size() == 1);
    auto* md = std::get_if<dssl::ast::MetaDecl>(&file.items[0]);
    assert(md != nullptr);
    assert(md->fields.size() == 2);
    assert(md->fields[0].key == "author");
    std::cout << "  PASS: parse_meta\n";
}

static void test_parse_import() {
    std::string source = R"(
import std.types;
struct Foo {
    x: int32;
}
)";
    dssl::DiagEngine diag;
    dssl::ast::File file;
    bool ok = dssl::ast::ParseFile("test.dssl", source, file, diag);
    assert(ok);
    assert(file.imports.size() == 1);
    assert(file.imports[0].path == "std.types");
    std::cout << "  PASS: parse_import\n";
}

static void test_sema_duplicate() {
    std::string source = R"(
struct Foo { x: int32; }
struct Foo { y: string; }
)";
    dssl::DiagEngine diag;
    dssl::ast::File file;
    bool ok = dssl::ast::ParseFile("test.dssl", source, file, diag);
    assert(ok);

    dssl::sema::SemanticAnalyzer sema(diag);
    bool sema_ok = sema.analyze(file);
    assert(!sema_ok);
    assert(diag.has_errors());
    std::cout << "  PASS: sema_duplicate\n";
}

static void test_sema_unknown_type() {
    std::string source = R"(
rpc Send(msg: UnknownType);
)";
    dssl::DiagEngine diag;
    dssl::ast::File file;
    bool ok = dssl::ast::ParseFile("test.dssl", source, file, diag);
    assert(ok);

    dssl::sema::SemanticAnalyzer sema(diag);
    bool sema_ok = sema.analyze(file);
    assert(!sema_ok);
    std::cout << "  PASS: sema_unknown_type\n";
}

static void test_ast_printer() {
    std::string source = R"(
/// Test service
service test 0.1.0;
struct Msg { id: string; }
)";
    dssl::DiagEngine diag;
    dssl::ast::File file;
    bool ok = dssl::ast::ParseFile("test.dssl", source, file, diag);
    assert(ok);

    auto json = dssl::ast::PrintFile(file);
    assert(json.find("\"test\"") != std::string::npos);
    assert(json.find("\"struct\"") != std::string::npos);
    assert(json.find("\"Msg\"") != std::string::npos);
    std::cout << "  PASS: ast_printer\n";
}

static void test_full_pipeline() {
    std::string source = R"(
/// Full test service
service full_test 2.0.0;

import std.types;

/// A user record
struct User {
    /// Unique ID
    id: string;
    name: string;
    email: optional<string>;
    age: int32;
}

enum Role {
    Admin = 0;
    Member = 1;
    Guest = 2;
}

/// Create a user
rpc CreateUser(name: string, email: string) returns User;

/// List all users
rest GET "/api/users" () returns repeated<User>;

const MAX_USERS: int32 = 100;

meta {
    author: "test";
    version_tag: "beta";
}
)";
    dssl::DiagEngine diag;
    dssl::ast::File file;
    bool ok = dssl::ast::ParseFile("full_test.dssl", source, file, diag);
    if (!ok) {
        diag.print_all(std::cerr);
    }
    assert(ok);
    assert(file.service.has_value());
    assert(file.service->name == "full_test");
    assert(file.imports.size() == 1);
    assert(file.items.size() == 6);

    dssl::sema::SemanticAnalyzer sema(diag);
    bool sema_ok = sema.analyze(file);
    if (!sema_ok) {
        diag.print_all(std::cerr);
    }
    assert(sema_ok);

    auto json = dssl::ast::PrintFile(file);
    assert(!json.empty());
    std::cout << "  PASS: full_pipeline\n";
}

int main() {
    std::cout << "DSSL toolchain tests:\n";
    test_parse_service();
    test_parse_struct();
    test_parse_enum();
    test_parse_rpc();
    test_parse_rest();
    test_parse_exec();
    test_parse_const();
    test_parse_meta();
    test_parse_import();
    test_sema_duplicate();
    test_sema_unknown_type();
    test_ast_printer();
    test_full_pipeline();
    std::cout << "All DSSL toolchain tests passed.\n";
    return 0;
}
