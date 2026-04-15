# Daffy Service Specification Language (DSSL)
### Implementation Reference for AI Agents
**Version 0.1.0-draft**

---

## Table of Contents

1. Overview & Architecture
2. Source Encoding & File Structure
3. Lexical Analysis (Flex)
4. Grammar (Bison)
5. Type System
6. Semantic Analysis
7. Code Generation & Targets
8. Plugin System (`dssl-plugin.h`)
9. Toolchain Reference
10. Standard Library & Built-in Types
11. Error Codes & Diagnostics
12. Appendix: Token Reference

---

## 1. Overview & Architecture

### 1.1 Purpose

DSSL is a **declarative interface definition language** for DaffyChat services. It describes data structures, enumerations, remote procedure calls, REST APIs, system commands, and service metadata. DSSL does not contain executable logic — it is a contract language. The actual implementation logic lives in generated C++ skeletons.

DSSL is the source of truth for:

- The C++ skeleton files that service authors fill in.
- The Lua bindings injected into DaffyChat rooms.
- The JSON/YAML metadata and documentation artifacts.
- The REST API surface exposed by a deployed service.
- The Daemon Archive (`.da`) build and deploy scripts.

### 1.2 Toolchain Overview

service.dssl
     │
     ├──► dssl-bindgen ──► C++ skeleton, Lua bindings, build/deploy scripts, metadata
     │
     ├──► dssl-docstrip ──► docstring JSON/YAML
     │
     └──► dssl-docgen ──► HTML / LaTeX documentation


`dssl-bindgen` is the primary tool. It parses DSSL, runs semantic analysis, and drives code generation for one or more targets. All other tools reuse the same parser and semantic analysis library (`libdssl`).

### 1.3 Compilation Pipeline

service.dssl
      │
      ▼
 ┌─────────────┐
 │  Flex Lexer │  → Token stream
 └─────────────┘
      │
      ▼
 ┌──────────────┐
 │ Bison Parser │  → CST
 └──────────────┘
      │
      ▼
 ┌──────────────┐
 │  AST Builder │  → AST
 └──────────────┘
      │
      ▼
 ┌──────────────────┐
 │ Semantic Analyzer│  → Typed AST + Symbol Table
 └──────────────────┘
      │
      ▼
 ┌──────────────────────────────────────────────────┐
 │  Code Generator (target-specific)                │
 │  cpp / go / rust / python / ruby / lua / json    │
 └──────────────────────────────────────────────────┘
      │
      ▼
 Output artifacts


### 1.4 Implementation File Layout

dssl/
├── lexer/
│   ├── dssl.l               -- Flex source
│   └── token.h
├── parser/
│   ├── dssl.y               -- Bison source
│   └── cst.h
├── ast/
│   ├── ast.h
│   ├── ast_builder.h/.cpp
│   └── ast_printer.h/.cpp   -- JSON dump (--gen-ast)
├── sema/
│   ├── type_system.h/.cpp
│   ├── symbol_table.h/.cpp
│   └── sema.h/.cpp
├── codegen/
│   ├── target.h             -- Target interface (abstract base)
│   ├── cpp/
│   │   ├── cpp_gen.h/.cpp
│   │   └── templates/       -- Mustache-style templates
│   ├── lua/
│   │   └── lua_gen.h/.cpp
│   ├── go/
│   │   └── go_gen.h/.cpp
│   ├── rust/
│   │   └── rust_gen.h/.cpp
│   ├── python/
│   │   └── python_gen.h/.cpp
│   ├── ruby/
│   │   └── ruby_gen.h/.cpp
│   └── json/
│       └── json_gen.h/.cpp  -- metadata + doc output
├── plugin/
│   ├── dssl-plugin.h        -- Public plugin API
│   └── plugin_loader.h/.cpp -- dlopen-based loader
├── diag/
│   ├── diagnostic.h/.cpp
│   └── source_map.h/.cpp
├── tools/
│   ├── bindgen/main.cpp     -- dssl-bindgen entry point
│   ├── docstrip/main.cpp    -- dssl-docstrip entry point
│   └── docgen/main.cpp      -- dssl-docgen entry point
└── include/
    └── dssl-plugin.h        -- Installed public header


### 1.5 `dssl-bindgen` Invocation

dssl-bindgen [options] <source.dssl>

Options:
  --target <cpp|go|rust|python|ruby>   Code generation target (repeatable)
  --out-dir <dir>                      Output directory (default: .)
  --no-lua                             Suppress Lua binding generation
  --no-scripts                         Suppress build_*.sh / deploy_*.sh
  --no-meta                            Suppress metadata JSON
  --meta-yaml                          Emit metadata as YAML instead of JSON
  --doc-yaml                           Emit docstrings as YAML instead of JSON
  --gen-ast                            Dump AST as JSON to stdout and exit
  --plugin <path.so>                   Load a bindgen plugin
  --plugin-target <name> <sexp-file>   Register a custom target from S-expression
  --validate                           Validate only, no output
  --namespace <ns>                     Override namespace for generated code


---

## 2. Source Encoding & File Structure

### 2.1 Encoding

DSSL source files must be UTF-8. Non-ASCII is permitted only in string literals, docstrings, and comments.

### 2.2 File Extension

DSSL files use the `.dssl` extension. There is only one file type.

### 2.3 Top-Level Structure

A DSSL file consists of:

1. An optional **service declaration** (at most one per file).
2. Zero or more **import declarations**.
3. Zero or more **top-level items** in any order:
   - Type definitions (`struct`, `enum`, `flags`)
   - RPC definitions (`rpc`)
   - REST endpoint definitions (`endpoint`)
   - System command definitions (`syscmd`)
   - Constant definitions (`const`)
   - Error set definitions (`errors`)
   - Event definitions (`event`)
   - Middleware definitions (`middleware`)

### 2.4 Service Declaration

Every DSSL file intended for `dssl-bindgen` should begin with a service declaration:

```dssl
service echo {
    version: "1.0.0",
    description: "Echoes messages back to the room.",
    author: "alice",
}
```

The service declaration is optional for library DSSL files (files that only define types for import by other DSSL files).

---

## 3. Lexical Analysis (Flex)

### 3.1 Flex File Header

```c
%{
#include "token.h"
#include "parser/dssl.tab.h"
#include "diag/diagnostic.h"
#include <string.h>
#include <stdlib.h>

int yyline = 1;
int yycol  = 1;

#define YY_USER_ACTION                          \
    yylloc.first_line   = yyline;               \
    yylloc.first_column = yycol;                \
    for (int i = 0; yytext[i]; i++) {           \
        if (yytext[i] == '\n') {                \
            yyline++; yycol = 1;                \
        } else { yycol++; }                     \
    }                                           \
    yylloc.last_line   = yyline;                \
    yylloc.last_column = yycol;
%}

%option noyywrap reentrant bison-bridge bison-locations
```

### 3.2 Lexer States

%s INITIAL
%x BLOCK_COMMENT
%x LINE_DOC
%x BLOCK_DOC
%x STRING_LIT


- `LINE_DOC`: Inside a `///` docstring line.
- `BLOCK_DOC`: Inside a `/**` ... `*/` docstring block.

### 3.3 Whitespace & Comments

```flex
[ \t\r\n]+          { /* skip */ }

"#"[^\n]*           { /* line comment, skip */ }

"/*"                { BEGIN(BLOCK_COMMENT); }
<BLOCK_COMMENT>"*/" { BEGIN(INITIAL); }
<BLOCK_COMMENT>\n   { /* skip */ }
<BLOCK_COMMENT>.    { /* skip */ }
<BLOCK_COMMENT><<EOF>> {
    diag_error(yyline, yycol, "E010", "Unterminated block comment");
    yyterminate();
}
```

DSSL uses `#` for line comments and `/* */` for block comments, deliberately distinct from C++ and Lua to avoid confusion in generated output.

### 3.4 Docstrings

Docstrings are attached to the immediately following declaration. They are emitted as tokens (not discarded) so the AST builder can attach them.

```flex
"///"[^\n]*         {
    /* strip leading "/// " */
    char* text = yytext + 3;
    while (*text == ' ') text++;
    yylval->str = strdup(text);
    return DOC_LINE;
}

"/**"               { doc_buf_reset(); BEGIN(BLOCK_DOC); }
<BLOCK_DOC>"*/"     {
    yylval->str = doc_buf_finalize();
    BEGIN(INITIAL);
    return DOC_BLOCK;
}
<BLOCK_DOC>\n       { doc_buf_append('\n'); }
<BLOCK_DOC>.        { doc_buf_append(yytext[0]); }
<BLOCK_DOC><<EOF>>  {
    diag_error(yyline, yycol, "E011", "Unterminated docstring block");
    yyterminate();
}
```

### 3.5 Keywords

```flex
"service"       { return KW_SERVICE; }
"import"        { return KW_IMPORT; }
"from"          { return KW_FROM; }
"as"            { return KW_AS; }
"struct"        { return KW_STRUCT; }
"enum"          { return KW_ENUM; }
"flags"         { return KW_FLAGS; }
"rpc"           { return KW_RPC; }
"endpoint"      { return KW_ENDPOINT; }
"syscmd"        { return KW_SYSCMD; }
"const"         { return KW_CONST; }
"errors"        { return KW_ERRORS; }
"event"         { return KW_EVENT; }
"middleware"    { return KW_MIDDLEWARE; }

# HTTP methods
"GET"           { return KW_GET; }
"POST"          { return KW_POST; }
"PUT"           { return KW_PUT; }
"PATCH"         { return KW_PATCH; }
"DELETE"        { return KW_DELETE; }
"HEAD"          { return KW_HEAD; }
"OPTIONS"       { return KW_OPTIONS; }

# RPC / endpoint modifiers
"returns"       { return KW_RETURNS; }
"throws"        { return KW_THROWS; }
"stream"        { return KW_STREAM; }
"auth"          { return KW_AUTH; }
"deprecated"    { return KW_DEPRECATED; }
"since"         { return KW_SINCE; }
"path"          { return KW_PATH; }
"query"         { return KW_QUERY; }
"body"          { return KW_BODY; }
"header"        { return KW_HEADER; }
"response"      { return KW_RESPONSE; }
"status"        { return KW_STATUS; }
"use"           { return KW_USE; }
"exec"          { return KW_EXEC; }
"env"           { return KW_ENV; }
"timeout"       { return KW_TIMEOUT; }
"retry"         { return KW_RETRY; }
"optional"      { return KW_OPTIONAL; }
"required"      { return KW_REQUIRED; }
"default"       { return KW_DEFAULT; }
"nullable"      { return KW_NULLABLE; }
"readonly"      { return KW_READONLY; }
"writeonly"     { return KW_WRITEONLY; }
"version"       { return KW_VERSION; }
"description"   { return KW_DESCRIPTION; }
"author"        { return KW_AUTHOR; }
"license"       { return KW_LICENSE; }
"extends"       { return KW_EXTENDS; }
"implements"    { return KW_IMPLEMENTS; }
"interface"     { return KW_INTERFACE; }
"plugin"        { return KW_PLUGIN; }
"true"          { return KW_TRUE; }
"false"         { return KW_FALSE; }
"null"          { return KW_NULL; }
```

### 3.6 Primitive Type Keywords

```flex
"bool"          { return TY_BOOL; }
"int8"          { return TY_INT8; }
"int16"         { return TY_INT16; }
"int32"         { return TY_INT32; }
"int64"         { return TY_INT64; }
"uint8"         { return TY_UINT8; }
"uint16"        { return TY_UINT16; }
"uint32"        { return TY_UINT32; }
"uint64"        { return TY_UINT64; }
"float32"       { return TY_FLOAT32; }
"float64"       { return TY_FLOAT64; }
"string"        { return TY_STRING; }
"bytes"         { return TY_BYTES; }
"timestamp"     { return TY_TIMESTAMP; }
"duration"      { return TY_DURATION; }
"uuid"          { return TY_UUID; }
"url"           { return TY_URL; }
"any"           { return TY_ANY; }
"void"          { return TY_VOID; }
```

DSSL uses explicit-width integer types (`int32`, `uint64`, etc.) because it generates bindings for multiple languages and must be unambiguous about wire format.

### 3.7 Identifiers

```flex
[a-zA-Z_][a-zA-Z0-9_]*   {
    yylval->str = strdup(yytext);
    return IDENT;
}
```

### 3.8 Literals

