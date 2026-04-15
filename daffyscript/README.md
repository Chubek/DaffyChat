# Daffyscript Language Specification
### Implementation Reference for AI Agents
**Version 0.1.0-draft**

---

## Table of Contents

1. Overview & Architecture
2. Source Encoding & File Types
3. Lexical Analysis (Flex)
4. Grammar (Bison)
5. Type System
6. Semantic Analysis
7. IR & Code Generation (Binaryen)
8. Standard Library Namespaces
9. File-Type-Specific Semantics
10. Error Handling & Diagnostics
11. Appendix: Token Reference

---

## 1. Overview & Architecture

### 1.1 Purpose

Daffyscript is a statically-typed, compiled-to-WASM language exclusively serving the DaffyChat ecosystem. It has three file types — **modules** (`.dfy`), **programs** (`.dfyp`), and **recipes** (`.dfyr`) — each with a distinct syntactic personality appropriate to its role, but sharing a common lexical and grammatical foundation.

### 1.2 Compilation Pipeline

Source File (.dfy / .dfyp / .dfyr)
        │
        ▼
  ┌─────────────┐
  │  Flex Lexer │  → Token stream
  └─────────────┘
        │
        ▼
  ┌──────────────┐
  │ Bison Parser │  → Concrete Syntax Tree (CST)
  └──────────────┘
        │
        ▼
  ┌─────────────────┐
  │  AST Builder    │  → Abstract Syntax Tree
  └─────────────────┘
        │
        ▼
  ┌──────────────────┐
  │ Semantic Analyzer│  → Typed AST + Symbol Table
  └──────────────────┘
        │
        ▼
  ┌─────────────────┐
  │   IR Lowering   │  → Binaryen IR (BIR)
  └─────────────────┘
        │
        ▼
  ┌──────────────────┐
  │ Binaryen Backend │  → .wasm binary
  └──────────────────┘
        │
        ▼
  Output: module.wasm / program.wasm / recipe.wasm


### 1.3 Compiler Entry Point

The compiler binary is `daffyc`. Its invocation signature:

daffyc [options] <source-file>

Options:
  --target <module|program|recipe>   Override file-type detection
  --emit-ast                         Dump AST as JSON to stdout
  --emit-bir                         Dump Binaryen IR text format
  --opt <0|1|2|3>                    Optimization level (default: 1)
  --out <file>                       Output path (default: <stem>.wasm)
  --validate                         Validate without emitting
  --no-stdlib                        Suppress standard library linking


File type is inferred from the extension unless `--target` overrides it. The compiler enforces that the header declaration matches the inferred type and emits a fatal diagnostic if they conflict.

### 1.4 Implementation File Layout

Implementors should organize the compiler source as follows:

daffyc/
├── lexer/
│   ├── daffyscript.l        -- Flex source
│   └── token.h              -- Token enum & value union
├── parser/
│   ├── daffyscript.y        -- Bison source
│   └── cst.h                -- CST node definitions
├── ast/
│   ├── ast.h                -- AST node hierarchy (C++ classes)
│   ├── ast_builder.h/.cpp   -- CST → AST transformation
│   └── ast_printer.h/.cpp   -- JSON dump for --emit-ast
├── sema/
│   ├── type_system.h/.cpp   -- Type representation & unification
│   ├── symbol_table.h/.cpp  -- Scoped symbol table
│   └── sema.h/.cpp          -- Semantic analysis passes
├── codegen/
│   ├── bir_lowering.h/.cpp  -- AST → Binaryen IR
│   └── wasm_emitter.h/.cpp  -- Binaryen → .wasm
├── stdlib/
│   ├── ldc_bindings.h       -- ldc.* declarations
│   ├── dfc_bindings.h       -- dfc.* declarations
│   └── recipe_atoms.h       -- Recipe-scoped atoms
├── diag/
│   ├── diagnostic.h/.cpp    -- Error/warning infrastructure
│   └── source_map.h/.cpp    -- Source location tracking
└── main.cpp


---

## 2. Source Encoding & File Types

### 2.1 Encoding

All Daffyscript source files **must** be UTF-8 encoded. The lexer operates on bytes. Non-ASCII characters are permitted only inside string literals and comments. Any non-ASCII byte outside these contexts is a lexical error.

### 2.2 File Types & Header Requirements

Each file must begin (after optional comments and whitespace) with a mandatory header declaration. The header keyword determines which grammar sub-rules and semantic restrictions apply. A file whose extension and header declaration disagree produces a fatal error `E001`.

| Extension | Header Keyword | Role |
|-----------|---------------|------|
| `.dfy`    | `module`      | WASM frontend plugin / reusable library |
| `.dfyp`   | `program`     | Event-driven bot / automation logic |
| `.dfyr`   | `recipe`      | Room provisioning manifest, comes in an archive |

### 2.3 Line Endings

Both `\n` (LF) and `\r\n` (CRLF) are accepted. The lexer normalizes all line endings to `\n` before tokenization. This normalization happens in a pre-scan pass prior to Flex processing, or equivalently as a Flex rule that maps `\r\n` to `\n`.

---

## 3. Lexical Analysis (Flex)

### 3.1 Flex File Structure

The Flex source file is `lexer/daffyscript.l`. It uses the following header:

```c
%{
#include "token.h"
#include "parser/daffyscript.tab.h"
#include "diag/diagnostic.h"
#include <string.h>
#include <stdlib.h>

/* Source location tracking */
int yyline = 1;
int yycol  = 1;

#define YY_USER_ACTION \
    yylloc.first_line   = yyline;          \
    yylloc.first_column = yycol;           \
    for (int i = 0; yytext[i]; i++) {      \
        if (yytext[i] == '\n') {           \
            yyline++; yycol = 1;           \
        } else {                           \
            yycol++;                       \
        }                                  \
    }                                      \
    yylloc.last_line   = yyline;           \
    yylloc.last_column = yycol;
%}

%option noyywrap
%option yylineno
%option reentrant
%option bison-bridge
%option bison-locations
```

The lexer uses **reentrant** mode (`%option reentrant`) so multiple files can be lexed concurrently during recipe parsing. `bison-bridge` enables passing `yylval` and `yylloc` through the reentrant interface.

### 3.2 Lexer States

%s INITIAL
%x BLOCK_COMMENT
%x STRING_LIT
%x RAW_BYTES


- `INITIAL`: Normal tokenization.
- `BLOCK_COMMENT`: Inside `--[[ ... ]]`.
- `STRING_LIT`: Inside a `"..."` string, handling escape sequences.
- `RAW_BYTES`: Inside a `b"..."` byte literal.

### 3.3 Whitespace & Comments

```flex
/* Whitespace — consumed, not emitted */
[ \t\r]+                { /* skip */ }
\n                      { /* skip, location tracking handled by YY_USER_ACTION */ }

/* Single-line comment */
"--"[^\n]*              { /* skip */ }

/* Block comment open */
"--[["                  { BEGIN(BLOCK_COMMENT); }
<BLOCK_COMMENT>"]]"     { BEGIN(INITIAL); }
<BLOCK_COMMENT>\n       { /* skip */ }
<BLOCK_COMMENT>.        { /* skip */ }
<BLOCK_COMMENT><<EOF>>  {
    diag_error(yyline, yycol, "E010", "Unterminated block comment");
    yyterminate();
}
```

### 3.4 Keywords

All keywords are **reserved** and may not be used as identifiers. The lexer matches them before the general identifier rule.

#### 3.4.1 Universal Keywords (all file types)

```flex
"let"         { return KW_LET; }
"mutable"     { return KW_MUTABLE; }
"fn"          { return KW_FN; }
"return"      { return KW_RETURN; }
"if"          { return KW_IF; }
"else"        { return KW_ELSE; }
"for"         { return KW_FOR; }
"while"       { return KW_WHILE; }
"match"       { return KW_MATCH; }
"in"          { return KW_IN; }
"struct"      { return KW_STRUCT; }
"enum"        { return KW_ENUM; }
"pub"         { return KW_PUB; }
"import"      { return KW_IMPORT; }
"true"        { return KW_TRUE; }
"false"       { return KW_FALSE; }
"none"        { return KW_NONE; }
"and"         { return KW_AND; }
"or"          { return KW_OR; }
"not"         { return KW_NOT; }
"try"         { return KW_TRY; }
"catch"       { return KW_CATCH; }
"raise"       { return KW_RAISE; }
"as"          { return KW_AS; }
"type"        { return KW_TYPE; }
```

#### 3.4.2 Module-Scoped Keywords (`.dfy`)

```flex
"module"      { return KW_MODULE; }
"emit"        { return KW_EMIT; }
"expect"      { return KW_EXPECT; }
"hook"        { return KW_HOOK; }
"exports"     { return KW_EXPORTS; }
```

#### 3.4.3 Program-Scoped Keywords (`.dfyp`)

```flex
"program"     { return KW_PROGRAM; }
"command"     { return KW_COMMAND; }
"intercept"   { return KW_INTERCEPT; }
"message"     { return KW_MESSAGE; }
"every"       { return KW_EVERY; }
"at"          { return KW_AT; }
"timezone"    { return KW_TIMEZONE; }
"room"        { return KW_ROOM; }
```

#### 3.4.4 Recipe-Scoped Keywords (`.dfyr`)

```flex
"recipe"      { return KW_RECIPE; }
"service"     { return KW_SERVICE; }
"version"     { return KW_VERSION; }
"author"      { return KW_AUTHOR; }
"description" { return KW_DESCRIPTION; }
"config"      { return KW_CONFIG; }
"autostart"   { return KW_AUTOSTART; }
"roles"       { return KW_ROLES; }
"role"        { return KW_ROLE; }
"can"         { return KW_CAN; }
"cannot"      { return KW_CANNOT; }
"default_role"{ return KW_DEFAULT_ROLE; }
"webhooks"    { return KW_WEBHOOKS; }
"post"        { return KW_POST; }
"to"          { return KW_TO; }
"headers"     { return KW_HEADERS; }
"when"        { return KW_WHEN; }
"on"          { return KW_ON; }
"init"        { return KW_INIT; }
"event"       { return KW_EVENT; }
"enabled"     { return KW_ATOM_ENABLED; }
"disabled"    { return KW_ATOM_DISABLED; }
"strict"      { return KW_ATOM_STRICT; }
"moderate"    { return KW_ATOM_MODERATE; }
"relaxed"     { return KW_ATOM_RELAXED; }
```

