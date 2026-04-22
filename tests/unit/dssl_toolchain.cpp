#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "../../dssl/ast/ast.h"
#include "../../dssl/ast/ast_builder.hpp"
#include "../../dssl/ast/ast_printer.hpp"
#include "../../dssl/codegen/cpp/cpp_gen.hpp"
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

static void test_cpp_codegen_service_adapter() {
    const std::string spec_path = std::string(DAFFY_SOURCE_DIR) + "/services/specs/echo.dssl";
    std::ifstream source_file(spec_path);
    std::stringstream source_buffer;
    source_buffer << source_file.rdbuf();
    const std::string source = source_buffer.str();

    dssl::DiagEngine diag;
    dssl::ast::File file;
    const bool ok = dssl::ast::ParseFile(spec_path, source, file, diag);
    assert(ok);

    dssl::sema::SemanticAnalyzer sema(diag);
    assert(sema.analyze(file));

    const auto out_dir = std::filesystem::temp_directory_path() / "daffy-dssl-codegen-test";
    std::filesystem::create_directories(out_dir);
    dssl::codegen::CppGenerator gen;
    assert(gen.generate(file, out_dir.string()));

    std::ifstream service_header(out_dir / "echo.service.hpp");
    std::stringstream header_buffer;
    header_buffer << service_header.rdbuf();
    const auto header = header_buffer.str();
    assert(header.find("EchoGeneratedService") != std::string::npos);
    assert(header.find("Handle") != std::string::npos);

    std::ifstream service_source(out_dir / "echo.service.cpp");
    std::stringstream generated_buffer;
    generated_buffer << service_source.rdbuf();
    const auto generated = generated_buffer.str();
    assert(generated.find("service.echo") != std::string::npos);
    assert(generated.find("Echo(message, sender)") != std::string::npos);
    std::cout << "  PASS: cpp_codegen_service_adapter\n";
}

static std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

class CerrCapture {
public:
    CerrCapture() : old_(std::cerr.rdbuf(stream_.rdbuf())) {}
    ~CerrCapture() { std::cerr.rdbuf(old_); }

    std::string str() const { return stream_.str(); }

private:
    std::ostringstream stream_;
    std::streambuf* old_;
};

static void test_cpp_codegen_matches_checked_in_echo_fixture() {
    const std::filesystem::path source_dir = DAFFY_SOURCE_DIR;
    const auto spec_path = source_dir / "services/specs/echo.dssl";
    const auto generated_dir = source_dir / "services/generated";

    std::ifstream source_file(spec_path);
    std::stringstream source_buffer;
    source_buffer << source_file.rdbuf();

    dssl::DiagEngine diag;
    dssl::ast::File file;
    const bool ok = dssl::ast::ParseFile(spec_path.string(), source_buffer.str(), file, diag);
    assert(ok);

    dssl::sema::SemanticAnalyzer sema(diag);
    assert(sema.analyze(file));

    const auto out_dir = std::filesystem::temp_directory_path() / ("daffy-dssl-fixture-" + std::to_string(std::rand()));
    std::filesystem::create_directories(out_dir);

    dssl::codegen::CppGenerator gen;
    assert(gen.generate(file, out_dir.string()));

    assert(read_text_file(out_dir / "echo.generated.hpp") == read_text_file(generated_dir / "echo.generated.hpp"));
    assert(read_text_file(out_dir / "echo.service.hpp") == read_text_file(generated_dir / "echo.service.hpp"));
    assert(read_text_file(out_dir / "echo.service.cpp") == read_text_file(generated_dir / "echo.service.cpp"));
    assert(read_text_file(out_dir / "echo.skeleton.cpp") == read_text_file(generated_dir / "echo.skeleton.cpp"));
    std::filesystem::remove_all(out_dir);
    std::cout << "  PASS: cpp_codegen_matches_checked_in_echo_fixture\n";
}