```flex
# Integer
[0-9]+                          {
    yylval->int_val = strtoll(yytext, NULL, 10);
    return LIT_INT;
}
0x[0-9A-Fa-f]+                  {
    yylval->int_val = strtoll(yytext + 2, NULL, 16);
    return LIT_INT;
}

# Float
[0-9]+"."[0-9]+([eE][+-]?[0-9]+)?  {
    yylval->float_val = strtod(yytext, NULL);
    return LIT_FLOAT;
}

# Version string
[0-9]+"."[0-9]+"."[0-9]+        {
    yylval->str = strdup(yytext);
    return LIT_VERSION;
}

# String
"\""                             { str_buf_reset(); BEGIN(STRING_LIT); }
<STRING_LIT>"\""                 {
    yylval->str = str_buf_finalize();
    BEGIN(INITIAL);
    return LIT_STRING;
}
<STRING_LIT>"\\n"  { str_buf_append('\n'); }
<STRING_LIT>"\\t"  { str_buf_append('\t'); }
<STRING_LIT>"\\\\" { str_buf_append('\\'); }
<STRING_LIT>"\\\""  { str_buf_append('"'); }
<STRING_LIT>.      { str_buf_append(yytext[0]); }
<STRING_LIT>\n     {
    diag_error(yyline, yycol, "E012", "Unterminated string literal");
    yyterminate();
}

# HTTP status code literal (e.g. 200, 404) — same as LIT_INT, disambiguated by parser
```

### 3.9 Path Segments

REST endpoint paths contain segments like `/users/{id}/posts`. The path string is lexed as a `LIT_STRING` and parsed by a dedicated path parser in the semantic analysis phase. Path parameter names (`{id}`) are extracted and cross-referenced with the endpoint's `path` parameter declarations.

### 3.10 Operators & Punctuation

```flex
"{"     { return LBRACE; }
"}"     { return RBRACE; }
"["     { return LBRACKET; }
"]"     { return RBRACKET; }
"("     { return LPAREN; }
")"     { return RPAREN; }
","     { return COMMA; }
":"     { return COLON; }
";"     { return SEMICOLON; }
"."     { return DOT; }
"="     { return EQUALS; }
"?"     { return QUESTION; }
"|"     { return PIPE; }
"@"     { return AT; }
"->"    { return ARROW; }
"<"     { return LT; }
">"     { return GT; }
"*"     { return STAR; }
"!"     { return BANG; }
```

---

## 4. Grammar (Bison)

### 4.1 Bison File Header

```c
%{
#include "ast/ast.h"
#include "ast/ast_builder.h"
#include "diag/diagnostic.h"
#include <stdio.h>

extern int yylex(YYSTYPE*, YYLTYPE*, yyscan_t);
extern void yyerror(YYLTYPE*, yyscan_t, ASTNode**, const char*);
%}

%define api.pure full
%define parse.error verbose
%define parse.lac full
%locations
%param { yyscan_t scanner }
%parse-param { ASTNode** ast_root }

%union {
    int64_t     int_val;
    double      float_val;
    char*       str;
    ASTNode*    node;
    ASTList*    list;
    DSSLType*   type;
    HttpMethod  method;
}
```

### 4.2 Top-Level Grammar

```bison
%start translation_unit

translation_unit
    : opt_service_decl import_list top_level_item_list
        { *ast_root = ast_file($1, $2, $3); }
    ;

opt_service_decl
    : /* empty */       { $$ = NULL; }
    | service_decl      { $$ = $1; }
    ;

top_level_item_list
    : /* empty */
        { $$ = ast_list_empty(); }
    | top_level_item_list doc_comment top_level_item
        { $$ = ast_list_append($1, ast_attach_doc($3, $2)); }
    | top_level_item_list top_level_item
        { $$ = ast_list_append($1, $2); }
    ;

top_level_item
    : struct_def
    | enum_def
    | flags_def
    | rpc_def
    | endpoint_def
    | syscmd_def
    | const_def
    | errors_def
    | event_def
    | middleware_def
    | interface_def
    ;
```

### 4.3 Service Declaration

```bison
service_decl
    : KW_SERVICE IDENT LBRACE service_props RBRACE
        { $$ = ast_service($2, $4); }
    ;

service_props
    : /* empty */                          { $$ = ast_list_empty(); }
    | service_props service_prop           { $$ = ast_list_append($1, $2); }
    ;

service_prop
    : KW_VERSION COLON LIT_STRING COMMA    { $$ = ast_sprop("version", $3); }
    | KW_VERSION COLON LIT_STRING          { $$ = ast_sprop("version", $3); }
    | KW_DESCRIPTION COLON LIT_STRING COMMA { $$ = ast_sprop("description", $3); }
    | KW_DESCRIPTION COLON LIT_STRING      { $$ = ast_sprop("description", $3); }
    | KW_AUTHOR COLON LIT_STRING COMMA     { $$ = ast_sprop("author", $3); }
    | KW_AUTHOR COLON LIT_STRING           { $$ = ast_sprop("author", $3); }
    | KW_LICENSE COLON LIT_STRING COMMA    { $$ = ast_sprop("license", $3); }
    | KW_LICENSE COLON LIT_STRING          { $$ = ast_sprop("license", $3); }
    ;
```

### 4.4 Import Declarations

```bison
import_list
    : /* empty */                  { $$ = ast_list_empty(); }
    | import_list import_decl      { $$ = ast_list_append($1, $2); }
    ;

import_decl
    : KW_IMPORT LIT_STRING
        { $$ = ast_import($2, NULL); }
    | KW_IMPORT LIT_STRING KW_AS IDENT
        { $$ = ast_import($2, $4); }
    | KW_IMPORT LBRACE import_items RBRACE KW_FROM LIT_STRING
        { $$ = ast_import_named($6, $3); }
    ;

import_items
    : import_item                          { $$ = ast_list_single($1); }
    | import_items COMMA import_item       { $$ = ast_list_append($1, $3); }
    ;

import_item
    : IDENT                    { $$ = ast_import_item($1, NULL); }
    | IDENT KW_AS IDENT        { $$ = ast_import_item($1, $3); }
    ;
```

### 4.5 Type Expressions

```bison
type_expr
    : primitive_type
    | IDENT
        { $$ = dssl_type_named($1); }
    | IDENT DOT IDENT
        { $$ = dssl_type_qualified($1, $3); }
    | type_expr QUESTION
        { $$ = dssl_type_optional($1); }
    | LBRACKET type_expr RBRACKET
        { $$ = dssl_type_list($2); }
    | LBRACE type_expr COLON type_expr RBRACE
        { $$ = dssl_type_map($2, $4); }
    | LPAREN type_list RPAREN
        { $$ = dssl_type_tuple($2); }
    | IDENT LT type_list GT
        { $$ = dssl_type_generic($1, $3); }
    | type_expr PIPE type_expr
        { $$ = dssl_type_union($1, $3); }
    ;

primitive_type
    : TY_BOOL     { $$ = dssl_type_prim(PRIM_BOOL); }
    | TY_INT8     { $$ = dssl_type_prim(PRIM_INT8); }
    | TY_INT16    { $$ = dssl_type_prim(PRIM_INT16); }
    | TY_INT32    { $$ = dssl_type_prim(PRIM_INT32); }
    | TY_INT64    { $$ = dssl_type_prim(PRIM_INT64); }
    | TY_UINT8    { $$ = dssl_type_prim(PRIM_UINT8); }
    | TY_UINT16   { $$ = dssl_type_prim(PRIM_UINT16); }
    | TY_UINT32   { $$ = dssl_type_prim(PRIM_UINT32); }
    | TY_UINT64   { $$ = dssl_type_prim(PRIM_UINT64); }
    | TY_FLOAT32  { $$ = dssl_type_prim(PRIM_FLOAT32); }
    | TY_FLOAT64  { $$ = dssl_type_prim(PRIM_FLOAT64); }
    | TY_STRING   { $$ = dssl_type_prim(PRIM_STRING); }
    | TY_BYTES    { $$ = dssl_type_prim(PRIM_BYTES); }
    | TY_TIMESTAMP { $$ = dssl_type_prim(PRIM_TIMESTAMP); }
    | TY_DURATION  { $$ = dssl_type_prim(PRIM_DURATION); }
    | TY_UUID     { $$ = dssl_type_prim(PRIM_UUID); }
    | TY_URL      { $$ = dssl_type_prim(PRIM_URL); }
    | TY_ANY      { $$ = dssl_type_prim(PRIM_ANY); }
    | TY_VOID     { $$ = dssl_type_prim(PRIM_VOID); }
    ;

type_list
    : type_expr                    { $$ = ast_list_single($1); }
    | type_list COMMA type_expr    { $$ = ast_list_append($1, $3); }
    ;
```

### 4.6 Struct Definition

```bison
struct_def
    : KW_STRUCT IDENT LBRACE struct_body RBRACE
        { $$ = ast_struct($2, NULL, NULL, $4); }
    | KW_STRUCT IDENT LT type_params GT LBRACE struct_body RBRACE
        { $$ = ast_struct($2, $4, NULL, $7); }
    | KW_STRUCT IDENT KW_EXTENDS IDENT LBRACE struct_body RBRACE
        { $$ = ast_struct($2, NULL, $4, $6); }
    | KW_STRUCT IDENT LT type_params GT KW_EXTENDS IDENT LBRACE struct_body RBRACE
        { $$ = ast_struct($2, $4, $7, $9); }
    ;

struct_body
    : /* empty */                      { $$ = ast_list_empty(); }
    | struct_body doc_comment field_decl
        { $$ = ast_list_append($1, ast_attach_doc($3, $2)); }
    | struct_body field_decl
        { $$ = ast_list_append($1, $2); }
    ;

field_decl
    : IDENT COLON type_expr field_attrs SEMICOLON
        { $$ = ast_field($1, $3, $4); }
    ;

field_attrs
    : /* empty */                      { $$ = ast_list_empty(); }
    | field_attrs field_attr           { $$ = ast_list_append($1, $2); }
    ;

field_attr
    : AT KW_OPTIONAL                   { $$ = ast_fattr_optional(); }
    | AT KW_REQUIRED                   { $$ = ast_fattr_required(); }
    | AT KW_NULLABLE                   { $$ = ast_fattr_nullable(); }
    | AT KW_READONLY                   { $$ = ast_fattr_readonly(); }
    | AT KW_WRITEONLY                  { $$ = ast_fattr_writeonly(); }
    | AT KW_DEFAULT LPAREN const_val RPAREN
        { $$ = ast_fattr_default($4); }
    | AT KW_DEPRECATED                 { $$ = ast_fattr_deprecated(NULL); }
    | AT KW_DEPRECATED LPAREN LIT_STRING RPAREN
        { $$ = ast_fattr_deprecated($4); }
    | AT IDENT LPAREN attr_args RPAREN
        { $$ = ast_fattr_custom($2, $4); }
    | AT IDENT
        { $$ = ast_fattr_custom($2, NULL); }
    ;

attr_args
    : attr_arg                         { $$ = ast_list_single($1); }
    | attr_args COMMA attr_arg         { $$ = ast_list_append($1, $3); }
    ;

attr_arg
    : const_val                        { $$ = $1; }
    | IDENT EQUALS const_val           { $$ = ast_named_arg($1, $3); }
    ;

const_val
    : LIT_INT      { $$ = ast_cv_int($1); }
    | LIT_FLOAT    { $$ = ast_cv_float($1); }
    | LIT_STRING   { $$ = ast_cv_string($1); }
    | KW_TRUE      { $$ = ast_cv_bool(1); }
    | KW_FALSE     { $$ = ast_cv_bool(0); }
    | KW_NULL      { $$ = ast_cv_null(); }
    | IDENT        { $$ = ast_cv_ref($1); }
    | IDENT DOT IDENT { $$ = ast_cv_qref($1, $3); }
    ;
```

### 4.7 Enum Definition

```bison
enum_def
    : KW_ENUM IDENT LBRACE enum_body RBRACE
        { $$ = ast_enum($2, NULL, $4); }
    | KW_ENUM IDENT COLON primitive_type LBRACE enum_body RBRACE
        { $$ = ast_enum_typed($2, $4, $6); }
    ;

enum_body
    : /* empty */                      { $$ = ast_list_empty(); }
    | enum_body doc_comment enum_variant
        { $$ = ast_list_append($1, ast_attach_doc($3, $2)); }
    | enum_body enum_variant
        { $$ = ast_list_append($1, $2); }
    ;

enum_variant
    : IDENT COMMA
        { $$ = ast_enum_variant($1, NULL); }
    | IDENT EQUALS LIT_INT COMMA
        { $$ = ast_enum_variant_val($1, $3); }
    | IDENT EQUALS LIT_STRING COMMA
        { $$ = ast_enum_variant_str($1, $3); }
    | IDENT                            /* last variant, no comma */
        { $$ = ast_enum_variant($1, NULL); }
    | IDENT EQUALS LIT_INT
        { $$ = ast_enum_variant_val($1, $3); }
    | IDENT EQUALS LIT_STRING
        { $$ = ast_enum_variant_str($1, $3); }
    ;
```

### 4.8 Flags Definition

