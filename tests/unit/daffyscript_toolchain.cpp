#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

#include "../../daffyc/ast/ast.h"
#include "../../daffyc/ast/ast_builder.hpp"
#include "../../daffyc/ast/ast_printer.hpp"
#include "../../daffyc/sema/sema.hpp"
#include "../../daffyc/diag/diagnostic.hpp"

static void test_detect_module() {
    std::string source = "module test_mod\nversion 1.0.0\n";
    auto ft = daffyscript::ast::DetectFileType(source);
    assert(ft == daffyscript::ast::DetectedFileType::Module);
    std::cout << "  PASS: detect_module\n";
}

static void test_detect_program() {
    std::string source = "program my_bot\nversion 1.0.0\n";
    auto ft = daffyscript::ast::DetectFileType(source);
    assert(ft == daffyscript::ast::DetectedFileType::Program);
    std::cout << "  PASS: detect_program\n";
}

static void test_detect_recipe() {
    std::string source = "recipe \"my-room\"\nversion 1.0.0\n";
    auto ft = daffyscript::ast::DetectFileType(source);
    assert(ft == daffyscript::ast::DetectedFileType::Recipe);
    std::cout << "  PASS: detect_recipe\n";
}

static void test_detect_with_comment() {
    std::string source = "-- This is a bot\nprogram echo_bot\nversion 1.0.0\n";
    auto ft = daffyscript::ast::DetectFileType(source);
    assert(ft == daffyscript::ast::DetectedFileType::Program);
    std::cout << "  PASS: detect_with_comment\n";
}

static void test_parse_minimal_module() {
    std::string source = "module hello\nversion 1.0.0\n";
    daffyscript::DiagEngine diag;
    daffyscript::ast::File file;
    bool ok = daffyscript::ast::ParseFile("hello.dfy", source, file, diag);
    if (!ok) diag.print_all(std::cerr);
    assert(ok);
    assert(file.file_type == daffyscript::ast::FileType::Module);
    assert(file.name == "hello");
    assert(file.version == "1.0.0");
    std::cout << "  PASS: parse_minimal_module\n";
}

static void test_parse_minimal_program() {
    std::string source = "program echo\nversion 1.0.0\n";
    daffyscript::DiagEngine diag;
    daffyscript::ast::File file;
    bool ok = daffyscript::ast::ParseFile("echo.dfyp", source, file, diag);
    if (!ok) diag.print_all(std::cerr);
    assert(ok);
    assert(file.file_type == daffyscript::ast::FileType::Program);
    assert(file.name == "echo");
    assert(file.version == "1.0.0");
    std::cout << "  PASS: parse_minimal_program\n";
}

static void test_parse_program_with_import() {
    std::string source =
        "program test_bot\n"
        "version 1.0.0\n"
        "\n"
        "import ldc.message\n";
    daffyscript::DiagEngine diag;
    daffyscript::ast::File file;
    bool ok = daffyscript::ast::ParseFile("test.dfyp", source, file, diag);
    if (!ok) diag.print_all(std::cerr);
    assert(ok);
    assert(file.name == "test_bot");
    std::cout << "  PASS: parse_program_with_import\n";
}

static void test_parse_program_with_struct() {
    std::string source =
        "program test_bot\n"
        "version 1.0.0\n"
        "\n"
        "struct Config {\n"
        "    name: str,\n"
        "    count: int,\n"
        "}\n";
    daffyscript::DiagEngine diag;
    daffyscript::ast::File file;
    bool ok = daffyscript::ast::ParseFile("test.dfyp", source, file, diag);
    if (!ok) diag.print_all(std::cerr);
    assert(ok);
    std::cout << "  PASS: parse_program_with_struct\n";
}

static void test_parse_program_with_fn() {
    std::string source =
        "program test_bot\n"
        "version 1.0.0\n"
        "\n"
        "fn greet(name: str) {\n"
        "    let msg = name\n"
        "}\n";
    daffyscript::DiagEngine diag;
    daffyscript::ast::File file;
    bool ok = daffyscript::ast::ParseFile("test.dfyp", source, file, diag);
    if (!ok) diag.print_all(std::cerr);
    assert(ok);
    std::cout << "  PASS: parse_program_with_fn\n";
}

static void test_parse_recipe_minimal() {
    std::string source =
        "recipe \"test-room\"\n"
        "version 1.0.0\n";
    daffyscript::DiagEngine diag;
    daffyscript::ast::File file;
    bool ok = daffyscript::ast::ParseFile("test.dfyr", source, file, diag);
    if (!ok) diag.print_all(std::cerr);
    assert(ok);
    assert(file.file_type == daffyscript::ast::FileType::Recipe);
    assert(file.name == "test-room");
    std::cout << "  PASS: parse_recipe_minimal\n";
}

static void test_sema_module() {
    std::string source = "module test_mod\nversion 1.0.0\n";
    daffyscript::DiagEngine diag;
    daffyscript::ast::File file;
    bool ok = daffyscript::ast::ParseFile("test.dfy", source, file, diag);
    assert(ok);

    daffyscript::sema::SemanticAnalyzer sema(diag);
    bool sema_ok = sema.analyze(file);
    assert(sema_ok);
    std::cout << "  PASS: sema_module\n";
}

static void test_sema_missing_version() {
    daffyscript::DiagEngine diag;
    daffyscript::ast::File file;
    file.filename = "test.dfy";
    file.file_type = daffyscript::ast::FileType::Module;
    file.name = "test";
    file.version = "";

    daffyscript::sema::SemanticAnalyzer sema(diag);
    bool sema_ok = sema.analyze(file);
    assert(!sema_ok);
    assert(diag.has_errors());
    std::cout << "  PASS: sema_missing_version\n";
}

static void test_ast_printer() {
    std::string source = "module printer_test\nversion 2.0.0\n";
    daffyscript::DiagEngine diag;
    daffyscript::ast::File file;
    bool ok = daffyscript::ast::ParseFile("test.dfy", source, file, diag);
    assert(ok);

    auto json = daffyscript::ast::PrintFile(file);
    assert(json.find("\"module\"") != std::string::npos);
    assert(json.find("\"printer_test\"") != std::string::npos);
    assert(json.find("\"2.0.0\"") != std::string::npos);
    std::cout << "  PASS: ast_printer\n";
}

static void test_bad_file_type() {
    std::string source = "garbage_keyword something\n";
    daffyscript::DiagEngine diag;
    daffyscript::ast::File file;
    bool ok = daffyscript::ast::ParseFile("bad.dfy", source, file, diag);
    assert(!ok);
    assert(diag.has_errors());
    std::cout << "  PASS: bad_file_type\n";
}

int main() {
    std::cout << "Daffyscript toolchain tests:\n";
    test_detect_module();
    test_detect_program();
    test_detect_recipe();
    test_detect_with_comment();
    test_parse_minimal_module();
    test_parse_minimal_program();
    test_parse_program_with_import();
    test_parse_program_with_struct();
    test_parse_program_with_fn();
    test_parse_recipe_minimal();
    test_sema_module();
    test_sema_missing_version();
    test_ast_printer();
    test_bad_file_type();
    std::cout << "All Daffyscript toolchain tests passed.\n";
    return 0;
}