> **Implementation note:** All keyword tokens — regardless of file type — are always lexed. The parser grammar rules for module/program/recipe constructs are disjoint, so file-type scoping is enforced at the **grammar level**, not the lexer level. This avoids lexer state complexity and allows better error messages.

### 3.5 Identifiers

```flex
[a-zA-Z_][a-zA-Z0-9_]*   {
    yylval->str = strdup(yytext);
    return IDENT;
}
```

Identifiers begin with a letter or underscore, followed by letters, digits, or underscores. Maximum length is 255 bytes; the lexer truncates with warning `W001` if exceeded.

### 3.6 Qualified Identifiers

Qualified identifiers (e.g., `ldc.event`, `dfc.bridge`) are **not** lexed as single tokens. They are parsed as `IDENT DOT IDENT` chains and resolved during semantic analysis. The dot operator `.` is context-sensitive: when separating two identifiers it is a namespace accessor; when following a literal or expression it is a method call.

### 3.7 Literals

#### 3.7.1 Integer Literals

```flex
[0-9]+                    {
    yylval->int_val = strtoll(yytext, NULL, 10);
    return LIT_INT;
}
0x[0-9A-Fa-f]+            {
    yylval->int_val = strtoll(yytext + 2, NULL, 16);
    return LIT_INT;
}
0b[01]+                   {
    yylval->int_val = strtoll(yytext + 2, NULL, 2);
    return LIT_INT;
}
```

Integer literals are stored as `int64_t` in the token value union.

#### 3.7.2 Float Literals

```flex
[0-9]+"."[0-9]+([eE][+-]?[0-9]+)?   {
    yylval->float_val = strtod(yytext, NULL);
    return LIT_FLOAT;
}
```

Float literals are stored as `double`.

#### 3.7.3 String Literals

String literals are accumulated character-by-character in `STRING_LIT` state into a dynamic buffer.

```flex
"\""                      { str_buf_reset(); BEGIN(STRING_LIT); }

<STRING_LIT>"\""          {
    yylval->str = str_buf_finalize();
    BEGIN(INITIAL);
    return LIT_STRING;
}
<STRING_LIT>"\\n"         { str_buf_append('\n'); }
<STRING_LIT>"\\t"         { str_buf_append('\t'); }
<STRING_LIT>"\\\\"        { str_buf_append('\\'); }
<STRING_LIT>"\\\""        { str_buf_append('"'); }
<STRING_LIT>"\\r"         { str_buf_append('\r'); }
<STRING_LIT>"\\0"         { str_buf_append('\0'); }
<STRING_LIT>"\\x"[0-9A-Fa-f]{2} {
    char val = (char)strtol(yytext + 2, NULL, 16);
    str_buf_append(val);
}
<STRING_LIT>\\(.|\n)      {
    diag_error(yyline, yycol, "E011", "Invalid escape sequence");
}
<STRING_LIT>\n            {
    diag_error(yyline, yycol, "E012", "Unterminated string literal");
    yyterminate();
}
<STRING_LIT>.             { str_buf_append(yytext[0]); }
```

#### 3.7.4 Byte Literals

```flex
"b\""                     { bytes_buf_reset(); BEGIN(RAW_BYTES); }

<RAW_BYTES>"\""           {
    yylval->bytes = bytes_buf_finalize();
    BEGIN(INITIAL);
    return LIT_BYTES;
}
<RAW_BYTES>\\[0-9A-Fa-f]{2} {
    uint8_t val = (uint8_t)strtol(yytext + 1, NULL, 16);
    bytes_buf_append(val);
}
<RAW_BYTES>.              { bytes_buf_append((uint8_t)yytext[0]); }
<RAW_BYTES>\n             {
    diag_error(yyline, yycol, "E013", "Unterminated byte literal");
    yyterminate();
}
```

#### 3.7.5 Duration Literals (Program & Recipe only)

Duration literals are parsed as a compound: `LIT_INT DOT_DURATION DURATION_UNIT`. The `DOT_DURATION` distinction from regular `.` is handled at the **parser level** using lookahead — the parser rule for a duration checks that the integer literal is immediately followed by `.` then a duration unit keyword. This avoids lexer ambiguity with floating-point numbers.

Duration units are identifiers that the semantic analyzer checks for validity:

minutes  hours  days  seconds  weeks


So `30.minutes` is lexed as `LIT_INT(30) DOT IDENT("minutes")` and the parser rule `duration_lit` handles the reduction.

### 3.8 Operators & Punctuation

```flex
"+"     { return OP_PLUS; }
"-"     { return OP_MINUS; }
"*"     { return OP_STAR; }
"/"     { return OP_SLASH; }
"%"     { return OP_PERCENT; }
"++"    { return OP_CONCAT; }
"=="    { return OP_EQ; }
"!="    { return OP_NEQ; }
"<"     { return OP_LT; }
">"     { return OP_GT; }
"<="    { return OP_LTE; }
">="    { return OP_GTE; }
"="     { return OP_ASSIGN; }
"?"     { return OP_QUESTION; }
"??"    { return OP_NULLCOAL; }
"!"     { return OP_BANG; }
"->"    { return OP_ARROW; }
"=>"    { return OP_FATARROW; }
".."    { return OP_DOTDOT; }
".."    { return OP_SPREAD; }  /* context-resolved in parser */
"."     { return OP_DOT; }
","     { return OP_COMMA; }
":"     { return OP_COLON; }
"::"    { return OP_COLONCOLON; }
";"     { return OP_SEMICOLON; }
"{"     { return LBRACE; }
"}"     { return RBRACE; }
"["     { return LBRACKET; }
"]"     { return RBRACKET; }
"("     { return LPAREN; }
")"     { return RPAREN; }
"@"     { return AT_SIGN; }
"#"     { return HASH; }
"${"    { return INTERP_OPEN; }  /* recipe env interpolation */
```

> **Note on `..`**: Both `OP_DOTDOT` (range) and `OP_SPREAD` (struct spread) use the same token. The parser disambiguates by context: `OP_DOTDOT` appears in range expressions inside `match` arms and `for` loops; `OP_SPREAD` appears inside struct literal bodies preceded by `..ident`.

### 3.9 Version Literals

Version strings of the form `MAJOR.MINOR.PATCH` (e.g., `1.2.0`) appear in header declarations. They are lexed as a single token:

```flex
[0-9]+"."[0-9]+"."[0-9]+   {
    yylval->str = strdup(yytext);
    return LIT_VERSION;
}
```

This rule must appear **before** the float rule in the Flex file to take priority during header parsing. Since version literals only appear in specific grammar positions, the broader conflict with floats is harmless — the parser only requests a `LIT_VERSION` token in header rules where no float is valid.

### 3.10 Environment Interpolation

In recipe files, string values may contain `${env.VARNAME}`. This is handled by lexing `INTERP_OPEN` (`${`) as a token, followed by normal expression tokens, followed by `RBRACE`. The parser builds an `EnvInterpolation` AST node. The semantic analyzer rejects `${env.*}` outside recipe files.

### 3.11 Slash Commands

Program files use `/command_name` as bot command identifiers. These are lexed as:

```flex
"/"[a-zA-Z_][a-zA-Z0-9_]*   {
    yylval->str = strdup(yytext);   /* includes the leading slash */
    return LIT_COMMAND;
}
```

This rule is active in all lexer states but only valid in program-mode grammar rules. The parser emits error `E020` if a `LIT_COMMAND` token appears outside a `command` declaration.

---

## 4. Grammar (Bison)

### 4.1 Bison File Structure

```c
%{
#include "ast/ast.h"
#include "ast/ast_builder.h"
#include "diag/diagnostic.h"
#include "lexer/token.h"
#include <stdio.h>

extern int yylex(YYSTYPE*, YYLTYPE*, yyscan_t);
extern void yyerror(YYLTYPE*, yyscan_t, ASTNode**, const char*);
%}

%define api.pure full
%define api.push-pull push
%define parse.error verbose
%define parse.lac full

%locations
%param { yyscan_t scanner }
%parse-param { ASTNode** ast_root }

%union {
    int64_t     int_val;
    double      float_val;
    char*       str;
    ByteArray*  bytes;
    ASTNode*    node;
    ASTList*    list;
    ASTType*    type;
}
```

`api.pure full` enables fully reentrant parsing. `parse.lac full` enables look-ahead correction for better error messages. `parse.error verbose` gives Bison's built-in verbose error reporting as a baseline; the compiler's diagnostic layer augments this.

### 4.2 Operator Precedence

Precedence is declared from **lowest** to **highest**:

```bison
%nonassoc   KW_RETURN KW_RAISE
%right      OP_ASSIGN
%left       OP_NULLCOAL
%left       KW_OR
%left       KW_AND
%left       KW_NOT
%left       OP_EQ OP_NEQ
%left       OP_LT OP_GT OP_LTE OP_GTE
%left       OP_CONCAT
%left       OP_PLUS OP_MINUS
%left       OP_STAR OP_SLASH OP_PERCENT
%right      UNARY_MINUS UNARY_NOT
%left       OP_DOT OP_QUESTION OP_BANG
%left       LBRACKET LPAREN
```

### 4.3 Top-Level Grammar

```bison
%start translation_unit

translation_unit
    : module_file   { *ast_root = $1; }
    | program_file  { *ast_root = $1; }
    | recipe_file   { *ast_root = $1; }
    ;
```

The parser does not decide which alternative to try based on the extension; instead, the first token after comments determines which branch is taken:

- `KW_MODULE` → `module_file`
- `KW_PROGRAM` → `program_file`
- `KW_RECIPE` → `recipe_file`

### 4.4 Shared Sub-Grammars

These rules are used by all three file types.

#### 4.4.1 Import Declarations