`flags` is a bitmask enum. Each variant is a power-of-two bit position.

```bison
flags_def
    : KW_FLAGS IDENT LBRACE flags_body RBRACE
        { $$ = ast_flags($2, $4); }
    ;

flags_body
    : /* empty */                      { $$ = ast_list_empty(); }
    | flags_body doc_comment flags_variant
        { $$ = ast_list_append($1, ast_attach_doc($3
    | flags_body flags_variant
        { $$ = ast_list_append($1, $2); }
    ;

flags_variant
    : IDENT COMMA
        { $$ = ast_flags_variant($1, -1); }   /* auto-assign next bit */
    | IDENT EQUALS LIT_INT COMMA
        { $$ = ast_flags_variant_val($1, $3); }
    | IDENT                                    /* last variant */
        { $$ = ast_flags_variant($1, -1); }
    | IDENT EQUALS LIT_INT
        { $$ = ast_flags_variant_val($1, $3); }
    ;
```

### 4.9 RPC Definition

```bison
rpc_def
    : KW_RPC IDENT LPAREN param_list RPAREN rpc_tail
        { $$ = ast_rpc($2, $4, $6); }
    ;

rpc_tail
    : rpc_modifiers SEMICOLON
        { $$ = ast_rpc_tail(NULL, NULL, $1); }
    | KW_RETURNS type_expr rpc_modifiers SEMICOLON
        { $$ = ast_rpc_tail($2, NULL, $3); }
    | KW_THROWS error_ref_list rpc_modifiers SEMICOLON
        { $$ = ast_rpc_tail(NULL, $2, $3); }
    | KW_RETURNS type_expr KW_THROWS error_ref_list rpc_modifiers SEMICOLON
        { $$ = ast_rpc_tail($2, $4, $5); }
    ;

rpc_modifiers
    : /* empty */                          { $$ = ast_list_empty(); }
    | rpc_modifiers rpc_modifier           { $$ = ast_list_append($1, $2); }
    ;

rpc_modifier
    : AT KW_STREAM                         { $$ = ast_rmod_stream(); }
    | AT KW_AUTH LPAREN LIT_STRING RPAREN  { $$ = ast_rmod_auth($4); }
    | AT KW_AUTH                           { $$ = ast_rmod_auth(NULL); }
    | AT KW_DEPRECATED                     { $$ = ast_rmod_deprecated(NULL); }
    | AT KW_DEPRECATED LPAREN LIT_STRING RPAREN
        { $$ = ast_rmod_deprecated($4); }
    | AT KW_SINCE LPAREN LIT_STRING RPAREN
        { $$ = ast_rmod_since($4); }
    | AT KW_TIMEOUT LPAREN LIT_INT RPAREN
        { $$ = ast_rmod_timeout($4); }
    | AT KW_RETRY LPAREN LIT_INT RPAREN
        { $$ = ast_rmod_retry($4); }
    ;

param_list
    : /* empty */                          { $$ = ast_list_empty(); }
    | param_list_nonempty                  { $$ = $1; }
    ;

param_list_nonempty
    : param_decl                           { $$ = ast_list_single($1); }
    | param_list_nonempty COMMA param_decl { $$ = ast_list_append($1, $3); }
    ;

param_decl
    : IDENT COLON type_expr
        { $$ = ast_param($1, $3, NULL, 0); }
    | IDENT COLON type_expr EQUALS const_val
        { $$ = ast_param($1, $3, $5, 0); }
    | IDENT COLON type_expr AT KW_OPTIONAL
        { $$ = ast_param($1, $3, NULL, PARAM_OPTIONAL); }
    | IDENT COLON type_expr QUESTION
        { $$ = ast_param($1, dssl_type_optional($3), NULL, PARAM_OPTIONAL); }
    ;

error_ref_list
    : error_ref                            { $$ = ast_list_single($1); }
    | error_ref_list PIPE error_ref        { $$ = ast_list_append($1, $3); }
    ;

error_ref
    : IDENT                                { $$ = ast_error_ref($1, NULL); }
    | IDENT DOT IDENT                      { $$ = ast_error_ref($1, $3); }
    ;
```

### 4.10 REST Endpoint Definition

```bison
endpoint_def
    : KW_ENDPOINT http_method LIT_STRING LBRACE endpoint_body RBRACE
        { $$ = ast_endpoint($2, $3, $5); }
    ;

http_method
    : KW_GET        { $$ = HTTP_GET; }
    | KW_POST       { $$ = HTTP_POST; }
    | KW_PUT        { $$ = HTTP_PUT; }
    | KW_PATCH      { $$ = HTTP_PATCH; }
    | KW_DELETE     { $$ = HTTP_DELETE; }
    | KW_HEAD       { $$ = HTTP_HEAD; }
    | KW_OPTIONS    { $$ = HTTP_OPTIONS; }
    ;

endpoint_body
    : /* empty */                          { $$ = ast_list_empty(); }
    | endpoint_body endpoint_clause        { $$ = ast_list_append($1, $2); }
    ;

endpoint_clause
    : KW_PATH LBRACE param_list RBRACE SEMICOLON
        { $$ = ast_ep_path($3); }
    | KW_QUERY LBRACE param_list RBRACE SEMICOLON
        { $$ = ast_ep_query($3); }
    | KW_HEADER LBRACE param_list RBRACE SEMICOLON
        { $$ = ast_ep_header($3); }
    | KW_BODY COLON type_expr SEMICOLON
        { $$ = ast_ep_body($3); }
    | KW_RESPONSE LBRACE response_list RBRACE SEMICOLON
        { $$ = ast_ep_responses($3); }
    | KW_AUTH LPAREN LIT_STRING RPAREN SEMICOLON
        { $$ = ast_ep_auth($4); }
    | AT KW_DEPRECATED SEMICOLON
        { $$ = ast_ep_deprecated(NULL); }
    | AT KW_DEPRECATED LPAREN LIT_STRING RPAREN SEMICOLON
        { $$ = ast_ep_deprecated($4); }
    | KW_USE IDENT SEMICOLON
        { $$ = ast_ep_middleware($2); }
    ;

response_list
    : response_entry                       { $$ = ast_list_single($1); }
    | response_list response_entry         { $$ = ast_list_append($1, $2); }
    ;

response_entry
    : LIT_INT COLON type_expr SEMICOLON
        { $$ = ast_response($1, $3); }
    | LIT_INT COLON KW_VOID SEMICOLON
        { $$ = ast_response($1, dssl_type_prim(PRIM_VOID)); }
    ;
```

### 4.11 System Command Definition

```bison
syscmd_def
    : KW_SYSCMD IDENT LBRACE syscmd_body RBRACE
        { $$ = ast_syscmd($2, $4); }
    ;

syscmd_body
    : /* empty */                          { $$ = ast_list_empty(); }
    | syscmd_body syscmd_clause            { $$ = ast_list_append($1, $2); }
    ;

syscmd_clause
    : KW_EXEC COLON LIT_STRING SEMICOLON
        { $$ = ast_sc_exec($3); }
    | KW_ENV LBRACE env_list RBRACE SEMICOLON
        { $$ = ast_sc_env($3); }
    | KW_TIMEOUT COLON LIT_INT SEMICOLON
        { $$ = ast_sc_timeout($3); }
    | KW_RETRY COLON LIT_INT SEMICOLON
        { $$ = ast_sc_retry($3); }
    | KW_RETURNS type_expr SEMICOLON
        { $$ = ast_sc_returns($2); }
    | KW_THROWS error_ref_list SEMICOLON
        { $$ = ast_sc_throws($2); }
    ;

env_list
    : env_entry                            { $$ = ast_list_single($1); }
    | env_list COMMA env_entry             { $$ = ast_list_append($1, $3); }
    ;

env_entry
    : IDENT COLON type_expr
        { $$ = ast_env_entry($1, $3, NULL); }
    | IDENT COLON type_expr EQUALS LIT_STRING
        { $$ = ast_env_entry($1, $3, $5); }
    ;
```

### 4.12 Constant Definition

```bison
const_def
    : KW_CONST IDENT COLON type_expr EQUALS const_val SEMICOLON
        { $$ = ast_const($2, $4, $6); }
    ;
```

### 4.13 Error Set Definition

```bison
errors_def
    : KW_ERRORS IDENT LBRACE error_body RBRACE
        { $$ = ast_errors($2, $4); }
    ;

error_body
    : /* empty */                          { $$ = ast_list_empty(); }
    | error_body doc_comment error_entry
        { $$ = ast_list_append($1, ast_attach_doc($3, $2)); }
    | error_body error_entry
        { $$ = ast_list_append($1, $2); }
    ;

error_entry
    : IDENT EQUALS LIT_INT SEMICOLON
        { $$ = ast_error_entry($1, $3, NULL); }
    | IDENT EQUALS LIT_INT COLON LIT_STRING SEMICOLON
        { $$ = ast_error_entry($1, $3, $5); }
    ;
```

### 4.14 Event Definition

```bison
event_def
    : KW_EVENT IDENT LBRACE param_list RBRACE SEMICOLON
        { $$ = ast_event($2, $4); }
    | KW_EVENT IDENT LBRACE param_list RBRACE
        KW_THROWS error_ref_list SEMICOLON
        { $$ = ast_event_throws($2, $4, $7); }
    ;
```

### 4.15 Middleware Definition

```bison
middleware_def
    : KW_MIDDLEWARE IDENT LBRACE middleware_body RBRACE
        { $$ = ast_middleware($2, $4); }
    ;

middleware_body
    : /* empty */                          { $$ = ast_list_empty(); }
    | middleware_body middleware_clause    { $$ = ast_list_append($1, $2); }
    ;

middleware_clause
    : KW_BODY COLON type_expr SEMICOLON
        { $$ = ast_mw_body($3); }
    | KW_HEADER LBRACE param_list RBRACE SEMICOLON
        { $$ = ast_mw_header($3); }
    | KW_THROWS error_ref_list SEMICOLON
        { $$ = ast_mw_throws($2); }
    ;
```

### 4.16 Interface Definition

Interfaces define abstract service contracts that concrete services can implement.

```bison
interface_def
    : KW_INTERFACE IDENT LBRACE interface_body RBRACE
        { $$ = ast_interface($2, NULL, $4); }
    | KW_INTERFACE IDENT KW_EXTENDS IDENT LBRACE interface_body RBRACE
        { $$ = ast_interface($2, $4, $6); }
    ;

interface_body
    : /* empty */                          { $$ = ast_list_empty(); }
    | interface_body doc_comment interface_item
        { $$ = ast_list_append($1, ast_attach_doc($3, $2)); }
    | interface_body interface_item
        { $$ = ast_list_append($1, $2); }
    ;

interface_item
    : rpc_def
    | endpoint_def
    | event_def
    ;
```

### 4.17 Docstring Grammar

```bison
doc_comment
    : DOC_LINE doc_lines
        { $$ = ast_doc_concat($1, $2); }
    | DOC_BLOCK
        { $$ = ast_doc_block($1); }
    ;

doc_lines
    : /* empty */          { $$ = NULL; }
    | doc_lines DOC_LINE   { $$ = ast_doc_concat($1, $2); }
    ;
```

### 4.18 Generic Type Parameters

```bison
type_params
    : IDENT                            { $$ = ast_list_single(ast_tparam($1, NULL)); }
    | type_params COMMA IDENT          { $$ = ast_list_append($1, ast_tparam($3, NULL)); }
    | IDENT COLON type_expr            { $$ = ast_list_single(ast_tparam($1, $3)); }
    | type_params COMMA IDENT COLON type_expr
        { $$ = ast_list_append($1, ast_tparam($3, $5)); }
    ;
```

---

## 5. Type System

### 5.1 Type Representation in C

```c
/* type_system.h */

typedef enum {
    PRIM_BOOL,
    PRIM_INT8,  PRIM_INT16,  PRIM_INT32,  PRIM_INT64,
    PRIM_UINT8, PRIM_UINT16, PRIM_UINT32, PRIM_UINT64,
    PRIM_FLOAT32, PRIM_FLOAT64,
    PRIM_STRING, PRIM_BYTES,
    PRIM_TIMESTAMP, PRIM_DURATION,
    PRIM_UUID, PRIM_URL,
    PRIM_ANY, PRIM_VOID,
    PRIM_COUNT
} PrimKind;

typedef enum {
    TK_PRIM,
    TK_NAMED,       /* user-defined struct/enum/flags */
    TK_OPTIONAL,    /* T? */
    TK_LIST,        /* [T] */
    TK_MAP,         /* {K: V} */
    TK_TUPLE,       /* (T1, T2, ...) */
    TK_UNION,       /* T1 | T2 */
    TK_GENERIC,     /* Name<T1, T2> */
    TK_TYPEVAR,     /* T (in generic context) */
} TypeKind;

typedef struct DSSLType {
    TypeKind kind;
    union {
        PrimKind prim;
        struct {
            char*           name;
            char*           qualifier;   /* for qualified names: qualifier.name */
            struct Symbol*  resolved;    /* filled by sema */
        } named;
        struct DSSLType*    inner;       /* optional, list */
        struct {
            struct DSSLType* key;
            struct DSSLType* val;
        } map;
        struct {
            struct DSSLType** elems;
            int               count;
        } tuple;
        struct {
            struct DSSLType* left;
            struct DSSLType* right;
        } union_;
        struct {
            char*             name;
            struct DSSLType** args;
            int               argc;
        } generic;
        struct {
            char*             name;
            struct DSSLType*  bound;     /* constraint, may be NULL */
        } typevar;
    };
    SourceLoc loc;
} DSSLType;
```