static void test_cpp_codegen_matches_checked_in_room_ops_fixture() {
    const std::filesystem::path source_dir = DAFFY_SOURCE_DIR;
    const auto spec_path = source_dir / "services/specs/room_ops.dssl";
    const auto generated_dir = source_dir / "services/generated";

    std::ifstream source_file(spec_path);
    std::stringstream source_buffer;
    source_buffer << source_file.rdbuf();

    dssl::DiagEngine diag;
    dssl::ast::File file;
    const bool ok = dssl::ast::ParseFile(spec_path.string(), source_buffer.str(), file, diag);
    assert(ok);

    dssl::sema::SemanticAnalyzer sema(diag);
    assert(sema.analyze(file));

    const auto out_dir = std::filesystem::temp_directory_path() / ("daffy-dssl-room-ops-" + std::to_string(std::rand()));
    std::filesystem::create_directories(out_dir);

    dssl::codegen::CppGenerator gen;
    assert(gen.generate(file, out_dir.string()));

    assert(read_text_file(out_dir / "room_ops.generated.hpp") == read_text_file(generated_dir / "room_ops.generated.hpp"));
    assert(read_text_file(out_dir / "room_ops.service.hpp") == read_text_file(generated_dir / "room_ops.service.hpp"));
    assert(read_text_file(out_dir / "room_ops.service.cpp") == read_text_file(generated_dir / "room_ops.service.cpp"));
    assert(read_text_file(out_dir / "room_ops.skeleton.cpp") == read_text_file(generated_dir / "room_ops.skeleton.cpp"));
    std::filesystem::remove_all(out_dir);
    std::cout << "  PASS: cpp_codegen_matches_checked_in_room_ops_fixture\n";
}

static void test_cpp_codegen_multi_rpc_dispatch() {
    std::string source = R"(
service multi 1.0.0;

struct HelloReply {
    message: string;
}

struct GoodbyeReply {
    message: string;
}

rpc Hello(name: string) returns HelloReply;
rpc Goodbye(name: string) returns GoodbyeReply;
)";

    dssl::DiagEngine diag;
    dssl::ast::File file;
    const bool ok = dssl::ast::ParseFile("multi.dssl", source, file, diag);
    assert(ok);

    dssl::sema::SemanticAnalyzer sema(diag);
    assert(sema.analyze(file));

    const auto out_dir = std::filesystem::temp_directory_path() / ("daffy-dssl-multi-" + std::to_string(std::rand()));
    std::filesystem::create_directories(out_dir);

    dssl::codegen::CppGenerator gen;
    assert(gen.generate(file, out_dir.string()));

    const auto generated = read_text_file(out_dir / "multi.service.cpp");
    assert(generated.find("request.payload.Find(\"rpc\")") != std::string::npos);
    assert(generated.find("if (rpc_name == \"Hello\")") != std::string::npos);
    assert(generated.find("if (rpc_name == \"Goodbye\")") != std::string::npos);
    assert(generated.find("Unknown RPC requested for this service") != std::string::npos);

    std::filesystem::remove_all(out_dir);
    std::cout << "  PASS: cpp_codegen_multi_rpc_dispatch\n";
}

static void test_cpp_codegen_rejects_unsupported_adapter_types() {
    std::string source = R"(
service unsupported 1.0.0;

rpc BatchEcho(messages: repeated<string>) returns repeated<string>;
)";

    dssl::DiagEngine diag;
    dssl::ast::File file;
    const bool ok = dssl::ast::ParseFile("unsupported.dssl", source, file, diag);
    assert(ok);

    dssl::sema::SemanticAnalyzer sema(diag);
    assert(sema.analyze(file));

    const auto out_dir = std::filesystem::temp_directory_path() / ("daffy-dssl-unsupported-" + std::to_string(std::rand()));
    std::filesystem::create_directories(out_dir);

    CerrCapture cerr_capture;
    dssl::codegen::CppGenerator gen;
    assert(!gen.generate(file, out_dir.string()));
    const auto diagnostics = cerr_capture.str();
    assert(diagnostics.find("RPC `BatchEcho` parameter `messages` uses unsupported adapter type") != std::string::npos);
    assert(diagnostics.find("RPC `BatchEcho` return type is unsupported for the generated adapter") != std::string::npos);
    assert(!std::filesystem::exists(out_dir / "unsupported.service.cpp"));

    std::filesystem::remove_all(out_dir);
    std::cout << "  PASS: cpp_codegen_rejects_unsupported_adapter_types\n";
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
    test_cpp_codegen_service_adapter();
    test_cpp_codegen_matches_checked_in_echo_fixture();
    test_cpp_codegen_matches_checked_in_room_ops_fixture();
    test_cpp_codegen_multi_rpc_dispatch();
    test_cpp_codegen_rejects_unsupported_adapter_types();
    std::cout << "All DSSL toolchain tests passed.\n";
    return 0;
}