```bison
import_decl
    : KW_IMPORT qualified_ident
        { $$ = ast_import($2, NULL); }
    | KW_IMPORT qualified_ident LBRACE import_items RBRACE
        { $$ = ast_import($2, $4); }
    ;

qualified_ident
    : IDENT
        { $$ = ast_qident_single($1); }
    | qualified_ident OP_DOT IDENT
        { $$ = ast_qident_extend($1, $3); }
    ;

import_items
    : IDENT
        { $$ = ast_list_single(ast_import_item($1)); }
    | import_items OP_COMMA IDENT
        { $$ = ast_list_append($1, ast_import_item($3)); }
    ;
```

#### 4.4.2 Type Expressions

```bison
type_expr
    : IDENT
        { $$ = ast_type_named($1); }
    | type_expr OP_QUESTION
        { $$ = ast_type_optional($1); }
    | type_expr OP_BANG
        { $$ = ast_type_failable($1); }
    | LBRACKET type_expr RBRACKET
        { $$ = ast_type_list($2); }
    | LBRACE type_expr OP_COLON type_expr RBRACE
        { $$ = ast_type_map($2, $4); }
    | LPAREN type_list RPAREN
        { $$ = ast_type_tuple($2); }
    | IDENT OP_LT type_list OP_GT
        { $$ = ast_type_generic($1, $3); }
    ;

type_list
    : type_expr
        { $$ = ast_list_single($1); }
    | type_list OP_COMMA type_expr
        { $$ = ast_list_append($1, $3); }
    ;
```

#### 4.4.3 Expressions

```bison
expr
    : literal
    | qualified_ident
        { $$ = ast_expr_ident($1); }
    | expr OP_DOT IDENT
        { $$ = ast_expr_field_access($1, $3); }
    | expr LPAREN call_args RPAREN
        { $$ = ast_expr_call($1, $3); }
    | expr LBRACKET expr RBRACKET
        { $$ = ast_expr_index($1, $3); }
    | expr OP_PLUS expr
        { $$ = ast_expr_binop(BOP_ADD, $1, $3); }
    | expr OP_MINUS expr
        { $$ = ast_expr_binop(BOP_SUB, $1, $3); }
    | expr OP_STAR expr
        { $$ = ast_expr_binop(BOP_MUL, $1, $3); }
    | expr OP_SLASH expr
        { $$ = ast_expr_binop(BOP_DIV, $1, $3); }
    | expr OP_PERCENT expr
        { $$ = ast_expr_binop(BOP_MOD, $1, $3); }
    | expr OP_CONCAT expr
        { $$ = ast_expr_binop(BOP_CONCAT, $1, $3); }
    | expr OP_EQ expr
        { $$ = ast_expr_binop(BOP_EQ, $1, $3); }
    | expr OP_NEQ expr
        { $$ = ast_expr_binop(BOP_NEQ, $1, $3); }
    | expr OP_LT expr
        { $$ = ast_expr_binop(BOP_LT, $1, $3); }
    | expr OP_GT expr
        { $$ = ast_expr_binop(BOP_GT, $1, $3); }
    | expr OP_LTE expr
        { $$ = ast_expr_binop(BOP_LTE, $1, $3); }
    | expr OP_GTE expr
        { $$ = ast_expr_binop(BOP_GTE, $1, $3); }
    | expr KW_AND expr
        { $$ = ast_expr_binop(BOP_AND, $1, $3); }
    | expr KW_OR expr
        { $$ = ast_expr_binop(BOP_OR, $1, $3); }
    | KW_NOT expr
        { $$ = ast_expr_unop(UOP_NOT, $2); }
    | OP_MINUS expr %prec UNARY_MINUS
        { $$ = ast_expr_unop(UOP_NEG, $2); }
    | expr OP_QUESTION
        { $$ = ast_expr_optional_chain($1); }
    | expr OP_NULLCOAL expr
        { $$ = ast_expr_nullcoal($1, $3); }
    | KW_TRY expr
        { $$ = ast_expr_try($2); }
    | KW_CATCH expr block
        { $$ = ast_expr_catch($2, NULL, $3); }
    | KW_CATCH expr LBRACE catch_arms RBRACE
        { $$ = ast_expr_catch($2, $4, NULL); }
    | KW_RAISE expr
        { $$ = ast_expr_raise($2); }
    | struct_literal
    | list_literal
    | map_literal
    | tuple_literal
    | match_expr
    | if_expr
    | LPAREN expr RPAREN
        { $$ = $2; }
    ;

literal
    : LIT_INT     { $$ = ast_lit_int($1); }
    | LIT_FLOAT   { $$ = ast_lit_float($1); }
    | LIT_STRING  { $$ = ast_lit_string($1); }
    | LIT_BYTES   { $$ = ast_lit_bytes($1); }
    | KW_TRUE     { $$ = ast_lit_bool(1); }
    | KW_FALSE    { $$ = ast_lit_bool(0); }
    | KW_NONE     { $$ = ast_lit_none(); }
    ;

call_args
    : /* empty */             { $$ = ast_list_empty(); }
    | call_arg_list           { $$ = $1; }
    ;

call_arg_list
    : expr
        { $$ = ast_list_single($1); }
    | call_arg_list OP_COMMA expr
        { $$ = ast_list_append($1, $3); }
    ;

catch_arms
    : catch_arm                   { $$ = ast_list_single($1); }
    | catch_arms catch_arm        { $$ = ast_list_append($1, $2); }
    ;

catch_arm
    : IDENT OP_FATARROW block
        { $$ = ast_catch_arm($1, $3); }
    ;
```

#### 4.4.4 Statements

```bison
stmt
    : let_stmt
    | assign_stmt
    | expr_stmt
    | return_stmt
    | if_stmt
    | for_stmt
    | while_stmt
    | block_stmt
    ;

let_stmt
    : KW_LET IDENT OP_COLON type_expr OP_ASSIGN expr
        { $$ = ast_let($2, $4, $6, 0); }
    | KW_LET KW_MUTABLE IDENT OP_COLON type_expr OP_ASSIGN expr
        { $$ = ast_let($3, $5, $7, 1); }
    | KW_LET IDENT OP_ASSIGN expr
        { $$ = ast_let($2, NULL, $4, 0); }  /* type inferred */
    | KW_LET KW_MUTABLE IDENT OP_ASSIGN expr
        { $$ = ast_let($3, NULL, $5, 1); }
    ;

assign_stmt
    : IDENT OP_ASSIGN expr
        { $$ = ast_assign(ast_expr_ident_simple($1), $3); }
    | expr OP_DOT IDENT OP_ASSIGN expr
        { $$ = ast_assign(ast_expr_field_access($1, $3), $5); }
    | expr LBRACKET expr RBRACKET OP_ASSIGN expr
        { $$ = ast_assign(ast_expr_index($1, $3), $6); }
    ;

return_stmt
    : KW_RETURN expr   { $$ = ast_return($2); }
    | KW_RETURN        { $$ = ast_return(NULL); }
    ;

stmt_list
    : /* empty */         { $$ = ast_list_empty(); }
    | stmt_list stmt      { $$ = ast_list_append($1, $2); }
    ;

block
    : LBRACE stmt_list RBRACE
        { $$ = ast_block($2); }
    ;
```

#### 4.4.5 Struct, Enum, Function Declarations

```bison
struct_decl
    : KW_STRUCT IDENT LBRACE struct_fields RBRACE
        { $$ = ast_struct($2, NULL, $4); }
    | KW_STRUCT IDENT OP_LT type_params OP_GT LBRACE struct_fields RBRACE
        { $$ = ast_struct($2, $4, $7); }
    ;

struct_fields
    : /* empty */                  { $$ = ast_list_empty(); }
    | struct_fields struct_field   { $$ = ast_list_append($1, $2); }
    ;

struct_field
    : IDENT OP_COLON type_expr OP_COMMA
        { $$ = ast_struct_field($1, $3); }
    | IDENT OP_COLON type_expr   /* trailing field, no comma */
        { $$ = ast_struct_field($1, $3); }
    ;

enum_decl
    : KW_ENUM IDENT LBRACE enum_variants RBRACE
        { $$ = ast_enum($2, NULL, $4); }
    | KW_ENUM IDENT OP_LT type_params OP_GT LBRACE enum_variants RBRACE
        { $$ = ast_enum($2, $4, $7); }
    ;

enum_variants
    : enum_variant                         { $$ = ast_list_single($1); }
    | enum_variants OP_COMMA enum_variant  { $$ = ast_list_append($1, $3); }
    | enum_variants OP_COMMA               { $$ = $1; } /* trailing comma */
    ;

enum_variant
    : IDENT
        { $$ = ast_enum_variant($1, NULL); }
    | IDENT LPAREN type_list RPAREN
        { $$ = ast_enum_variant($1, $3); }
    ;

fn_decl
    : KW_FN IDENT LPAREN param_list RPAREN OP_ARROW type_expr block
        { $$ = ast_fn($2, $4, $7, $8, 0); }
    | KW_FN IDENT LPAREN param_list RPAREN block
        { $$ = ast_fn($2, $4, NULL, $6, 0); }   /* void return */
    | KW_PUB KW_FN IDENT LPAREN param_list RPAREN OP_ARROW type_expr block
        { $$ = ast_fn($3, $5, $8, $9, 1); }
    | KW_PUB KW_FN IDENT LPAREN param_list RPAREN block
        { $$ = ast_fn($3, $5, NULL, $7, 1); }
    ;

param_list
    : /* empty */             { $$ = ast_list_empty(); }
    | param_list_nonempty     { $$ = $1; }
    ;

param_list_nonempty
    : param
        { $$ = ast_list_single($1); }
    | param_list_nonempty OP_COMMA param
        { $$ = ast_list_append($1, $3); }
    ;

param
    : IDENT OP_COLON type_expr
        { $$ = ast_param($1, $3); }
    ;

type_params
    : IDENT
        { $$ = ast_list_single(ast_type_param($1)); }
    | type_params OP_COMMA IDENT
        { $$ = ast_list_append($1, ast_type_param($3)); }
    ;
```

#### 4.4.6 Match Expression