### 5.2 Wire Format Mapping

| DSSL Type | C++ | Go | Rust | Python | Lua | JSON |
|---|---|---|---|---|---|---|
| `bool` | `bool` | `bool` | `bool` | `bool` | `boolean` | `boolean` |
| `int8` | `int8_t` | `int8` | `i8` | `int` | `integer` | `number` |
| `int16` | `int16_t` | `int16` | `i16` | `int` | `integer` | `number` |
| `int32` | `int32_t` | `int32` | `i32` | `int` | `integer` | `number` |
| `int64` | `int64_t` | `int64` | `i64` | `int` | `integer` | `string`* |
| `uint8` | `uint8_t` | `uint8` | `u8` | `int` | `integer` | `number` |
| `uint16` | `uint16_t` | `uint16` | `u16` | `int` | `integer` | `number` |
| `uint32` | `uint32_t` | `uint32` | `u32` | `int` | `integer` | `number` |
| `uint64` | `uint64_t` | `uint64` | `u64` | `int` | `integer` | `string`* |
| `float32` | `float` | `float32` | `f32` | `float` | `number` | `number` |
| `float64` | `double` | `float64` | `f64` | `float` | `number` | `number` |
| `string` | `std::string` | `string` | `String` | `str` | `string` | `string` |
| `bytes` | `std::vector<uint8_t>` | `[]byte` | `Vec<u8>` | `bytes` | `string` | `string` (base64) |
| `timestamp` | `int64_t` (unix ms) | `time.Time` | `DateTime<Utc>` | `datetime` | `number` | `string` (ISO 8601) |
| `duration` | `int64_t` (ms) | `time.Duration` | `Duration` | `timedelta` | `number` | `number` (ms) |
| `uuid` | `std::string` | `uuid.UUID` | `Uuid` | `UUID` | `string` | `string` |
| `url` | `std::string` | `url.URL` | `Url` | `str` | `string` | `string` |
| `any` | `nlohmann::json` | `interface{}` | `serde_json::Value` | `Any` | `table` | `any` |
| `void` | `void` | — | `()` | `None` | — | `null` |

*`int64`/`uint64` serialize as JSON strings to avoid precision loss in JavaScript.

### 5.3 Optional Types

`T?` maps to:
- C++: `std::optional<T>`
- Go: `*T` (pointer)
- Rust: `Option<T>`
- Python: `Optional[T]`
- Lua: `T | nil`

### 5.4 List Types

`[T]` maps to:
- C++: `std::vector<T>`
- Go: `[]T`
- Rust: `Vec<T>`
- Python: `List[T]`
- Lua: `table` (array)

### 5.5 Map Types

`{K: V}` maps to:
- C++: `std::unordered_map<K, V>`
- Go: `map[K]V`
- Rust: `HashMap<K, V>`
- Python: `Dict[K, V]`
- Lua: `table`

Map key types are restricted to: `string`, `int32`, `int64`, `uint32`, `uint64`, `uuid`. Semantic analysis enforces this.

### 5.6 Generic Instantiation

DSSL supports generic structs. Generics are monomorphized at code generation time for C++ and Rust, and use type parameters for Go, Python, and Ruby.

```dssl
struct Page<T> {
    items: [T];
    total: int32;
    page: int32;
    per_page: int32;
}

rpc listUsers(filter: UserFilter) returns Page<User>;
```

The semantic analyzer builds a monomorphization table. Each unique instantiation (`Page<User>`, `Page<Post>`, etc.) gets its own entry. Code generators use this table to emit concrete types for languages that require it.

---

## 6. Semantic Analysis

### 6.1 Analysis Passes

Semantic analysis runs in 7 ordered passes over the typed AST:

| Pass | Name | Description |
|---|---|---|
| 1 | `collect_symbols` | Build top-level symbol table (types, RPCs, endpoints, etc.) |
| 2 | `resolve_imports` | Load imported DSSL files, merge their symbol tables |
| 3 | `resolve_types` | Resolve all named type references to symbol table entries |
| 4 | `check_struct_cycles` | Detect circular struct references (must use `?` to break cycles) |
| 5 | `check_generics` | Validate generic parameter counts and constraints |
| 6 | `check_rpc_types` | Validate RPC parameter/return types, error refs |
| 7 | `check_endpoint_paths` | Parse path strings, cross-reference `{param}` with `path {}` block |

### 6.2 Symbol Table

```c
/* symbol_table.h */

typedef enum {
    SYM_STRUCT,
    SYM_ENUM,
    SYM_FLAGS,
    SYM_RPC,
    SYM_ENDPOINT,
    SYM_SYSCMD,
    SYM_CONST,
    SYM_ERRORS,
    SYM_EVENT,
    SYM_MIDDLEWARE,
    SYM_INTERFACE,
    SYM_TYPEVAR,
    SYM_IMPORT,
} SymbolKind;

typedef struct Symbol {
    SymbolKind      kind;
    char*           name;
    char*           qualified_name;  /* module.name for imports */
    ASTNode*        decl;
    DSSLType*       type;            /* for consts, fields */
    SourceLoc       loc;
    bool            is_public;
    bool            is_deprecated;
    char*           deprecated_msg;
    struct Symbol*  next;            /* hash chain */
} Symbol;

typedef struct SymbolTable {
    Symbol**        buckets;
    int             bucket_count;
    struct SymbolTable* parent;      /* for scoped lookups */
} SymbolTable;

Symbol* symtab_lookup(SymbolTable* st, const char* name);
Symbol* symtab_lookup_qualified(SymbolTable* st,
                                const char* qualifier,
                                const char* name);
bool    symtab_insert(SymbolTable* st, Symbol* sym);
```

### 6.3 Pass 1: `collect_symbols`

Iterates all top-level items and inserts them into the global symbol table. Reports `E100` for duplicate names.

```c
void pass_collect_symbols(ASTNode* file, SymbolTable* st, DiagList* diags) {
    ASTList* items = file->file.items;
    for (int i = 0; i < items->count; i++) {
        ASTNode* item = items->nodes[i];
        Symbol* sym = symbol_from_decl(item);
        if (!symtab_insert(st, sym)) {
            Symbol* existing = symtab_lookup(st, sym->name);
            diag_error_at(diags, item->loc, "E100",
                "Duplicate symbol '%s', previously defined at %s:%d:%d",
                sym->name,
                existing->loc.file,
                existing->loc.line,
                existing->loc.col);
        }
    }
}
```

### 6.4 Pass 3: `resolve_types`

Walks all type expressions in the AST and resolves `TK_NAMED` types to their symbol table entries. Reports `E101` for unknown types.

```c
void resolve_type(DSSLType* ty, SymbolTable* st, DiagList* diags) {
    switch (ty->kind) {
    case TK_NAMED: {
        Symbol* sym = ty->named.qualifier
            ? symtab_lookup_qualified(st, ty->named.qualifier, ty->named.name)
            : symtab_lookup(st, ty->named.name);
        if (!sym) {
            diag_error_at(diags, ty->loc, "E101",
                "Unknown type '%s'", ty->named.name);
        } else if (sym->kind != SYM_STRUCT &&
                   sym->kind != SYM_ENUM   &&
                   sym->kind != SYM_FLAGS  &&
                   sym->kind != SYM_TYPEVAR) {
            diag_error_at(diags, ty->loc, "E102",
                "'%s' is not a type", ty->named.name);
        } else {
            ty->named.resolved = sym;
        }
        break;
    }
    case TK_OPTIONAL: resolve_type(ty->inner, st, diags); break;
    case TK_LIST:     resolve_type(ty->inner, st, diags); break;
    case TK_MAP:
        resolve_type(ty->map.key, st, diags);
        resolve_type(ty->map.val, st, diags);
        break;
    /* ... etc */
    }
}
```

### 6.5 Pass 4: `check_struct_cycles`

Uses DFS with a `GREY/BLACK` coloring scheme. A `GREY` node encountered during DFS indicates a cycle. Cycles are only permitted if the back-edge field is `T?` (optional), since optional fields can be null-terminated.

```c
typedef enum { WHITE, GREY, BLACK } Color;

void dfs_struct(Symbol* sym, Color* colors, SymbolTable* st, DiagList* diags) {
    if (colors[sym->index] == BLACK) return;
    if (colors[sym->index] == GREY) {
        diag_error_at(diags, sym->loc, "E103",
            "Circular struct reference involving '%s'; "
            "use optional type '?' to break the cycle", sym->name);
        return;
    }
    colors[sym->index] = GREY;
    ASTNode* s = sym->decl;
    for (int i = 0; i < s->struct_.fields->count; i++) {
        ASTNode* field = s->struct_.fields->nodes[i];
        DSSLType* ty = field->field.type;
        /* unwrap optional — optional fields are allowed to be cyclic */
        bool is_optional = (ty->kind == TK_OPTIONAL);
        DSSLType* inner = is_optional ? ty->inner : ty;
        if (inner->kind == TK_NAMED && inner->named.resolved) {
            Symbol* dep = inner->named.resolved;
            if (dep->kind == SYM_STRUCT) {
                if (!is_optional && colors[dep->index] == GREY) {
                    diag_error_at(diags, field->loc, "E103",
                        "Circular reference to '%s' must be optional",
                        dep->name);
                } else {
                    dfs_struct(dep, colors, st, diags);
                }
            }
        }
    }
    colors[sym->index] = BLACK;
}
```

### 6.6 Pass 7: `check_endpoint_paths`

Parses the path string (e.g. `"/users/{id}/posts/{post_id}"`) and extracts parameter names. Cross-references them with the `path {}` block parameters. Reports errors for:

- `E110`: Path parameter `{x}` has no matching `path` declaration.
- `E111`: `path` declaration `x` has no matching `{x}` in path string.
- `E112`: Path string does not start with `/`.
- `E113`: Empty path segment (`//`).

```c
void check_endpoint_path(ASTNode* ep, DiagList* diags) {
    const char* path_str = ep->endpoint.path;
    if (path_str[0] != '/') {
        diag_error_at(diags, ep->loc, "E112",
            "Endpoint path must start with '/'");
    }

    StrSet* path_params = extract_path_params(path_str, ep->loc, diags);
    StrSet* decl_params = collect_path_decl_params(ep);

    /* every {x} must have a declaration */
    StrSetIter it;
    strset_iter_init(&it, path_params);
    const char* name;
    while ((name = strset_iter_next(&it))) {
        if (!strset_contains(decl_params, name)) {
            diag_error_at(diags, ep->loc, "E110",
                "Path parameter '{%s}' has no 'path' declaration", name);
        }
    }

    /* every path declaration must appear in the path string */
    strset_iter_init(&it, decl_params);
    while ((name = strset_iter_next(&it))) {
        if (!strset_contains(path_params, name)) {
            diag_error_at(diags, ep->loc, "E111",
                "Path declaration '%s' has no '{%s}' in path string",
                name, name);
        }
    }
}
```

---

## 7. Code Generation & Targets

### 7.1 Target Interface

All code generators implement the `DSSLTarget` abstract interface:

```c
/* codegen/target.h */

typedef struct DSSLTarget {
    const char* name;

    /* Called once before any items are generated */
    void (*begin)(struct DSSLTarget* self,
                  ASTNode* file,
                  const char* out_dir,
                  const char* namespace_);

    /* Called for each top-level item */
    void (*emit_struct)    (struct DSSLTarget*, ASTNode*);
    void (*emit_enum)      (struct DSSLTarget*, ASTNode*);
    void (*emit_flags)     (struct DSSLTarget*, ASTNode*);
    void (*emit_rpc)       (struct DSSLTarget*, ASTNode*);
    void (*emit_endpoint)  (struct DSSLTarget*, ASTNode*);
    void (*emit_syscmd)    (struct DSSLTarget*, ASTNode*);
    void (*emit_const)     (struct DSSLTarget*, ASTNode*);
    void (*emit_errors)    (struct DSSLTarget*, ASTNode*);
    void (*emit_event)     (struct DSSLTarget*, ASTNode*);
    void (*emit_middleware)(struct DSSLTarget*, ASTNode*);

    /* Called once after all items */
    void (*end)(struct DSSLTarget* self);

    /* Optional: emit build/deploy scripts */
    void (*emit_build_script) (struct DSSLTarget*, ASTNode* file);
    void (*emit_deploy_script)(struct DSSLTarget*, ASTNode* file);

    /* Optional: emit metadata */
    void (*emit_metadata)(struct DSSLTarget*, ASTNode* file);

    void* priv;  /* target-private data */
} DSSLTarget;
```

