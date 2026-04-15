#pragma once
#include <string>
#include <cstdint>

namespace daffyscript {

enum class TokenKind {
    Eof,

    Ident,
    LitCommand,
    LitInt,
    LitFloat,
    LitString,
    LitBytes,
    LitVersion,

    LParen, RParen,
    LBrace, RBrace,
    LBracket, RBracket,

    Comma,
    Colon,
    Semicolon,
    Dot,
    DotDot,

    Plus, Minus, Star, Slash, Percent,

    Assign,
    Eq, Neq, Lt, Gt, Lte, Gte,

    Concat,
    Arrow, FatArrow,
    Question, NullCoal, Bang,

    KwLet, KwMutable, KwFn, KwReturn,
    KwIf, KwElse, KwFor, KwWhile,
    KwMatch, KwIn,
    KwStruct, KwEnum, KwType,
    KwImport, KwPub,
    KwTrue, KwFalse, KwNone,
    KwAnd, KwOr, KwNot,
    KwTry, KwCatch, KwRaise,

    KwModule, KwEmit, KwExpect, KwHook, KwExports,

    KwProgram, KwCommand, KwIntercept, KwMessage,
    KwEvery, KwAt, KwTimezone, KwRoom, KwEvent,

    KwRecipe, KwService, KwVersion, KwAuthor, KwDescription,
    KwConfig, KwAutostart,
    KwRoles, KwRole, KwCan, KwCannot, KwDefaultRole,
    KwWebhooks, KwPost, KwTo, KwHeaders,
    KwWhen, KwOn, KwInit,

    KwAtomEnabled, KwAtomDisabled,
    KwAtomStrict, KwAtomModerate, KwAtomRelaxed,

    InterpOpen,
};

struct SourceLocation {
    std::string file;
    std::size_t line = 1;
    std::size_t column = 1;
};

struct Token {
    TokenKind kind = TokenKind::Eof;
    std::string text;
    SourceLocation location;

    int64_t int_value = 0;
    double float_value = 0.0;
};

} // namespace daffyscript