```bison
match_expr
    : KW_MATCH expr LBRACE match_arms RBRACE
        { $$ = ast_match($2, $4); }
    ;

match_arms
    : match_arm                  { $$ = ast_list_single($1); }
    | match_arms match_arm       { $$ = ast_list_append($1, $2); }
    ;

match_arm
    : match_pattern OP_FATARROW block
        { $$ = ast_match_arm($1, $3); }
    ;

match_pattern
    : literal
        { $$ = ast_pat_literal($1); }
    | IDENT
        { $$ = ast_pat_bind($1); }
    | IDENT LPAREN pattern_list RPAREN
        { $$ = ast_pat_enum_variant($1, $3); }
    | LIT_INT OP_DOTDOT LIT_INT
        { $$ = ast_pat_range($1, $3); }
    | KW_NONE
        { $$ = ast_pat_none(); }
    | IDENT OP_QUESTION
        { $$ = ast_pat_some($1); }
    | IDENT OP_ASSIGN match_pattern
        { $$ = ast_pat_alias($1, $3); }
    | OP_DOTDOT   /* wildcard `_` is an IDENT, but `..` as wildcard rest */
        { $$ = ast_pat_wildcard(); }
    | IDENT  /* if IDENT == "_" the semantic phase converts to wildcard */
    ;

pattern_list
    : match_pattern
        { $$ = ast_list_single($1); }
    | pattern_list OP_COMMA match_pattern
        { $$ = ast_list_append($1, $3); }
    ;
```

#### 4.4.7 Struct and Collection Literals

```bison
struct_literal
    : IDENT LBRACE struct_literal_fields RBRACE
        { $$ = ast_struct_literal($1, $3, NULL); }
    | IDENT LBRACE OP_DOTDOT IDENT OP_COMMA struct_literal_fields RBRACE
        { $$ = ast_struct_literal($1, $6, $4); }  /* spread: ..base */
    | IDENT LBRACE OP_DOTDOT IDENT RBRACE
        { $$ = ast_struct_literal($1, NULL, $4); } /* only spread */
    ;

struct_literal_fields
    : struct_literal_field
        { $$ = ast_list_single($1); }
    | struct_literal_fields OP_COMMA struct_literal_field
        { $$ = ast_list_append($1, $3); }
    | struct_literal_fields OP_COMMA
        { $$ = $1; }  /* trailing comma */
    ;

struct_literal_field
    : IDENT OP_COLON expr
        { $$ = ast_struct_lit_field($1, $3); }
    | IDENT   /* shorthand: field name == variable name */
        { $$ = ast_struct_lit_field_shorthand($1); }
    ;

list_literal
    : LBRACKET RBRACKET
        { $$ = ast_list_literal(ast_list_empty()); }
    | LBRACKET expr_list RBRACKET
        { $$ = ast_list_literal($2); }
    ;

map_literal
    : LBRACE RBRACE
        { $$ = ast_map_literal(ast_list_empty()); }
    | LBRACE map_entries RBRACE
        { $$ = ast_map_literal($2); }
    ;

map_entries
    : map_entry
        { $$ = ast_list_single($1); }
    | map_entries OP_COMMA map_entry
        { $$ = ast_list_append($1, $3); }
    | map_entries OP_COMMA
        { $$ = $1; }
    ;

map_entry
    : expr OP_COLON expr
        { $$ = ast_map_entry($1, $3); }
    ;

tuple_literal
    : LPAREN expr OP_COMMA expr_list RPAREN
        { $$ = ast_tuple_literal(ast_list_prepend($4, $2)); }
    ;

expr_list
    : expr
        { $$ = ast_list_single($1); }
    | expr_list OP_COMMA expr
        { $$ = ast_list_append($1, $3); }
    ;
```

---

### 4.5 Module File Grammar

```bison
module_file
    : module_header import_list module_item_list
        { $$ = ast_module_file($1, $2, $3); }
    ;

module_header
    : KW_MODULE IDENT LIT_VERSION
        { $$ = ast_module_header($2, $3); }
    ;

module_item_list
    : /* empty */                        { $$ = ast_list_empty(); }
    | module_item_list module_item       { $$ = ast_list_append($1, $2); }
    ;

module_item
    : fn_decl
    | struct_decl
    | enum_decl
    | emit_decl
    | expect_hook_decl
    | on_hook_decl
    | exports_block
    | type_alias
    ;

emit_decl
    : KW_EMIT LIT_STRING LBRACE struct_fields RBRACE
        { $$ = ast_emit_decl($2, $4); }
    ;

expect_hook_decl
    : KW_EXPECT KW_HOOK LIT_STRING
        { $$ = ast_expect_hook($3); }
    ;

on_hook_decl
    : KW_ON KW_HOOK LIT_STRING LPAREN param_list RPAREN block
        { $$ = ast_on_hook($3, $5, $7); }
    ;

exports_block
    : KW_EXPORTS LBRACE export_items RBRACE
        { $$ = ast_exports($3); }
    ;

export_items
    : export_item                        { $$ = ast_list_single($1); }
    | export_items OP_COMMA export_item  { $$ = ast_list_append($1, $3); }
    | export_items OP_COMMA              { $$ = $1; }
    ;

export_item
    : IDENT
        { $$ = ast_export_fn($1); }
    | KW_ON KW_HOOK LIT_STRING
        { $$ = ast_export_hook($3); }
    ;
```

---

### 4.6 Program File Grammar

```bison
program_file
    : program_header program_opt_room import_list program_item_list
        { $$ = ast_program_file($1, $2, $3, $4); }
    ;

program_header
    : KW_PROGRAM IDENT LIT_VERSION
        { $$ = ast_program_header($2, $3); }
    ;

program_opt_room
    : /* empty */               { $$ = NULL; }
    | KW_ROOM LIT_STRING        { $$ = ast_program_room($2); }
    ;

program_item_list
    : /* empty */                            { $$ = ast_list_empty(); }
    | program_item_list program_item         { $$ = ast_list_append($1, $2); }
    ;

program_item
    : fn_decl
    | struct_decl
    | enum_decl
    | command_decl
    | on_event_decl
    | intercept_decl
    | timer_decl
    | type_alias
    ;

command_decl
    : KW_COMMAND LIT_COMMAND LPAREN param_list RPAREN block
        { $$ = ast_command($2, $4, $6); }
    ;

on_event_decl
    : KW_ON KW_EVENT LIT_STRING LPAREN param_list RPAREN block
        { $$ = ast_on_event($3, $5, $7); }
    ;

intercept_decl
    : KW_INTERCEPT KW_MESSAGE LPAREN param_list RPAREN OP_ARROW type_expr block
        { $$ = ast_intercept($4, $7, $8); }
    ;

timer_decl
    : KW_EVERY duration_lit block
        { $$ = ast_timer_every($2, $3); }
    | KW_AT LIT_STRING KW_TIMEZONE LIT_STRING block
        { $$ = ast_timer_at($2, $4, $5); }
    ;

duration_lit
    : LIT_INT OP_DOT IDENT
        { $$ = ast_duration($1, $3); }
        /* semantic check ensures $3 is a valid duration unit */
    ;
```

---

### 4.7 Recipe File Grammar