### 7.2 C++ Target

The C++ target generates:
- `<service>_types.h` — all struct/enum/flags definitions
- `<service>_service.h` — RPC/endpoint/event declarations
- `<service>_service.cpp` — skeleton implementations (stubs that throw `NotImplemented`)
- `<service>_errors.h` — error code enums and exception classes
- `build_cpp.sh` — CMake-based build script
- `deploy_
Continuing from section 7.2 C++ Target:

- `deploy_<service>.sh` — deployment script for `.da` archive creation


#### 7.2.1 Struct Generation

```cpp
/* codegen/cpp_target.c — emit_struct */

static void cpp_emit_struct(DSSLTarget* self, ASTNode* node) {
    CppCtx* ctx = self->priv;
    const char* name = node->struct_.name;

    /* forward declaration in header */
    fprintf(ctx->hdr, "struct %s;\n", name);

    /* full definition */
    fprintf(ctx->hdr, "\nstruct %s {\n", name);

    ASTList* fields = node->struct_.fields;
    for (int i = 0; i < fields->count; i++) {
        ASTNode* field = fields->nodes[i];
        const char* fname = field->field.name;
        char* ctype = dssl_type_to_cpp(field->field.type);

        if (field->doc)
            cpp_emit_doc(ctx->hdr, field->doc, 4);

        fprintf(ctx->hdr, "    %s %s", ctype, fname);

        /* default value for optional fields */
        if (field->field.type->kind == TK_OPTIONAL)
            fprintf(ctx->hdr, " = std::nullopt");
        else if (field->field.type->kind == TK_LIST ||
                 field->field.type->kind == TK_MAP)
            fprintf(ctx->hdr, " = {}");

        fprintf(ctx->hdr, ";\n");
        free(ctype);
    }

    /* JSON serialization helpers */
    fprintf(ctx->hdr,
        "\n"
        "    void to_json(nlohmann::json& j) const;\n"
        "    static %s from_json(const nlohmann::json& j);\n"
        "};\n", name);

    /* emit to_json / from_json in .cpp */
    cpp_emit_struct_json(ctx, node);
}

static void cpp_emit_struct_json(CppCtx* ctx, ASTNode* node) {
    const char* name = node->struct_.name;
    ASTList* fields = node->struct_.fields;

    /* to_json */
    fprintf(ctx->src,
        "void %s::to_json(nlohmann::json& j) const {\n"
        "    j = nlohmann::json{\n", name);

    for (int i = 0; i < fields->count; i++) {
        ASTNode* field = fields->nodes[i];
        const char* fname = field->field.name;
        DSSLType* ftype = field->field.type;

        if (ftype->kind == TK_OPTIONAL) {
            fprintf(ctx->src,
                "        {\"%s\", %s.has_value() ? "
                "nlohmann::json(%s.value()) : nlohmann::json(nullptr)},\n",
                fname, fname, fname);
        } else if (is_int64_type(ftype)) {
            /* int64/uint64 → JSON string */
            fprintf(ctx->src,
                "        {\"%s\", std::to_string(%s)},\n",
                fname, fname);
        } else {
            fprintf(ctx->src,
                "        {\"%s\", %s},\n", fname, fname);
        }
    }

    fprintf(ctx->src, "    };\n}\n\n");

    /* from_json */
    fprintf(ctx->src,
        "%s %s::from_json(const nlohmann::json& j) {\n"
        "    %s obj;\n", name, name, name);

    for (int i = 0; i < fields->count; i++) {
        ASTNode* field = fields->nodes[i];
        const char* fname = field->field.name;
        DSSLType* ftype = field->field.type;

        if (ftype->kind == TK_OPTIONAL) {
            fprintf(ctx->src,
                "    if (j.contains(\"%s\") && !j[\"%s\"].is_null())\n"
                "        obj.%s = j[\"%s\"].get<%s>();\n",
                fname, fname, fname, fname,
                dssl_type_to_cpp(ftype->inner));
        } else if (is_int64_type(ftype)) {
            const char* ctype = dssl_type_to_cpp(ftype);
            fprintf(ctx->src,
                "    obj.%s = static_cast<%s>("
                "std::stoll(j[\"%s\"].get<std::string>()));\n",
                fname, ctype, fname);
            free((void*)ctype);
        } else {
            const char* ctype = dssl_type_to_cpp(ftype);
            fprintf(ctx->src,
                "    obj.%s = j[\"%s\"].get<%s>();\n",
                fname, fname, ctype);
            free((void*)ctype);
        }
    }

    fprintf(ctx->src, "    return obj;\n}\n\n");
}
```

#### 7.2.2 Enum Generation

```cpp
static void cpp_emit_enum(DSSLTarget* self, ASTNode* node) {
    CppCtx* ctx = self->priv;
    const char* name = node->enum_.name;

    if (node->doc)
        cpp_emit_doc(ctx->hdr, node->doc, 0);

    fprintf(ctx->hdr, "enum class %s {\n", name);

    ASTList* variants = node->enum_.variants;
    for (int i = 0; i < variants->count; i++) {
        ASTNode* v = variants->nodes[i];
        if (v->doc)
            cpp_emit_doc(ctx->hdr, v->doc, 4);
        if (v->enum_variant.has_value)
            fprintf(ctx->hdr, "    %s = %d,\n",
                v->enum_variant.name, v->enum_variant.value);
        else
            fprintf(ctx->hdr, "    %s,\n", v->enum_variant.name);
    }

    fprintf(ctx->hdr, "};\n\n");

    /* string conversion helpers */
    fprintf(ctx->hdr,
        "const char* %s_to_string(%s v);\n"
        "%s %s_from_string(const char* s);\n\n",
        name, name, name, name);

    /* emit implementations */
    fprintf(ctx->src,
        "const char* %s_to_string(%s v) {\n"
        "    switch (v) {\n", name, name);
    for (int i = 0; i < variants->count; i++) {
        ASTNode* v = variants->nodes[i];
        fprintf(ctx->src,
            "    case %s::%s: return \"%s\";\n",
            name, v->enum_variant.name, v->enum_variant.name);
    }
    fprintf(ctx->src,
        "    default: return \"<unknown>\";\n"
        "    }\n}\n\n");

    fprintf(ctx->src,
        "%s %s_from_string(const char* s) {\n", name, name);
    for (int i = 0; i < variants->count; i++) {
        ASTNode* v = variants->nodes[i];
        fprintf(ctx->src,
            "    if (strcmp(s, \"%s\") == 0) return %s::%s;\n",
            v->enum_variant.name, name, v->enum_variant.name);
    }
    fprintf(ctx->src,
        "    throw std::invalid_argument("
        "std::string(\"Unknown %s: \") + s);\n"
        "}\n\n", name);
}
```

#### 7.2.3 Flags Generation

```cpp
static void cpp_emit_flags(DSSLTarget* self, ASTNode* node) {
    CppCtx* ctx = self->priv;
    const char* name = node->flags_.name;

    if (node->doc)
        cpp_emit_doc(ctx->hdr, node->doc, 0);

    /* underlying type is uint32_t unless any value >= 2^32 */
    const char* utype = flags_needs_64bit(node) ? "uint64_t" : "uint32_t";

    fprintf(ctx->hdr, "enum class %s : %s {\n", name, utype);
    fprintf(ctx->hdr, "    None = 0,\n");

    ASTList* variants = node->flags_.variants;
    int bit = 0;
    for (int i = 0; i < variants->count; i++) {
        ASTNode* v = variants->nodes[i];
        if (v->doc)
            cpp_emit_doc(ctx->hdr, v->doc, 4);
        int assigned_bit = v->flags_variant.has_value
            ? v->flags_variant.value : bit;
        fprintf(ctx->hdr, "    %s = 1 << %d,\n",
            v->flags_variant.name, assigned_bit);
        bit = assigned_bit + 1;
    }

    fprintf(ctx->hdr, "};\n\n");

    /* bitwise operator overloads */
    fprintf(ctx->hdr,
        "inline %s operator|(%s a, %s b) "
        "{ return static_cast<%s>("
        "static_cast<%s>(a) | static_cast<%s>(b)); }\n"
        "inline %s operator&(%s a, %s b) "
        "{ return static_cast<%s>("
        "static_cast<%s>(a) & static_cast<%s>(b)); }\n"
        "inline %s operator~(%s a) "
        "{ return static_cast<%s>(~static_cast<%s>(a)); }\n"
        "inline bool has_flag(%s set, %s flag) "
        "{ return (static_cast<%s>(set) & static_cast<%s>(flag)) != 0; }\n\n",
        name, name, name, name, utype, utype,
        name, name, name, name, utype, utype,
        name, name, name, utype,
        name, name, utype, utype);
}
```

#### 7.2.4 RPC Skeleton Generation

```cpp
static void cpp_emit_rpc(DSSLTarget* self, ASTNode* node) {
    CppCtx* ctx = self->priv;
    const char* rname = node->rpc.name;

    /* build parameter string */
    char params_buf[4096] = {0};
    ASTList* params = node->rpc.params;
    for (int i = 0; i < params->count; i++) {
        ASTNode* p = params->nodes[i];
        char* ctype = dssl_type_to_cpp(p->param.type);
        /* pass structs/strings by const ref */
        bool by_ref = cpp_should_pass_by_ref(p->param.type);
        char tmp[256];
        snprintf(tmp, sizeof(tmp), "%s%s %s%s",
            by_ref ? "const " : "",
            ctype,
            by_ref ? "& " : " ",
            p->param.name);
        if (i > 0) strncat(params_buf, ", ", sizeof(params_buf)-1);
        strncat(params_buf, tmp, sizeof(params_buf)-1);
        free(ctype);
    }

    char* ret_type = node->rpc.returns
        ? dssl_type_to_cpp(node->rpc.returns)
        : strdup("void");

    /* check for @stream modifier */
    bool is_stream = rpc_has_modifier(node, RMOD_STREAM);
    if (is_stream) {
        /* streaming RPCs return via callback */
        char* actual_ret = ret_type;
        ret_type = strdup("void");
        char stream_param[256];
        snprintf(stream_param, sizeof(stream_param),
            "std::function<void(%s)> on_item, "
            "std::function<void()> on_done",
            actual_ret);
        if (strlen(params_buf) > 0)
            strncat(params_buf, ", ", sizeof(params_buf)-1);
        strncat(params_buf, stream_param, sizeof(params_buf)-1);
        free(actual_ret);
    }

    if (node->doc)
        cpp_emit_doc(ctx->hdr, node->doc, 4);

    fprintf(ctx->hdr, "    virtual %s %s(%s);\n",
        ret_type, rname, params_buf);

    /* skeleton in .cpp */
    fprintf(ctx->src,
        "%s %s::%s(%s) {\n"
        "    throw DaffyNotImplemented(\"%s\");\n"
        "}\n\n",
        ret_type, ctx->service_name, rname, params_buf, rname);

    free(ret_type);
}
```

#### 7.2.5 Error Set Generation

```cpp
static void cpp_emit_errors(DSSLTarget* self, ASTNode* node) {
    CppCtx* ctx = self->priv;
    const char* name = node->errors.name;

    /* error code enum */
    fprintf(ctx->hdr, "enum class %sCode : int32_t {\n", name);
    ASTList* entries = node->errors.entries;
    for (int i = 0; i < entries->count; i++) {
        ASTNode* e = entries->nodes[i];
        if (e->doc)
            cpp_emit_doc(ctx->hdr, e->doc, 4);
        fprintf(ctx->hdr, "    %s = %d,\n",
            e->error_entry.name, e->error_entry.code);
    }
    fprintf(ctx->hdr, "};\n\n");

    /* exception class */
    fprintf(ctx->hdr,
        "class %sException : public DaffyServiceError {\n"
        "public:\n"
        "    %sCode code;\n"
        "    std::string message;\n"
        "    explicit %sException(%sCode code, std::string msg = \"\")\n"
        "        : DaffyServiceError(std::move(msg)), code(code) {}\n"
        "    int32_t error_code() const noexcept override {\n"
        "        return static_cast<int32_t>(code);\n"
        "    }\n"
        "};\n\n",
        name, name, name, name);
}
```

#### 7.2.6 Build Script Generation

```c
static void cpp_emit_build_script(DSSLTarget* self, ASTNode* file) {
    CppCtx* ctx = self->priv;
    const char* svc = ctx->service_name;

    FILE* f = open_output(ctx->out_dir, "build_cpp.sh");
    fprintf(f,
        "#!/usr/bin/env bash\n"
        "set -euo pipefail\n\n"
        "SERVICE=\"%s\"\n"
        "BUILD_DIR=\"build_cpp\"\n\n"
        "cmake -B \"$BUILD_DIR\" -S . \\\n"
        "    -DCMAKE_BUILD_TYPE=Release \\\n"
        "    -DSERVICE_NAME=\"$SERVICE\"\n\n"
        "cmake --build \"$BUILD_DIR\" --parallel\n\n"
        "# Package into Daemon Archive\n"
        "mkdir -p dist\n"
        "tar -czf \"dist/${SERVICE}.da\" \\\n"
        "    -C \"$BUILD_DIR\" \"lib${SERVICE}.so\" \\\n"
        "    -C .. \"${SERVICE}.dssl\" \\\n"
        "    metadata.json\n\n"
        "echo \"Built dist/${SERVICE}.da\"\n",
        svc);
    fclose(f);
    chmod_exec(ctx->out_dir, "build_cpp.sh");

    /* also emit CMakeLists.txt */
    FILE* cmake = open_output(ctx->out_dir, "CMakeLists.txt");
    fprintf(cmake,
        "cmake_minimum_required(VERSION 3.20)\n"
        "project(%s)\n\n"
        "set(CMAKE_CXX_STANDARD 20)\n\n"
        "find_package(nlohmann_json REQUIRED)\n\n"
        "add_library(%s SHARED\n"
        "    %s_service.cpp\n"
        ")\n\n"
        "target_include_directories(%s PUBLIC .)\n"
        "target_link_libraries(%s PRIVATE nlohmann_json::nlohmann_json)\n",
        svc, svc, svc, svc, svc);
    fclose(cmake);
}
```

#### 7.2.7 Deploy Script Generation

```c
static void cpp_emit_deploy_script(DSSLTarget* self, ASTNode* file) {
    CppCtx* ctx = self->priv;
    const char* svc = ctx->service_name;

    FILE* f = open_output(ctx->out_dir, "deploy_cpp.sh");
    fprintf(f,
        "#!/usr/bin/env bash\n"
        "set -euo pipefail\n\n"
        "SERVICE=\"%s\"\n"
        "DA_FILE=\"dist/${SERVICE}.da\"\n"
        "DEPLOY_HOST=\"${DAFFY_DEPLOY_HOST:-localhost}\"\n"
        "DEPLOY_PORT=\"${DAFFY_DEPLOY_PORT:-7700}\"\n\n"
        "if [[ ! -f \"$DA_FILE\" ]]; then\n"
        "    echo \"Error: $DA_FILE not found. Run build_cpp.sh first.\"\n"
        "    exit 1\n"
        "fi\n\n"
        "echo \"Deploying $DA_FILE to $DEPLOY_HOST:$DEPLOY_PORT...\"\n"
        "curl -fsSL \\\n"
        "    -X POST \\\n"
        "    -H 'Content-Type: application/octet-stream' \\\n"
        "    --data-binary @\"$DA_FILE\" \\\n"
        "    \"http://${DEPLOY_HOST}:${DEPLOY_PORT}/api/services/deploy\"\n\n"
        "echo \"Deployed ${SERVICE} successfully.\"\n",
        svc);
    fclose(f);
    chmod_exec(ctx->out_dir, "deploy_cpp.sh");
}
```

---

### 7.3 Lua Binding Target

The Lua target generates a `.lua` file exposing the service as a Lua module, usable from DaffyChat room scripts.

#### 7.3.1 Module Structure

```c
static void lua_begin(DSSLTarget* self, ASTNode* file,
                      const char* out_dir, const char* ns) {
    LuaCtx* ctx = self->priv;
    ctx->out = open_output(out_dir,
        str_concat(file->file.service_name, ".lua"));

    fprintf(ctx->out,
        "-- Auto-generated by dssl-bindgen (Lua target)\n"
        "-- Service: %s\n"
        "-- DO NOT EDIT\n\n"
        "local M = {}\n\n"
        "local _rpc = require('daffy.rpc')\n"
        "local _svc = '%s'\n\n",
        file->file.service_name,
        file->file.service_name);
}

static void lua_end(DSSLTarget* self) {
    LuaCtx* ctx = self->priv;
    fprintf(ctx->out, "return M\n");
    fclose(ctx->out);
}
```

#### 7.3.2 Struct Constructors

```c
static void lua_emit_struct(DSSLTarget* self, ASTNode* node) {
    LuaCtx* ctx = self->priv;
    const char* name = node->struct_.name;
    ASTList* fields = node->struct_.fields;

    fprintf(ctx->out,
        "--- Constructor for %s\n"
        "-- @param fields table\n"
        "-- @return %s\n"
        "function M.%s(fields)\n"
        "    local obj = {}\n",
        name, name, name);

    for (int i = 0; i < fields->count; i++) {
        ASTNode* f = fields->nodes[i];
        bool optional = f->field.type->kind == TK_OPTIONAL;
        if (optional) {
            fprintf(ctx->out,
                "    obj.%s = fields.%s  -- optional\n",
                f->field.name, f->field.name);
        } else {
            fprintf(ctx->out,
                "    assert(fields.%s ~= nil, "
                "'%s.%s is required')\n"
                "    obj.%s = fields.%s\n",
                f->field.name, name, f->field.name,
                f->field.name, f->field.name);
        }
    }

    fprintf(ctx->out,
        "    return obj\n"
        "end\n\n");
}
```

#### 7.3.3 RPC Bindings

```c
static void lua_emit_rpc(DSSLTarget* self, ASTNode* node) {
    LuaCtx* ctx = self->priv;
    const char* rname = node->rpc.name;
    ASTList* params = node->rpc.params;

    /* build param name list */
    char pnames[1024] = {0};
    for (int i = 0; i < params->count; i++) {
        if (i > 0) strncat(pnames, ", ", sizeof(pnames)-1);
        strncat(pnames, params->nodes[i]->param.name, sizeof(pnames)-1);
    }

    bool is_stream = rpc_has_modifier(node, RMOD_STREAM);

    if (node->doc)
        lua_emit_doc(ctx->out, node->doc);

    if (is_stream) {
        fprintf(ctx->out,
            "function M.%s(%s, on_item, on_done)\n"
            "    return _rpc.stream(_svc, '%s', {%s}, on_item, on_done)\n"
            "end\n\n",
            rname, pnames, rname, pnames);
    } else {
        fprintf(ctx->out,
            "function M.%s(%s)\n"
            "    return _rpc.call(_svc, '%s', {%s})\n"
            "end\n\n",
            rname, pnames, rname, pnames);
    }
}
```

#### 7.3.4 Enum Tables

```c
static void lua_emit_enum(DSSLTarget* self, ASTNode* node) {
    LuaCtx* ctx = self->priv;
    const char* name = node->enum_.name;
    ASTList* variants = node->enum_.variants;

    fprintf(ctx->out, "M.%s = {\n", name);
    for (int i = 0; i < variants->count; i++) {
        ASTNode* v = variants->nodes[i];
        if (v->enum_variant.has_value)
            fprintf(ctx->out, "    %s = %d,\n",
                v->enum_variant.name, v->enum_variant.value);
        else
            fprintf(ctx->out, "    %s = %d,\n",
                v->enum_variant.name, i);
    }
    fprintf(ctx->out, "}\n\n");
}
```

---

### 7.4 Go Target

#### 7.4.1 Package Structure

```c
static void go_begin(DSSLTarget* self, ASTNode* file,
                     const char* out_dir, const char* ns) {
    GoCtx* ctx = self->priv;
    const char* svc = file->file.service_name;
    char* pkg = str_to_lower(svc);

    ctx->out = open_output(out_dir, str_concat(pkg, ".go"));

    fprintf(ctx->out,
        "// Code generated by dssl-bindgen (Go target). DO NOT EDIT.\n"
        "// Service: %s\n\n"
        "package %s\n\n"
        "import (\n"
        "    \"context\"\n"
        "    \"encoding/json\"\n"
        "    \"fmt\"\n"
        "    \"time\"\n"
        ")\n\n"
        "var _ = fmt.Sprintf  // suppress unused import\n"
        "var _ = time.Now     // suppress unused import\n"
        "var _ json.Marshaler // suppress unused import\n\n",
        svc, pkg);

    free(pkg);
}
```

#### 7.4.2 Struct Generation

```c
static void go_emit_struct(DSSLTarget* self, ASTNode* node) {
    GoCtx* ctx = self->priv;
    const char* name = node->struct_.name;
    ASTList* fields = node->struct_.fields;

    if (node->doc)
        go_emit_doc(ctx->out, node->doc, name);

    fprintf(ctx->out, "type %s struct {\n", name);

    for (int i = 0; i < fields->count; i++) {
        ASTNode* f = fields->nodes[i];
        char* gotype = dssl_type_to_go(f->field.type);
        char* exported = str_capitalize(f->field.name);
        char* json_tag = f->field.name;  /* keep snake_case in JSON */

        bool optional = f->field.type->kind == TK_OPTIONAL;

        if (f->doc)
            go_emit_doc(ctx->out, f->doc, NULL);

        fprintf(ctx->out,
            "    %s %s `json:\"%s%s\"`\n",
            exported, gotype, json_tag,
            optional ? ",omitempty" : "");

        free(gotype);
        free(exported);
    }

    fprintf(ctx->out, "}\n\n");
}
```

#### 7.4.3 Interface and Client Generation

```c
static void go_emit_service_interface(GoCtx* ctx, ASTNode* file) {
    const char* svc = file->file.service_name;

    fprintf(ctx->out,
        "// %sService defines the service interface.\n"
        "type %sService interface {\n", svc, svc);

    ASTList* items = file->file.items;
    for (int i = 0; i < items->count; i++) {
        ASTNode* item = items->nodes[i];
        if (item->kind == AST_RPC) {
            go_emit_rpc_signature(ctx, item, true);
        }
    }

    fprintf(ctx->out, "}\n\n");

    /* HTTP client implementation */
    fprintf(ctx->out,
        "type %sClient struct {\n"
        "    baseURL    string\n"
        "    httpClient *http.Client\n"
        "}\n\n"
        "func New%sClient(baseURL string) *%sClient {\n"
        "    return &%sClient{\n"
        "        baseURL:    baseURL,\n"
        "        httpClient: &http.Client{Timeout: 30 * time.Second},\n"
        "    }\n"
        "}\n\n",
        svc, svc, svc, svc);
}