```bison
recipe_file
    : recipe_header recipe_meta_list recipe_item_list
        { $$ = ast_recipe_file($1, $2, $3); }
    ;

recipe_header
    : KW_RECIPE LIT_STRING LIT_VERSION
        { $$ = ast_recipe_header($2, $3); }
    ;

recipe_meta_list
    : /* empty */                        { $$ = ast_list_empty(); }
    | recipe_meta_list recipe_meta       { $$ = ast_list_append($1, $2); }
    ;

recipe_meta
    : KW_AUTHOR LIT_STRING       { $$ = ast_recipe_meta("author", $2); }
    | KW_DESCRIPTION LIT_STRING  { $$ = ast_recipe_meta("description", $2); }
    ;

recipe_item_list
    : /* empty */                          { $$ = ast_list_empty(); }
    | recipe_item_list recipe_item         { $$ = ast_list_append($1, $2); }
    ;

recipe_item
    : room_block
    | service_decl
    | program_decl
    | module_decl
    | roles_block
    | webhooks_block
    | when_block
    | on_init_block
    ;

room_block
    : KW_ROOM LBRACE room_properties RBRACE
        { $$ = ast_room_block($3); }
    ;

room_properties
    : /* empty */                            { $$ = ast_list_empty(); }
    | room_properties room_property          { $$ = ast_list_append($1, $2); }
    ;

room_property
    : IDENT OP_COLON room_prop_value OP_COMMA
        { $$ = ast_room_prop($1, $3); }
    | IDENT OP_COLON room_prop_value
        { $$ = ast_room_prop($1, $3); }
    ;

room_prop_value
    : LIT_INT       { $$ = ast_rpv_int($1); }
    | LIT_STRING    { $$ = ast_rpv_string($1); }
    | KW_ATOM_ENABLED   { $$ = ast_rpv_atom("enabled"); }
    | KW_ATOM_DISABLED  { $$ = ast_rpv_atom("disabled"); }
    | KW_ATOM_STRICT    { $$ = ast_rpv_atom("strict"); }
    | KW_ATOM_MODERATE  { $$ = ast_rpv_atom("moderate"); }
    | KW_ATOM_RELAXED   { $$ = ast_rpv_atom("relaxed"); }
    | duration_lit  { $$ = ast_rpv_duration($1); }
    ;

service_decl
    : KW_SERVICE LIT_STRING LBRACE service_body RBRACE
        { $$ = ast_service_decl($2, $4); }
    ;

service_body
    : /* empty */                          { $$ = ast_list_empty(); }
    | service_body service_body_item       { $$ = ast_list_append($1, $2); }
    ;

service_body_item
    : KW_FROM OP_COLON LIT_STRING OP_COMMA
        { $$ = ast_svc_from($3); }
    | KW_FROM OP_COLON LIT_STRING
        { $$ = ast_svc_from($3); }
    | KW_AUTOSTART OP_COLON KW_TRUE OP_COMMA
        { $$ = ast_svc_autostart(1); }
    | KW_AUTOSTART OP_COLON KW_FALSE OP_COMMA
        { $$ = ast_svc_autostart(0); }
    | KW_AUTOSTART OP_COLON KW_TRUE
        { $$ = ast_svc_autostart(1); }
    | KW_AUTOSTART OP_COLON KW_FALSE
        { $$ = ast_svc_autostart(0); }
    | KW_CONFIG LBRACE config_entries RBRACE
        { $$ = ast_svc_config($3); }
    ;

config_entries
    : /* empty */                            { $$ = ast_list_empty(); }
    | config_entries config_entry            { $$ = ast_list_append($1, $2); }
    ;

config_entry
    : IDENT OP_COLON config_value OP_COMMA
        { $$ = ast_config_entry($1, $3); }
    | IDENT OP_COLON config_value
        { $$ = ast_config_entry($1, $3); }
    ;

config_value
    : LIT_INT       { $$ = ast_cv_int($1); }
    | LIT_FLOAT     { $$ = ast_cv_float($1); }
    | LIT_STRING    { $$ = ast_cv_string($1); }
    | KW_TRUE       { $$ = ast_cv_bool(1); }
    | KW_FALSE      { $$ = ast_cv_bool(0); }
    | interp_string { $$ = ast_cv_interp($1); }
    ;

interp_string
    : LIT_STRING
        { $$ = ast_interp_plain($1); }
    | INTERP_OPEN KW_ENV OP_DOT IDENT RBRACE
        { $$ = ast_interp_env($4); }
    ;

program_decl
    : KW_PROGRAM LIT_STRING LBRACE KW_FROM OP_COLON LIT_STRING RBRACE
        { $$ = ast_recipe_program($2, $6); }
    ;

module_decl
    : KW_MODULE LIT_STRING LBRACE KW_FROM OP_COLON LIT_STRING RBRACE
        { $$ = ast_recipe_module($2, $6); }
    ;

roles_block
    : KW_ROLES LBRACE roles_body RBRACE
        { $$ = ast_roles_block($3); }
    ;

roles_body
    : /* empty */                      { $$ = ast_list_empty(); }
    | roles_body role_item             { $$ = ast_list_append($1, $2); }
    ;

role_item
    : KW_ROLE LIT_STRING LBRACE role_perms RBRACE
        { $$ = ast_role($2, $4); }
    | KW_DEFAULT_ROLE OP_COLON LIT_STRING
        { $$ = ast_role_default($3); }
    | KW_DEFAULT_ROLE OP_COLON LIT_STRING OP_COMMA
        { $$ = ast_role_default($3); }
    ;

role_perms
    : /* empty */                        { $$ = ast_list_empty(); }
    | role_perms role_perm               { $$ = ast_list_append($1, $2); }
    ;

role_perm
    : KW_CAN OP_COLON LBRACKET perm_list RBRACKET OP_COMMA
        { $$ = ast_role_perm(PERM_CAN, $4); }
    | KW_CAN OP_COLON LBRACKET perm_list RBRACKET
        { $$ = ast_role_perm(PERM_CAN, $4); }
    | KW_CANNOT OP_COLON LBRACKET perm_list RBRACKET OP_COMMA
        { $$ = ast_role_perm(PERM_CANNOT, $4); }
    | KW_CANNOT OP_COLON LBRACKET perm_list RBRACKET
        { $$ = ast_role_perm(PERM_CANNOT, $4); }
    ;

perm_list
    : IDENT
        { $$ = ast_list_single(ast_perm($1)); }
    | perm_list OP_COMMA IDENT
        { $$ = ast_list_append($1, ast_perm($3)); }
    ;

webhooks_block
    : KW_WEBHOOKS LBRACE webhook_list RBRACE
        { $$ = ast_webhooks_block($3); }
    ;

webhook_list
    : /* empty */                       { $$ = ast_list_empty(); }
    | webhook_list webhook_decl         { $$ = ast_list_append($1, $2); }
    ;

webhook_decl
    : KW_ON KW_EVENT LIT_STRING KW_POST KW_TO LIT_STRING
        { $$ = ast_webhook($3, $6, NULL); }
    | KW_ON KW_EVENT LIT_STRING KW_POST KW_TO LIT_STRING LBRACE webhook_opts RBRACE
        { $$ = ast_webhook($3, $6, $8); }
    ;

webhook_opts
    : /* empty */                          { $$ = ast_list_empty(); }
    | webhook_opts webhook_opt             { $$ = ast_list_append($1, $2); }
    ;

webhook_opt
    : KW_HEADERS LBRACE header_entries RBRACE
        { $$ = ast_webhook_headers($3); }
    ;

header_entries
    : header_entry                           { $$ = ast_list_single($1); }
    | header_entries OP_COMMA header_entry   { $$ = ast_list_append($1, $3); }
    ;

header_entry
    : LIT_STRING OP_COLON interp_string
        { $$ = ast_header_entry($1, $3); }
    ;

when_block
    : KW_WHEN expr LBRACE recipe_item_list RBRACE
        { $$ = ast_when_block($2, $4); }
    ;

on_init_block
    : KW_ON KW_INIT block
        { $$ = ast_on_init($3); }
    ;
```

---

## 5. Type System

### 5.1 Type Representation

All types are represented in `sema/type_system.h` as a tagged union. The full C++ type hierarchy:

```cpp
enum class TypeKind {
    // Primitives
    Int, Float, Bool, Str, Byte, Void,
    // Compound
    List, Map, Tuple, Optional, Failable,
    // User-defined
    Struct, Enum, TypeAlias,
    // Generics
    TypeVar,      // Unresolved generic T
    Generic,      // Instantiated generic, e.g. Result<int>
    // Special
    Never,        // Type of `raise` and diverging branches
    Unknown,      // Pre-inference placeholder
};

struct Type {
    TypeKind kind;
    std::string name;               // for named types
    std::vector<Type*> params;      // for Generic, Tuple, Map, etc.
    Type* inner = nullptr;          // for List, Optional, Failable
    StructDef* struct_def = nullptr;
    EnumDef*   enum_def   = nullptr;
    bool       is_mutable = false;
};
```

### 5.2 Primitive Types

| Daffyscript Name | Internal Kind | WASM Representation |
|-----------------|--------------|---------------------|
| `int`           | `Int`        | `i64` |
| `float`         | `Float`      | `f64` |
| `bool`          | `Bool`       | `i32` (0 or 1) |
| `str`           | `Str`        | `i32` (pointer into linear memory) |
| `byte`          | `Byte`       | `i32` |
| `void`          | `Void`       | no WASM value |

### 5.3 Compound Types

#### List `[T]`

Represented as a fat pointer in linear memory: `(i32 ptr, i32 length, i32 capacity)`. The runtime maintains a bump allocator for list storage. List elements are stored contiguously.

#### Map `{K: V}`

Implemented as a hash map in linear memory. Key type must satisfy the `Hashable` constraint (all primitive types are hashable). Stored as `(i32 ptr_to_buckets, i32 count, i32 capacity)`.

#### Tuple `(T1, T2, ...)`

Stored as consecutive fields in linear memory, aligned to the largest member. Represented as a single `i32` pointer.

#### Optional `T?`

Represented as a tagged union: `(i32 tag, <T value>)`. Tag `0` = none, tag `1` = some. For reference types (str, list, struct), the tag is merged with the pointer: `0` = none, non-zero = some.

#### Failable `T!`

A function returning `T!` actually returns a tuple `(i32 error_flag, T value_or_error)`. If `error_flag == 0`, the value is valid. If `error_flag == 1`, the second component is a `str` pointer holding the error message.

### 5.4 Type Inference

Type inference uses **bidirectional type checking** with a constraint set. The implementation follows these passes:

1. **Collection pass**: Gather all top-level declarations and populate the symbol table with their declared types (or `Unknown`).
2. **Inference pass**: Walk expressions bottom-up. For each expression, compute an inferred type. When an `Unknown` is encountered, generate a type variable $T_n$ and add constraints.
3. **Unification pass**: Solve constraints using standard Hindley-Milner unification. If unification fails, emit `E030` (type mismatch).
4. **Substitution pass**: Walk the AST and substitute all resolved type variables.

Type variables are identified by a monotonically increasing integer. Unification is implemented in `sema/type_system.cpp` as `Type* unify(Type* a, Type* b, SourceLocation loc)`.

### 5.5 Generics

Generics are monomorphized at compile time. Each instantiation of a generic function or struct generates a distinct symbol in the symbol table with a mangled name:

fn wrap<T>(...) → wrap__int, wrap__str, etc.


Mangling scheme: `<name>__<type1>__<type2>...` where each type name is its Daffyscript primitive name or struct name. For nested generics: `Result__List__int`.

### 5.6 Subtyping

Daffyscript has minimal subtyping:

- `Never` is a subtype of every type (from `raise`).
- `T` is a subtype of `T?` (a concrete value coerces to optional).
- No other implicit subtyping. All other conversions are explicit.

### 5.7 Mutability

Mutability is a **binding-level** property, not a type-level property. A `let mutable` binding can be reassigned. A `let` binding cannot. Passing a mutable binding to a function that takes an immutable parameter is allowed (the function cannot mutate it). There are no reference types — all values are passed by copy, except structs and lists which are passed by pointer; the callee cannot modify the caller's binding, only the pointed-to data if the struct is mutable. This is defined at the semantic level as:

- Struct fields are immutable by default.
- A struct binding declared `mutable` allows field assignment.
- A struct binding declared without `mutable` disallows field assignment, producing error `E031`.

---

## 6. Semantic Analysis

### 6.1 Pass Order

Semantic analysis runs in the following sequence, all implemented in `sema/sema.cpp`:

Pass 1: Header Validation
Pass 2: Import Resolution
Pass 3: Declaration Collection
Pass 4: Type Inference & Checking
Pass 5: Name Resolution
Pass 6: File-Type Constraint Enforcement
Pass 7: Mutability Checking
Pass 8: Exhaustiveness Checking (match)
Pass 9: Reachability & Return Checking


### 6.2 Symbol Table

The symbol table is a **scoped, persistent** table:

```cpp
class SymbolTable {
public:
    void push_scope();
    void pop_scope();
    void define(const std::string& name, Symbol sym);
    Symbol* lookup(const std::string& name);  // walks scope chain
    Symbol* lookup_local(const std::string& name);  // current scope only
private:
    std::vector<std::unordered_map<std::string, Symbol>> scopes;
};

struct Symbol {
    std::string      name;
    Type*            type;
    SymbolKind       kind;   // Var, Fn, Struct, Enum, Import, Param
    bool             is_mutable;
    SourceLocation   defined_at;
    ASTNode*         ast_node;
};
```

### 6.3 Pass 1: Header Validation

- Confirms that the header keyword (`module`/`program`/`recipe`) matches the file extension.
- Validates the version string format (`MAJOR.MINOR.PATCH`, all numeric).
- Stores the file type in a global compilation context `CompileCtx`.

### 6.4 Pass 2: Import Resolution

- For each `import` declaration, look up the qualified name in:
  1. The standard library manifest (`stdlib/manifest.json`, generated at build time).
  2. Relative paths (for `import ./util`).
  3. Installed DaffyChat packages (future; not in v0.1.0).
- If the qualified name is not found, emit `E040` (unresolved import).
- Populate the symbol table with imported symbols.

### 6.5 Pass 3: Declaration Collection

Walk all top-level items and insert them into the symbol table:

- Struct declarations → `SymbolKind::Struct`
- Enum declarations → `SymbolKind::Enum`
- Function declarations → `SymbolKind::Fn`
- Type aliases → `SymbolKind::TypeAlias`

At this point, function bodies are **not** analyzed; only signatures are collected. This enables mutual recursion.

### 6.6 Pass 4: Type Inference & Checking

Walk all function bodies and expression contexts. Key rules:

- Binary operator `++` requires both operands to be `str` or both to be `[T]` for the same `T`. Result is the same type.
- Binary arithmetic operators require both operands to be `int` or both to be `float`. No implicit widening.
- Comparison operators `==` and `!=` require identical types. `<`, `>`, `<=`, `>=` require both to be `int` or both `float`.
- `and`, `or`, `not` require `bool` operands.
- Null coalescing `??` requires left side to be `T?` and right side to be `T`. Result is `T`.
- Optional chaining `expr?` on a field access or method call short-circuits to `none` if the receiver is `none`.
- `try expr` requires `expr` to have a failable type `T!`. The result type is `T`, and the error propagates to the enclosing failable function. If the enclosing function is not failable, emit `E041`.
- `catch expr { arm }` requires `expr` to have a failable type `T!`. The catch arm receives a `str` error. The arm must produce a `T` value.

### 6.7 Pass 5: Name Resolution

Resolve all identifier references to their declared symbols:

- `qualified_ident` chains are resolved left-to-right: the first component is looked up in the symbol table; subsequent components are looked up in the namespace of the resolved symbol.
- Method calls `expr.method(args)` are resolved by looking up `method` in the type's method table. Built-in methods for `str`, `[T]`, `{K:V}` are pre-loaded.
- Unresolved names emit `E050`.

### 6.8 Pass 6: File-Type Constraint Enforcement

This pass enforces the isolation rules between file types:

| Construct | `.dfy` | `.dfyp` | `.dfyr` |
|-----------|--------|---------|---------|
| `emit`              | ✓ | ✗ (`E060`) | ✗ (`E060`) |
| `expect hook`       | ✓ | ✗ (`E061`) | ✗ (`E061`) |
| `on hook`           | ✓ | ✗ (`E061`) | ✗ (`E061`) |
| `command`           | ✗ (`E062`) | ✓ | ✗ (`E062`) |
| `on event`          | ✗ (`E063`) | ✓ | ✗ (`E063`) |
| `intercept message` | ✗ (`E064`) | ✓ | ✗ (`E064`) |
| `every`/`at`        | ✗ (`E065`) | ✓ | ✗ (`E065`) |
| `service`/`program`/`module` decls | ✗ | ✗ | ✓ |
| `room {}`           | ✗ (`E066`) | ✗ (`E066`) | ✓ |
| `when`              | ✗ (`E067`) | ✗ (`E067`) | ✓ |
| `on init`           | ✗ (`E068`) | ✗ (`E068`) | ✓ |
| `ldc.*` imports     | ✗ (`E069`) | ✓ | only in `on init` |
| `dfc.*` imports     | ✓ | ✗ (`E070`) | ✗ (`E070`) |
| `${env.*}`          | ✗ (`E071`) | ✗ (`E071`) | ✓ |

### 6.9 Pass 7: Mutability Checking

Walk all assignment statements:

- If the LHS resolves to a binding declared without `mutable`, emit `E031`.
- If the LHS is a field access `a.b = ...`, check that `a` is a `mutable` binding.
- Parameters are always immutable.

### 6.10 Pass 8: Match Exhaustiveness

For each `match` expression, verify exhaustiveness:

- If matching on an enum type, every variant must be covered, either explicitly or via a wildcard `_` arm.
- If matching on `int`, a wildcard arm is required unless the range covers all values (not checked at v0.1.0; wildcard always required for int).
- If matching on `T?`, both `none` and `T` (via a binding pattern) must be covered.
- Non-exhaustive match emits `E080`. Unreachable arms (shadowed by prior pattern) emit warning `W010`.

### 6.11 Pass 9: Reachability & Return Checking

- Every non-void function must return a value on all code paths. Missing return emits `E090`.
- A `return` after a `raise` or inside an arm following a diverging expression emits warning `W011` (unreachable code).
- `for` and `while` loops with a `return` inside do not count as a definite return for the enclosing function (since they may not execute).

---

## 7. IR & Code Generation (Binaryen)

### 7.1 Binaryen API Usage

The code generator uses the Binaryen C API (`binaryen-c.h`). The module handle is created once per compilation unit:

```cpp
BinaryenModuleRef mod = BinaryenModuleCreate();
```

At the end of codegen, the module is validated and written:

```cpp
BinaryenModuleValidate(mod);
BinaryenModuleOptimize(mod);  // if opt level > 0
BinaryenModuleWrite(mod, output_path);
BinaryenModuleDispose(mod);
```

### 7.2 Memory Layout

Each compiled WASM module uses a single linear memory with the following layout:

[0x0000 - 0x00FF]  Reserved / null guard
[0x0100 - 0x1FFF]  Static string table (string literals)
[0x2000 - 0x7FFF]  Static data (struct layouts, vtables)
[0x8000 -      ...]  Heap (bump allocator, grows upward)


The static string table is populated during codegen. Each string literal is interned: if the same string appears twice, it reuses the same offset. String pointers are `i32` offsets into linear memory.

The heap uses a simple bump allocator. `alloc(size)` is an internal function that increments the `heap_ptr` global and returns the old value:

```wat
(global $heap_ptr (mut i32) (i32.const 0x8000))

(func $alloc (param $size i32) (result i32)
  (local $ptr i32)
  (local.set $ptr (global.get $heap_ptr))
  (global.set $heap_ptr
    (i32.add (global.get $heap_ptr) (local.get $size)))
  (local.get $ptr)
)
```

### 7.3 Lowering Strategy

Each AST node kind has a corresponding lowering function in `codegen/bir_lowering.cpp`:

```cpp
BinaryenExpressionRef lower_expr(BinaryenModuleRef mod,
                                  ASTNode* node,
                                  LowerCtx& ctx);

BinaryenExpressionRef lower_stmt(BinaryenModuleRef mod,
                                  ASTNode* node,
                                  LowerCtx& ctx);
```

`LowerCtx` carries:

```cpp
struct LowerCtx {
    BinaryenModuleRef     mod;
    SymbolTable&          sym;
    FunctionBuilder*      fn;      // current function being built
    LocalsMap             locals;  // name → Binaryen local index
    int                   opt_level;
    FileType              file_type;
    StringTable&          strtab;
};
```

### 7.4 Primitive Value Codegen

| Daffyscript | Binaryen Expression |
|------------|---------------------|
| `int` literal | `BinaryenConst(mod, BinaryenLiteralInt64(v))` |
| `float` literal | `BinaryenConst(mod, BinaryenLiteralFloat64(v))` |
| `bool` literal | `BinaryenConst(mod, BinaryenLiteralInt32(v ? 1 : 0))` |
| `str` literal | `BinaryenConst(mod, BinaryenLiteralInt32(strtab.intern(s)))` |
| `none` | `BinaryenConst(mod, BinaryenLiteralInt32(0))` |

### 7.5 Function Codegen

For each `fn_decl`:

1. Collect all parameter types and map to Binaryen types.
2. Collect all `let` declarations in the body to determine locals count.
3. Call `BinaryenAddFunction`.
4. For `pub` functions, call `BinaryenAddFunctionExport`.

```cpp
// Example: fn add(a: int, b: int) -> int { return a + b }
BinaryenType params[] = { BinaryenTypeInt64(), BinaryenTypeInt64() };
BinaryenType results  = BinaryenTypeInt64();

BinaryenExpressionRef body = BinaryenBinary(
    mod,
    BinaryenAddInt64(),
    BinaryenLocalGet(mod, 0, BinaryenTypeInt64()),
    BinaryenLocalGet(mod, 1, BinaryenTypeInt64())
);

BinaryenAddFunction(mod, "add", 
    BinaryenTypeCreate(params, 2), results,
    NULL, 0,    // no extra locals
    body);

BinaryenAddFunctionExport(mod, "add", "add");
```

### 7.6 Control Flow Codegen

#### If-Else

```cpp
// if cond { then_block } else { else_block }
BinaryenIf(mod, lower_expr(cond), lower_block(then_block), lower_block(else_block))
```

#### While Loop

While loops are lowered to a `block` + `loop` + `br_if`:

```wat
(block $break
  (loop $continue
    (br_if $break (i32.eqz <condition>))
    <body>
    (br $continue)
  )
)
```

In Binaryen C API:

```cpp
BinaryenExpressionRef loop_body[] = {
    BinaryenBreak(mod, "break", 
        BinaryenUnary(mod, BinaryenEqZInt32(), lower_expr(cond)), 
        NULL),
    lower_block(body),
    BinaryenBreak(mod, "continue", NULL, NULL),
};
BinaryenExpressionRef loop = BinaryenLoop(mod, "continue",
    BinaryenBlock(mod, NULL, loop_body, 3, BinaryenTypeNone()));
BinaryenBlock(mod, "break", &loop, 1, BinaryenTypeNone());
```