static void go_emit_rpc_signature(GoCtx* ctx, ASTNode* node, bool iface) {
    const char* rname = node->rpc.name;
    ASTList* params = node->rpc.params;
    bool is_stream = rpc_has_modifier(node, RMOD_STREAM);

    fprintf(ctx->out, "    %s(ctx context.Context", rname);

    for (int i = 0; i < params->count; i++) {
        ASTNode* p = params->nodes[i];
        char* gotype = dssl_type_to_go(p->param.type);
        char* exported_param = str_capitalize(p->param.name);
        fprintf(ctx->out, ", %s %s", exported_param, gotype);
        free(gotype);
        free(exported_param);
    }

    if (is_stream && node->rpc.returns) {
        char* ret = dssl_type_to_go(node->rpc.returns);
        fprintf(ctx->out, ") (<-chan %s, error)", ret);
        free(ret);
    } else if (node->rpc.returns) {
        char* ret = dssl_type_to_go(node->rpc.returns);
        fprintf(ctx->out, ") (%s, error)", ret);
        free(ret);
    } else {
        fprintf(ctx->out, ") error");
    }

    fprintf(ctx->out, iface ? "\n" : " {\n");
}
```

---

### 7.5 Rust Target

#### 7.5.1 Crate Structure

```c
static void rust_begin(DSSLTarget* self, ASTNode* file,
                       const char* out_dir, const char* ns) {
    RustCtx* ctx = self->priv;
    const char* svc = file->file.service_name;

    ctx->types_out = open_output(out_dir, "types.rs");
    ctx->service_out = open_output(out_dir, "lib.rs");

    fprintf(ctx->types_out,
        "// Auto-generated by dssl-bindgen (Rust target). DO NOT EDIT.\n"
        "use serde::{Deserialize, Serialize};\n"
        "use std::collections::HashMap;\n\n");

    fprintf(ctx->service_out,
        "// Auto-generated by dssl-bindgen (Rust target). DO NOT EDIT.\n"
        "pub mod types;\n"
        "use types::*;\n"
        "use async_trait::async_trait;\n\n");
}
```

#### 7.5.2 Struct Generation

```c
static void rust_emit_struct(DSSLTarget* self, ASTNode* node) {
    RustCtx* ctx = self->priv;
    const char* name = node->struct_.name;
    ASTList* fields = node->struct_.fields;

    if (node->doc)
        rust_emit_doc(ctx->types_out, node->doc);

    fprintf(ctx->types_out,
        "#[derive(Debug, Clone, Serialize, Deserialize)]\n"
        "pub struct %s {\n", name);

    for (int i = 0; i < fields->count; i++) {
        ASTNode* f = fields->nodes[i];
        char* rtype = dssl_type_to_rust(f->field.type);
        char* snake = str_to_snake(f->field.name);

        if (f->doc)
            rust_emit_doc(ctx->types_out, f->doc);

        /* serde rename if original name differs */
        if (strcmp(snake, f->field.name) != 0)
            fprintf(ctx->types_out,
                "    #[serde(rename = \"%s\")]\n", f->field.name);

        /* skip_serializing_if for Option */
        if (f->field.type->kind == TK_OPTIONAL)
            fprintf(ctx->types_out,
                "    #[serde(skip_serializing_if = \"Option::is_none\")]\n");

        /* int64/uint64 need string serialization for JSON wire format */
        if (is_int64_type(f->field.type))
            fprintf(ctx->types_out,
                "    #[serde(with = \"serde_str\")]\n");

        fprintf(ctx->types_out,
            "    pub %s: %s,\n", snake, rtype);

        free(rtype);
        free(snake);
    }

    fprintf(ctx->types_out, "}\n\n");
}
```

#### 7.5.3 Enum Generation

```c
static void rust_emit_enum(DSSLTarget* self, ASTNode* node) {
    RustCtx* ctx = self->priv;
    const char* name = node->enum_.name;
    ASTList* variants = node->enum_.variants;

    if (node->doc)
        rust_emit_doc(ctx->types_out, node->doc);

    fprintf(ctx->types_out,
        "#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]\n"
        "#[serde(rename_all = \"SCREAMING_SNAKE_CASE\")]\n"
        "pub enum %s {\n", name);

    for (int i = 0; i < variants->count; i++) {
        ASTNode* v = variants->nodes[i];
        if (v->doc)
            rust_emit_doc(ctx->types_out, v->doc);
        if (v->enum_variant.has_value)
            fprintf(ctx->types_out,
                "    %s = %d,\n",
                v->enum_variant.name, v->enum_variant.value);
        else
            fprintf(ctx->types_out,
                "    %s,\n", v->enum_variant.name);
    }

    fprintf(ctx->types_out, "}\n\n");
}
```

#### 7.5.4 Flags Generation

```c
static void rust_emit_flags(DSSLTarget* self, ASTNode* node) {
    RustCtx* ctx = self->priv;
    const char* name = node->flags_.name;
    ASTList* variants = node->flags_.variants;

    /* use the bitflags! macro */
    fprintf(ctx->types_out,
        "bitflags::bitflags! {\n"
        "    #[derive(Debug, Clone, Copy, PartialEq, Eq, "
        "Serialize, Deserialize)]\n"
        "    #[serde(transparent)]\n"
        "    pub struct %s: %s {\n",
        name,
        flags_needs_64bit(node) ? "u64" : "u32");

    int bit = 0;
    for (int i = 0; i < variants->count; i++) {
        ASTNode* v = variants->nodes[i];
        int assigned_bit = v->flags_variant.has_value
            ? v->flags_variant.value : bit;
        if (v->doc)
            rust_emit_doc(ctx->types_out, v->doc);
        fprintf(ctx->types_out,
            "        const %s = 1 << %d;\n",
            v->flags_variant.name, assigned_bit);
        bit = assigned_bit + 1;
    }

    fprintf(ctx->types_out, "    }\n}\n\n");
}
```

#### 7.5.5 Async Trait Generation

```c
static void rust_emit_service_trait(RustCtx* ctx, ASTNode* file) {
    const char* svc = file->file.service_name;

    fprintf(ctx->service_out,
        "#[async_trait]\n"
        "pub trait %sService: Send + Sync {\n", svc);

    ASTList* items = file->file.items;
    for (int i = 0; i < items->count; i++) {
        ASTNode* item = items->nodes[i];
        if (item->kind != AST_RPC) continue;

        rust_emit_rpc_signature(ctx->service_out, item, svc, true);
    }

    fprintf(ctx->service_out, "}\n\n");
}

static void rust_emit_rpc_signature(FILE* out, ASTNode* node,
                                    const char* svc, bool is_trait) {
    const char* rname = node->rpc.name;
    char* snake_name = str_to_snake(rname);
    ASTList* params = node->rpc.params;
    bool is_stream = rpc_has_modifier(node, RMOD_STREAM);

    if (node->doc)
        rust_emit_doc(out, node->doc);

    fprintf(out, "    async fn %s(&self", snake_name);

    for (int i = 0; i < params->count; i++) {
        ASTNode* p = params->nodes[i];
        char* rtype = dssl_type_to_rust(p->param.type);
        char* snake_param = str_to_snake(p->param.name);
        fprintf(out, ", %s: %s", snake_param, rtype);
        free(rtype);
        free(snake_param);
    }

    /* determine return type */
    if (is_stream && node->rpc.returns) {
        char* ret = dssl_type_to_rust(node->rpc.returns);
        /* streaming → return a channel receiver */
        fprintf(out,
            ") -> Result<tokio::sync::mpsc::Receiver<%s>, ServiceError>",
            ret);
        free(ret);
    } else if (node->rpc.returns) {
        char* ret = dssl_type_to_rust(node->rpc.returns);
        /* check throws clause */
        if (node->rpc.throws) {
            char* err = dssl_type_to_rust(node->rpc.throws);
            fprintf(out, ") -> Result<%s, %s>", ret, err);
            free(err);
        } else {
            fprintf(out, ") -> Result<%s, ServiceError>", ret);
        }
        free(ret);
    } else {
        fprintf(out, ") -> Result<(), ServiceError>");
    }

    fprintf(out, is_trait ? ";\n" : " {\n");
    free(snake_name);
}
```

#### 7.5.6 HTTP Client Implementation

```c
static void rust_emit_client(RustCtx* ctx, ASTNode* file) {
    const char* svc = file->file.service_name;

    fprintf(ctx->service_out,
        "pub struct %sClient {\n"
        "    base_url: String,\n"
        "    client: reqwest::Client,\n"
        "}\n\n"
        "impl %sClient {\n"
        "    pub fn new(base_url: impl Into<String>) -> Self {\n"
        "        Self {\n"
        "            base_url: base_url.into(),\n"
        "            client: reqwest::Client::new(),\n"
        "        }\n"
        "    }\n"
        "}\n\n",
        svc, svc);

    /* implement the trait for the client */
    fprintf(ctx->service_out,
        "#[async_trait]\n"
        "impl %sService for %sClient {\n", svc, svc);

    ASTList* items = file->file.items;
    for (int i = 0; i < items->count; i++) {
        ASTNode* item = items->nodes[i];
        if (item->kind != AST_RPC) continue;

        rust_emit_rpc_signature(ctx->service_out, item, svc, false);
        rust_emit_rpc_client_body(ctx->service_out, item, svc);
        fprintf(ctx->service_out, "    }\n\n");
    }

    fprintf(ctx->service_out, "}\n\n");
}

static void rust_emit_rpc_client_body(FILE* out, ASTNode* node,
                                      const char* svc) {
    const char* rname = node->rpc.name;
    char* snake_name = str_to_snake(rname);
    ASTList* params = node->rpc.params;
    bool is_stream = rpc_has_modifier(node, RMOD_STREAM);

    /* build request payload struct inline */
    fprintf(out,
        "        #[derive(Serialize)]\n"
        "        struct __Req<'a> {\n");

    for (int i = 0; i < params->count; i++) {
        ASTNode* p = params->nodes[i];
        char* rtype = dssl_type_to_rust(p->param.type);
        char* snake_param = str_to_snake(p->param.name);
        fprintf(out, "            %s: &'a %s,\n", snake_param, rtype);
        free(rtype);
        free(snake_param);
    }

    fprintf(out, "        }\n");

    /* build the param references */
    char req_fields[2048] = {0};
    for (int i = 0; i < params->count; i++) {
        ASTNode* p = params->nodes[i];
        char* snake_param = str_to_snake(p->param.name);
        char tmp[128];
        snprintf(tmp, sizeof(tmp), "%s%s: &%s",
            i > 0 ? ", " : "", snake_param, snake_param);
        strncat(req_fields, tmp, sizeof(req_fields)-1);
        free(snake_param);
    }

    if (is_stream) {
        fprintf(out,
            "        let url = format!(\"{}/rpc/%s\", self.base_url);\n"
            "        let req = __Req { %s };\n"
            "        let (tx, rx) = tokio::sync::mpsc::channel(32);\n"
            "        let client = self.client.clone();\n"
            "        tokio::spawn(async move {\n"
            "            let mut resp = client.post(&url)\n"
            "                .json(&req)\n"
            "                .send().await?;\n"
            "            while let Some(chunk) = resp.chunk().await? {\n"
            "                if let Ok(item) = serde_json::from_slice(&chunk) {\n"
            "                    let _ = tx.send(item).await;\n"
            "                }\n"
            "            }\n"
            "            Ok::<_, reqwest::Error>(())\n"
            "        });\n"
            "        Ok(rx)\n",
            rname, req_fields);
    } else {
        fprintf(out,
            "        let url = format!(\"{}/rpc/%s\", self.base_url);\n"
            "        let req = __Req { %s };\n"
            "        let resp = self.client\n"
            "            .post(&url)\n"
            "            .json(&req)\n"
            "            .send().await\n"
            "            .map_err(ServiceError::Http)?;\n"
            "        if !resp.status().is_success() {\n"
            "            return Err(ServiceError::status(resp.status()));\n"
            "        }\n",
            rname, req_fields);

        if (node->rpc.returns) {
            fprintf(out,
                "        let result = resp.json().await\n"
                "            .map_err(ServiceError::Http)?;\n"
                "        Ok(result)\n");
        } else {
            fprintf(out, "        Ok(())\n");
        }
    }

    free(snake_name);
}
```

#### 7.5.7 Cargo.toml Generation

```c
static void rust_emit_cargo_toml(RustCtx* ctx, ASTNode* file) {
    const char* svc = file->file.service_name;
    char* snake_svc = str_to_snake(svc);

    FILE* f = open_output(ctx->out_dir, "Cargo.toml");
    fprintf(f,
        "[package]\n"
        "name = \"%s\"\n"
        "version = \"0.1.0\"\n"
        "edition = \"2021\"\n\n"
        "[dependencies]\n"
        "serde = { version = \"1\", features = [\"derive\"] }\n"
        "serde_json = \"1\"\n"
        "async-trait = \"0.1\"\n"
        "tokio = { version = \"1\", features = [\"full\"] }\n"
        "reqwest = { version = \"0.11\", features = [\"json\"] }\n"
        "bitflags = \"2\"\n\n"
        "[lib]\n"
        "name = \"%s\"\n"
        "crate-type = [\"cdylib\", \"rlib\"]\n",
        snake_svc, snake_svc);

    fclose(f);
    free(snake_svc);
}
```

---

### 7.6 Python Target

#### 7.6.1 Module Structure

```c
static void python_begin(DSSLTarget* self, ASTNode* file,
                         const char* out_dir, const char* ns) {
    PyCtx* ctx = self->priv;
    const char* svc = file->file.service_name;
    char* snake_svc = str_to_snake(svc);

    ctx->out = open_output(out_dir, str_concat(snake_svc, ".py"));

    fprintf(ctx->out,
        "# Auto-generated by dssl-bindgen (Python target). DO NOT EDIT.\n"
        "# Service: %s\n\n"
        "from __future__ import annotations\n"
        "from dataclasses import dataclass, field, asdict\n"
        "from typing import Optional, List, Dict, Iterator, Any\n"
        "from enum import IntEnum, IntFlag\n"
        "import json\n"
        "import requests\n\n",
        svc);

    free(snake_svc);
}
```

#### 7.6.2 Dataclass Generation

```c
static void python_emit_struct(DSSLTarget* self, ASTNode* node) {
    PyCtx* ctx = self->priv;
    const char* name = node->struct_.name;
    ASTList* fields = node->struct_.fields;

    if (node->doc)
        python_emit_doc(ctx->out, node->doc, 0);

    fprintf(ctx->out, "@dataclass\nclass %s:\n", name);

    /* required fields first, then optional */
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < fields->count; i++) {
            ASTNode* f = fields->nodes[i];
            bool optional = f->field.type->kind == TK_OPTIONAL;
            if ((pass == 0) == optional) continue;

            char* pytype = dssl_type_to_python(f->field.type);
            if (f->doc)
                python_emit_doc(ctx->out, f->doc, 4);

            if (optional) {
                fprintf(ctx->out,
                    "    %s: %s = None\n",
                    f->field.name, pytype);
            } else {
                fprintf(ctx->out,
                    "    %s: %s\n",
                    f->field.name, pytype);
            }
            free(pytype);
        }
    }

    /* to_dict / from_dict for wire format */
    fprintf(ctx->out,
        "\n"
        "    def to_dict(self) -> dict:\n"
        "        d = asdict(self)\n");

    /* int64 fields need string conversion */
    for (int i = 0; i < fields->count; i++) {
        ASTNode* f = fields->nodes[i];
        if (is_int64_type(f->field.type)) {
            fprintf(ctx->out,
                "        if d.get('%s') is not None:\n"
                "            d['%s'] = str(d['%s'])\n",
                f->field.name, f->field.name, f->field.name);
        }
    }

    fprintf(ctx->out,
        "        return {k: v for k, v in d.items() if v is not None}\n\n"
        "    @classmethod\n"
        "    def from_dict(cls, d: dict) -> '%s':\n"
        "        return cls(\n", name);

    for (int i = 0; i < fields->count; i++) {
        ASTNode* f = fields->nodes[i];
        char* pytype = dssl_type_to_python(f->field.type);
        bool optional = f->field.type->kind == TK_OPTIONAL;

        if (is_int64_type(f->field.type)) {
            fprintf(ctx->out,
                "            %s=int(d['%s']) if '%s' in d else None,\n",
                f->field.name, f->field.name, f->field.name);
        } else if (optional) {
            fprintf(ctx->out,
                "            %s=d.get('%s'),\n",
                f->field.name, f->field.name);
        } else {
            fprintf(ctx->out,
                "            %s=d['%s'],\n",
                f->field.name, f->field.name);
        }
        free(pytype);
    }

    fprintf(ctx->out, "        )\n\n");
}
```

#### 7.6.3 Client Class Generation

```c
static void python_emit_client(PyCtx* ctx, ASTNode* file) {
    const char* svc = file->file.service_name;

    fprintf(ctx->out,
        "class %sClient:\n"
        "    def __init__(self, base_url: str) -> None:\n"
        "        self._base_url = base_url.rstrip('/')\n"
        "        self._session = requests.Session()\n\n",
        svc);

    ASTList* items = file->file.items;
    for (int i = 0; i < items->count; i++) {
        ASTNode* item = items->nodes[i];
        if (item->kind != AST_RPC) continue;
        python_emit_rpc_method(ctx, item);
    }
}