#### For Loop

```daffyscript
for item in collection { ... }
```

Is lowered to an index-based while loop over the collection's length, with `item` bound to `collection[i]` at each iteration.

#### Match

Match is lowered to a chain of `BinaryenIf`/`BinaryenBlock` nodes. For enum matches, the tag field is extracted from the enum value pointer and compared. For range patterns, bounds checks are generated. The wildcard arm, if present, becomes the final `else` branch.

### 7.7 Struct Codegen

Structs are laid out in linear memory. The codegen phase computes field offsets using a standard C-like layout algorithm (fields packed in declaration order, each aligned to its natural alignment).

A struct type `S { a: int, b: float, c: bool }` gets:

offset 0:  i64  a
offset 8:  f64  b
offset 16: i32  c
total size: 20 bytes (padded to 24 for 8-byte alignment)


Struct allocation calls `$alloc(sizeof(S))` and stores field values via `BinaryenStore`.

Field access `s.field` becomes a `BinaryenLoad` at the computed offset.

### 7.8 String Runtime

Strings are represented as `(i32 ptr, i32 length)` pairs. However, in most contexts only the pointer is stored as the primary value; the length is stored at `ptr - 4` (a length-prefixed string layout). Built-in string operations are compiled as calls to internal runtime functions:

__str_concat(ptr_a: i32, ptr_b: i32) -> i32
__str_len(ptr: i32) -> i32
__str_eq(ptr_a: i32, ptr_b: i32) -> i32
__str_starts_with(ptr: i32, prefix: i32) -> i32
__str_contains(ptr: i32, sub: i32) -> i32
__str_to_upper(ptr: i32) -> i32
__str_to_lower(ptr: i32) -> i32
__str_trim_start(ptr: i32, chars: i32) -> i32
__str_join(list_ptr: i32, sep: i32) -> i32
__int_to_str(val: i64) -> i32
__float_to_str(val: f64) -> i32


These are implemented in a companion C file `stdlib/str_runtime.c`, compiled to WASM and linked via Binaryen's module merging: `BinaryenModuleMerge`.

### 7.9 Failable Function Codegen

A function `fn f() -> T!` has its WASM signature modified to return a struct `(i32 error_flag, T value)`. Since WASM multi-value return is supported in Binaryen via `BinaryenTypeCreate` with multiple value types:

```cpp
BinaryenType results[] = { BinaryenTypeInt32(), lower_type(T) };
BinaryenType result_type = BinaryenTypeCreate(results, 2);
```

`raise "msg"` lowers to:

```cpp
BinaryenExpressionRef raise_exprs[] = {
    BinaryenConst(mod, BinaryenLiteralInt32(1)),     // error_flag = 1
    BinaryenConst(mod, BinaryenLiteralInt32(strtab.intern(msg))), // error str
};
BinaryenReturn(mod, BinaryenTupleMake(mod, raise_exprs, 2));
```

`try expr` lowers to:

1. Call the failable function.
2. Extract the error flag via `BinaryenTupleExtract(mod, call_result, 0)`.
3. If error flag is set, return the error from the current function (propagation).
4. Otherwise, extract the value via `BinaryenTupleExtract(mod, call_result, 1)`.

### 7.10 Event Bridge Codegen (Modules)

`emit "event-name" { ... }` in a module compiles to a call to the imported host function `__dfc_emit`:

```wat
(import "dfc" "emit" (func $__dfc_emit (param i32 i32)))
;; params: event_name_ptr: i32, payload_json_ptr: i32
```

The payload struct is serialized to JSON in linear memory by generated serialization code, then passed as a string pointer. The JSON serializer is generated per struct type in `codegen/bir_lowering.cpp` as a dedicated WASM function.

### 7.11 `ldc.*` Codegen (Programs)

All `ldc.*` calls compile to imported host functions. The import namespace is `"ldc"`. For example:

```wat
(import "ldc" "message.send"  (func $ldc_message_send  (param i32)))
(import "ldc" "event.emit"    (func $ldc_event_emit     (param i32 i32)))
(import "ldc" "storage.set"   (func $ldc_storage_set    (param i32 i32)))
(import "ldc" "storage.get"   (func $ldc_storage_get    (param i32) (result i32)))
```

The DaffyChat runtime provides these imports when instantiating the WASM module. The mapping from `ldc.*` qualified names to import function names follows the scheme: replace `.` with `_`, prefix with `ldc_`. The full import table is defined in `stdlib/ldc_bindings.h`.

### 7.12 Recipe Codegen

Recipes do not compile to executable WASM in the traditional sense. Instead, they compile to a **WASM module that exports a single function `__recipe_manifest`** that, when called, invokes imported host functions to provision the room:

```wat
(import "recipe" "declare_service"  (func $declare_service  (param i32 i32 i32)))
(import "recipe" "declare_program"  (func $declare_program  (param i32 i32)))
(import "recipe" "declare_module"   (func $declare_module   (param i32 i32)))
(import "recipe" "set_room_prop"    (func $set_room_prop    (param i32 i32)))
(import "recipe" "declare_role"     (func $declare_role     (param i32)))
(import "recipe" "add_permission"   (func $add_permission   (param i32 i32 i32)))
(import "recipe" "register_webhook" (func $register_webhook (param i32 i32)))
(export "__recipe_manifest" (func $recipe_manifest))
```

The `on init` block is compiled to a second exported function `__recipe_init` that is called after provisioning:

```wat
(export "__recipe_init" (func $recipe_init))
```

`when` blocks are lowered to conditional calls inside `__recipe_manifest`: the condition expression is evaluated, and if true, the enclosed declarations' host calls are executed.

`${env.VARNAME}` compiles to a call to:

```wat
(import "recipe" "get_env" (func $get_env (param i32) (result i32)))
```

where the parameter is the variable name string pointer and the result is a string pointer.

---

## 8. Standard Library Namespaces

### 8.1 `dfc.*` — Frontend Bridge (Modules only)

| Symbol | Signature | Description |
|--------|-----------|-------------|
| `dfc.bridge.emit` | `(event: str, payload: T) -> void` | Emit event to frontend |
| `dfc.bridge.on_hook` | *syntactic* | Handled by `on hook` grammar |
| `dfc.crypto.random_int` | `(min: int, max: int) -> int` | Cryptographic random integer |
| `dfc.crypto.random_bytes` | `(n: int) -> [byte]` | Random byte array |
| `dfc.types.Color` | struct | RGB color representation |

### 8.2 `ldc.*` — Room APIs (Programs only)

#### `ldc.message`

| Symbol | Signature | Description |
|--------|-----------|-------------|
| `ldc.message.send` | `(text: str) -> void` | Send a message to the room |
| `ldc.message.send_formatted` | `(text: str, format: MessageFormat) -> void` | Send with formatting |
| `ldc.message.delete` | `(id: str) -> void!` | Delete a message |
| `ldc.message.pin` | `(id: str) -> void!` | Pin a message |

#### `ldc.event`

| Symbol | Signature | Description |
|--------|-----------|-------------|
| `ldc.event.emit` | `(event: str, payload: {str: str}) -> void` | Emit to the event bus |
| `ldc.event.subscribe` | `(event: str, handler: fn({str:str}) -> void) -> void` | Subscribe dynamically |

#### `ldc.storage`

| Symbol | Signature | Description |
|--------|-----------|-------------|
| `ldc.storage.set` | `(key: str, value: T) -> void` | Persist a value |
| `ldc.storage.get` | `(key: str) -> T?` | Retrieve a value |
| `ldc.storage.delete` | `(key: str) -> void` | Delete a key |

#### `ldc.service`

| Symbol | Signature | Description |
|--------|-----------|-------------|
| `ldc.service.call` | `(service: str, method: str, args: {str: str}) -> {str: str}!` | RPC call |

#### `ldc.crypto`

| Symbol | Signature | Description |
|--------|-----------|-------------|
| `ldc.crypto.random_int` | `(min: int, max: int) -> int` | Secure random |
| `ldc.crypto.hash` | `(data: str, algo: HashAlgo) -> str` | Hash a string |

#### `ldc.http`

| Symbol | Signature | Description |
|--------|-----------|-------------|
| `ldc.http.get` | `(url: str) -> HttpResponse!` | HTTP GET |
| `ldc.http.post` | `(url: str, body: str) -> HttpResponse!` | HTTP POST |

### 8.3 Built-in Type Methods

These are resolved at compile time and do not go through the import mechanism:

#### `str` methods

| Method | Signature | Lowers to |
|--------|-----------|-----------|
| `.len()` | `() -> int` | `__str_len` |
| `.contains(sub)` | `(str) -> bool` | `__str_contains` |
| `.starts_with(pre)` | `(str) -> bool` | `__str_starts_with` |
| `.to_uppercase()` | `() -> str` | `__str_to_upper` |
| `.to_lowercase()` | `() -> str` | `__str_to_lower` |
| `.trim_start(chars)` | `(str) -> str` | `__str_trim_start` |
| `.to_str()` | `() -> str` | identity |
| `.parse_int()` | `() -> int!` | `__str_parse_int` |

#### `[T]` methods

| Method | Signature | Lowers to |
|--------|-----------|-----------|
| `.len()` | `() -> int` | inline load |
| `.append(item)` | `(T) -> void` | `__list_append` |
| `.join(sep)` | `(str) -> str` (T must be str) | `__str_join` |
| `.enumerate()` | `() -> [(int, T)]` | `__list_enumerate` |

#### `int` methods

| Method | Signature | Lowers to |
|--------|-----------|-----------|
| `.to_str()` | `() -> str` | `__int_to_str` |

---

## 9. File-Type-Specific Semantics

### 9.1 Module Semantics

#### Emit Declarations

`emit "event-name" { field: type, ... }` does two things:

1. **Registers the event shape**: The semantic analyzer records the event name and its payload type in the module's event registry. If the same event name is emitted twice with different shapes, `E100` is emitted.
2. **Generates a typed emitter function**: The codegen phase generates a helper function `__emit_<sanitized_event_name>(payload_ptr: i32)` that serializes the struct to JSON and calls `__dfc_emit`.

#### Expect Hook Declarations

`expect hook "hook-name"` registers a hook name that the frontend is expected to implement. At runtime, if the frontend does not register a handler for this hook, the event is silently dropped (this is by design — the spec says the frontend *optionally* complies). The semantic analyzer warns `W020` if an `expect hook` is declared but no `on hook` for the same name exists.

#### Exports Block

If present, the exports block is authoritative. Any `pub fn` not listed in exports receives warning `W021`. Any name listed in exports that is not `pub` or not defined emits `E101`.

If absent, all `pub fn` functions and all `on hook` handlers are exported automatically.

### 9.2 Program Semantics

#### Command Declarations

A `command "/name"` declaration registers the slash command globally for the room. If two commands with the same name are declared in the same program file, `E110` is emitted. Duplicate command names across different program files in the same room are a runtime conflict, not a compile-time error; the runtime resolves by last-loaded-wins.

The `args` parameter is always of type `[str]`. Command declarations must have exactly one parameter. If not, `E111` is emitted.

#### Event Handlers

`on event "event.name"` declarations are registered with the event bus at WASM module instantiation. The payload type is checked against the declared parameter struct in the semantic analysis pass. If the payload struct fields do not match the event's registered schema (from the DSSL-defined service that emits the event), warning `W030` is emitted.

#### Intercept Declarations

At most one `intercept message` declaration is allowed per program file. Multiple intercept declarations emit `E112`. The return type must be `Message?`. `Message` is a built-in struct defined in `ldc.message`:

```daffyscript
struct Message {
    id:        str,
    sender:    str,
    text:      str,
    timestamp: int,
    room:      str,
}
```

#### Timer Declarations

- `every N.unit` — `N` must be a positive integer, `unit` must be one of `seconds`, `minutes`, `hours`, `days`, `weeks`. Semantic check in Pass 4 validates the unit identifier. Invalid units emit `E113`.
- `at "HH:MM" timezone "TZ"` — The time string is validated against the regex `^([01]\d|2[0-3]):[0-5]\d$`. The timezone string is checked against the IANA timezone database bundled at compile time. Invalid time format emits `E114`; invalid timezone emits `E115`.

### 9.3 Recipe Semantics

#### Room Properties

Valid room properties and their value types:

| Property | Value Type | Valid Values / Range |
|----------|-----------|---------------------|
| `max_users` | `int` | 1–10000 |
| `voice` | atom | `enabled`, `disabled` |
| `history` | duration | any valid duration |
| `language` | `str` | IETF BCP 47 language tag |
| `moderation` | atom | `strict`, `moderate`, `relaxed` |

Unknown property names emit warning `W040`. Out-of-range values emit `E120`.

#### Service Declarations

- `from` field is required. If absent, `E121` is emitted.
- The `.da` file referenced by `from` is not validated at compile time (it is a runtime artifact). A warning `W041` is emitted if the file does not exist relative to the recipe file's directory.
- `config` entries have no compile-time schema (the service schema is in the `.da` binary). They are encoded as-is.

#### Roles & Permissions

The `default_role` must reference a `role` declared in the same `roles` block. If it does not, `E122` is emitted. Permission names are arbitrary identifiers; the DaffyChat runtime enforces their validity. The compiler does not validate permission names.

#### When Blocks

`when` conditions are restricted to:
- Environment variable comparisons: `env.VAR == "value"` or `env.VAR != "value"`.
- Boolean literals.

Any other expression in a `when` condition emits `E123`. This restriction exists because recipes are provisioning manifests; arbitrary logic is a design anti-pattern. The `on init` block is the correct place for runtime logic.

#### On Init Block

The `on init` block is semantically a program-mode embedded snippet. Inside it:
- All `ldc.*` imports that are available in programs are available.
- `dfc.*` and `${env.*}` are not available.
- The same statement grammar applies.
- The block must not declare new top-level functions (they would be unreachable). Emit `E124` if attempted.

---

## 10. Error Handling & Diagnostics

### 10.1 Diagnostic Infrastructure

All diagnostics go through `diag/diagnostic.h`:

```cpp
enum class DiagLevel { Note, Warning, Error, Fatal };

struct Diagnostic {
    DiagLevel     level;
    std::string   code;        // e.g. "E001"
    std::string   message;
    SourceLocation location;
    std::vector<std::string> notes;
};

class DiagEngine {
public:
    void emit(Diagnostic d);
    bool has_errors() const;
    void print_all(std::ostream& out, SourceMap& smap);
private:
    std::vector<Diagnostic> diagnostics;
};
```

Errors abort compilation after the current pass completes (remaining items in the pass are still processed to surface as many errors as possible). Fatal errors abort immediately.

### 10.2 
continuation

### 10.2 Error Code Ranges

| Range | Category |
|------|----------|
| `E001–E019` | Lexical errors |
| `E020–E029` | Token/context misuse |
| `E030–E039` | Type errors |
| `E040–E049` | Import resolution |
| `E050–E059` | Name resolution |
| `E060–E079` | File‑type restrictions |
| `E080–E089` | Pattern / match analysis |
| `E090–E099` | Control‑flow correctness |
| `E100–E109` | Module‑specific errors |
| `E110–E119` | Program‑specific errors |
| `E120–E129` | Recipe‑specific errors |

Warnings use the same ranges prefixed with `W`.

---

### 10.3 Diagnostic Formatting

Diagnostics must follow a consistent human‑readable format.

Example:

error[E031]: assignment to immutable binding
  --> bot.dfyp:42:9
   |
42 |     count = count + 1
   |     ^^^^^
   |
note: variable `count` was declared without `mutable`
  --> bot.dfyp:10:5


Rules:

- `error[...]` or `warning[...]`
- `file:line:column`
- code snippet with caret marker
- optional notes

The compiler should keep a `SourceMap` that stores the entire source file contents to render snippets.

---

### 10.4 Recovery Strategy

Parser error recovery uses Bison’s `error` token.

Example:

```bison
stmt
    : error OP_SEMICOLON
        {
            diag_error(@$, "E200", "invalid statement");
            yyerrok;
        }
```

Recovery strategy:

1. Skip tokens until a synchronization point:
   - `;`
   - `}`
2. Resume parsing.

This allows the compiler to report multiple syntax errors in one run.

---

# 11. Appendix: Token Reference

## 11.1 Token Enum

`lexer/token.h`

```cpp
enum TokenKind {

    /* identifiers */
    IDENT,
    LIT_COMMAND,

    /* literals */
    LIT_INT,
    LIT_FLOAT,
    LIT_STRING,
    LIT_BYTES,
    LIT_VERSION,

    /* punctuation */
    LPAREN, RPAREN,
    LBRACE, RBRACE,
    LBRACKET, RBRACKET,

    OP_COMMA,
    OP_COLON,
    OP_SEMICOLON,
    OP_DOT,
    OP_DOTDOT,

    /* operators */
    OP_PLUS,
    OP_MINUS,
    OP_STAR,
    OP_SLASH,
    OP_PERCENT,

    OP_ASSIGN,
    OP_EQ,
    OP_NEQ,
    OP_LT,
    OP_GT,
    OP_LTE,
    OP_GTE,

    OP_CONCAT,
    OP_ARROW,
    OP_FATARROW,

    OP_QUESTION,
    OP_NULLCOAL,
    OP_BANG,

    /* keywords (core) */
    KW_LET,
    KW_MUTABLE,
    KW_FN,
    KW_RETURN,
    KW_IF,
    KW_ELSE,
    KW_FOR,
    KW_WHILE,
    KW_MATCH,
    KW_IN,

    KW_STRUCT,
    KW_ENUM,
    KW_TYPE,
    KW_IMPORT,
    KW_PUB,

    KW_TRUE,
    KW_FALSE,
    KW_NONE,

    KW_AND,
    KW_OR,
    KW_NOT,

    KW_TRY,
    KW_CATCH,
    KW_RAISE,

    /* module */
    KW_MODULE,
    KW_EMIT,
    KW_EXPECT,
    KW_HOOK,
    KW_EXPORTS,

    /* program */
    KW_PROGRAM,
    KW_COMMAND,
    KW_INTERCEPT,
    KW_MESSAGE,
    KW_EVERY,
    KW_AT,
    KW_TIMEZONE,
    KW_ROOM,
    KW_EVENT,

    /* recipe */
    KW_RECIPE,
    KW_SERVICE,
    KW_VERSION,
    KW_AUTHOR,
    KW_DESCRIPTION,
    KW_CONFIG,
    KW_AUTOSTART,
    KW_ROLES,
    KW_ROLE,
    KW_CAN,
    KW_CANNOT,
    KW_DEFAULT_ROLE,
    KW_WEBHOOKS,
    KW_POST,
    KW_TO,
    KW_HEADERS,
    KW_WHEN,
    KW_ON,
    KW_INIT,

    /* recipe atoms */
    KW_ATOM_ENABLED,
    KW_ATOM_DISABLED,
    KW_ATOM_STRICT,
    KW_ATOM_MODERATE,
    KW_ATOM_RELAXED,

    /* interpolation */
    INTERP_OPEN
};
```

---

# 12. Minimal End‑to‑End Example

## Source

program echo 1.0.0

import ldc.message

command "/echo" (args: [str]) {
    let text = args.join(" ")
    ldc.message.send(text)
}


---

## AST (simplified)

ProgramFile
 ├─ header(name="echo", version="1.0.0")
 ├─ imports
 │   └─ ldc.message
 └─ command "/echo"
     ├─ params
     │   └─ args: [str]
     └─ block
         ├─ let text
         │   └─ call join
         └─ call ldc.message.send


---

## Generated WASM (conceptual)

(import "ldc" "message.send" (func $ldc_message_send (param i32)))

(func $cmd_echo (param $args i32)
  (local $text i32)

  local.get $args
  i32.const <ptr_to_space_string>
  call $__str_join
  local.set $text

  local.get $text
  call $ldc_message_send
)

(export "cmd_echo" (func $cmd_echo))


---

# End of Specification (v0.1.0 draft)