static void python_emit_rpc_method(PyCtx* ctx, ASTNode* node) {
    const char* rname = node->rpc.name;
    char* snake_name = str_to_snake(rname);
    ASTList* params = node->rpc.params;
    bool is_stream = rpc_has_modifier(node, RMOD_STREAM);

    if (node->doc)
        python_emit_doc(ctx->out, node->doc, 4);

    /* method signature */
    fprintf(ctx->out, "    def %s(self", snake_name);
    for (int i = 0; i < params->count; i++) {
        ASTNode* p = params->nodes[i];
        char* pytype = dssl_type_to_python(p->param.type);
        char* snake_param = str_to_snake(p->param.name);
        fprintf(ctx->out, ", %s: %s", snake_param, pytype);
        free(pytype);
        free(snake_param);
    }

    if (is_stream && node->rpc.returns) {
        char* ret = dssl_type_to_python(node->rpc.returns);
        fprintf(ctx->out, ") -> Iterator[%s]:\n", ret);
        free(ret);
    } else if (node->rpc.returns) {
        char* ret = dssl_type_to_python(node->rpc.returns);
        fprintf(ctx->out, ") -> %s:\n", ret);
        free(ret);
    } else {
        fprintf(ctx->out, ") -> None:\n");
    }

    /* body */
    fprintf(ctx->out,
        "        payload = {\n");
    for (int i = 0; i < params->count; i++) {
        ASTNode* p = params->nodes[i];
        char* snake_param = str_to_snake(p->param.name);
        /* structs need .to_dict() */
        bool is_struct = p->param.type->kind == TK_NAMED &&
                         symbol_is_struct(p->param.type->name);
        fprintf(ctx->out,
            "            '%s': %s%s%s,\n",
            p->param.name,
            snake_param,
            is_struct ? ".to_dict()" : "",
            "");
        free(snake_param);
    }
    fprintf(ctx->out, "        }\n");

    if (is_stream) {
        fprintf(ctx->out,
            "        resp = self._session.post(\n"
            "            f'{self._base_url}/rpc/%s',\n"
            "            json=payload,\n"
            "            stream=True,\n"
            "        )\n"
            "        resp.raise_for_status()\n"
            "        for line in resp.iter_lines():\n"
            "            if line:\n"
            "                yield %s.from_dict(json.loads(line))\n\n",
            rname,
            node->rpc.returns
                ? node->rpc.returns->name
                : "dict");
    } else {
        fprintf(ctx->out,
            "        resp = self._session.post(\n"
            "            f'{self._base_url}/rpc/%s',\n"
            "            json=payload,\n"
            "        )\n"
            "        resp.raise_for_status()\n",
            rname);

        if (node->rpc.returns) {
            bool ret_is_struct =
                node->rpc.returns->kind == TK_NAMED &&
                symbol_is_struct(node->rpc.returns->name);
            if (ret_is_struct) {
                fprintf(ctx->out,
                    "        return %s.from_dict(resp.json())\n\n",
                    node->rpc.returns->name);
            } else {
                fprintf(ctx->out,
                    "        return resp.json()\n\n");
            }
        } else {
            fprintf(ctx->out, "\n");
        }
    }

    free(snake_name);
}
```

---

### 7.7 Ruby Target

#### 7.7.1 Module Structure

```c
static void ruby_begin(DSSLTarget* self, ASTNode* file,
                       const char* out_dir, const char* ns) {
    RubyCtx* ctx = self->priv;
    const char* svc = file->file.service_name;

    ctx->out = open_output(out_dir,
        str_concat(str_to_snake(svc), ".rb"));

    fprintf(ctx->out,
        "# frozen_string_literal: true\n"
        "# Auto-generated by dssl-bindgen (Ruby target). DO NOT EDIT.\n"
        "# Service: %s\n\n"
        "require 'json'\n"
        "require 'net/http'\n"
        "require 'uri'\n\n"
        "module %s\n",
        svc, svc);
}

static void ruby_end(DSSLTarget* self) {
    RubyCtx* ctx = self->priv;
    fprintf(ctx->out, "end\n");
    fclose(ctx->out);
}
```

#### 7.7.2 Struct Generation

```c
static void ruby_emit_struct(DSSLTarget* self, ASTNode* node) {
    RubyCtx* ctx = self->priv;
    const char* name = node->struct_.name;
    ASTList* fields = node->struct_.fields;

    if (node->doc)
        ruby_emit_doc(ctx->out, node->doc, 2);

    fprintf(ctx->out, "  class %s\n", name);

    /* attr_accessor for all fields */
    fprintf(ctx->out, "    attr_accessor");
    for (int i = 0; i < fields->count; i++) {
        char* snake = str_to_snake(fields->nodes[i]->field.name);
        fprintf(ctx->out, "%s :%s",
            i == 0 ? "" : ",", snake);
        free(snake);
    }
    fprintf(ctx->out, "\n\n");

    /* initialize */
    fprintf(ctx->out, "    def initialize(");
    for (int i = 0; i < fields->count; i++) {
        ASTNode* f = fields->nodes[i];
        char* snake = str_to_snake(f->field.name);
        bool optional = f->field.type->kind == TK_OPTIONAL;
        fprintf(ctx->out, "%s%s%s",
            i == 0 ? "" : ", ",
            snake,
            optional ? ": nil" : ":");
        free(snake);
    }
    fprintf(ctx->out, ")\n");

    for (int i = 0; i < fields->count; i++) {
        ASTNode* f = fields->nodes[i];
        char* snake = str_to_snake(f->field.name);
        fprintf(ctx->out,
            "      @%s = %s\n", snake, snake);
        free(snake);
    }
    fprintf(ctx->out, "    end\n\n");

    /* to_h */
    fprintf(ctx->out,
        "    def to_h\n"
        "      h = {}\n");
    for (int i = 0; i < fields->count; i++) {
        ASTNode* f = fields->nodes[i];
        char* snake = str_to_snake(f->field.name);
        bool optional = f->field.type->kind == TK_OPTIONAL;
        bool is_struct = f->field.type->kind == TK_NAMED &&
                         symbol_is_struct(f->field.type->name);
        bool is_int64 = is_int64_type(f->field.type);

        if (optional) {
            fprintf(ctx->out,
                "      h['%s'] = %s%s unless @%s.nil?\n",
                f->field.name,
                is_struct ? "@" : "@",
                is_struct
                    ? str_concat(snake, ".to_h")
                    : is_int64
                        ? str_concat(snake, ".to_s")
                        : snake,
                snake);
        } else {
            fprintf(ctx->out,
                "      h['%s'] = %s\n",
                f->field.name,
                is_struct
                    ? str_concat("@", str_concat(snake, ".to_h"))
                    : is_int64
                        ? str_concat("@", str_concat(snake, ".to_s"))
                        : str_concat("@", snake));
        }
        free(snake);
    }
    fprintf(ctx->out,
        "      h\n"
        "    end\n\n");

    /* self.from_h */
    fprintf(ctx->out,
        "    def self.from_h(h)\n"
        "      new(\n");
    for (int i = 0; i < fields->count; i++) {
        ASTNode* f = fields->nodes[i];
        char* snake = str_to_snake(f->field.name);
        bool optional = f->field.type->kind == TK_OPTIONAL;
        bool is_int64 = is_int64_type(f->field.type);
        bool is_struct = f->field.type->kind == TK_NAMED &&
                         symbol_is_struct(f->field.type->name);

        fprintf(ctx->out, "        %s: ", snake);

        if (is_struct) {
            if (optional)
                fprintf(ctx->out,
                    "h['%s'] ? %s.from_h(h['%s']) : nil",
                    f->field.name,
                    f->field.type->name,
                    f->field.name);
            else
                fprintf(ctx->out,
                    "%s.from_h(h['%s'])",
                    f->field.type->name, f->field.name);
        } else if (is_int64) {
            fprintf(ctx->out,
                "h['%s']&.to_i", f->field.name);
        } else {
            fprintf(ctx->out,
                "h['%s']", f->field.name);
        }

        fprintf(ctx->out, "%s\n",
            i < fields->count - 1 ? "," : "");
        free(snake);
    }
    fprintf(ctx->out,
        "      )\n"
        "    end\n"
        "  end\n\n");
}
```

#### 7.7.3 Client Class

```c
static void ruby_emit_client(RubyCtx* ctx, ASTNode* file) {
    const char* svc = file->file.service_name;

    fprintf(ctx->out,
        "  class Client\n"
        "    def initialize(base_url)\n"
        "      @base_url = base_url.chomp('/')\n"
        "    end\n\n");

    ASTList* items = file->file.items;
    for (int i = 0; i < items->count; i++) {
        ASTNode* item = items->nodes[i];
        if (item->kind != AST_RPC) continue;
        ruby_emit_rpc_method(ctx, item);
    }

    fprintf(ctx->out,
        "    private\n\n"
        "    def post_rpc(method, payload)\n"
        "      uri = URI(\"#{@base_url}/rpc/#{method}\")\n"
        "      http = Net::HTTP.new(uri.host, uri.port)\n"
        "      req = Net::HTTP::Post.new(uri, "
        "'Content-Type' => 'application/json')\n"
        "      req.body = payload.to_json\n"
        "      resp = http.request(req)\n"
        "      raise \"HTTP #{resp.code}\" unless resp.is_a?("
        "Net::HTTPSuccess)\n"
        "      JSON.parse(resp.body)\n"
        "    end\n"
        "  end\n\n");
}

static void ruby_emit_rpc_method(RubyCtx* ctx, ASTNode* node) {
    const char* rname = node->rpc.name;
    char* snake_name = str_to_snake(rname);
    ASTList* params = node->rpc.params;

    if (node->doc)
        ruby_emit_doc(ctx->out, node->doc, 4);

    fprintf(ctx->out, "    def %s(", snake_name);
    for (int i = 0; i < params->count; i++) {
        ASTNode* p = params->nodes[i];
        char* snake_param = str_to_snake(p->param.name);
        fprintf(ctx->out, "%s%s", i == 0 ? "" : ", ", snake_param);
        free(snake_param);
    }
    fprintf(ctx->out, ")\n");

    fprintf(ctx->out, "      payload = {\n");
    for (int i = 0; i < params->count; i++) {
        ASTNode* p = params->nodes[i];
        char* snake_param = str_to_
```
