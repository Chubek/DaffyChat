#pragma once
#include <string>
#include <cstdint>

namespace dssl {

enum class TokenKind {
    Eof,

    Ident,
    LitString,
    LitInt,
    LitFloat,
    LitVersion,

    LParen, RParen,
    LBrace, RBrace,
    LBracket, RBracket,
    LAngle, RAngle,

    Comma,
    Colon,
    Semicolon,
    Dot,
    Arrow,
    FatArrow,
    Question,
    Equals,
    Pipe,
    Ampersand,
    At,
    Hash,

    KwService,
    KwImport,
    KwStruct,
    KwEnum,
    KwUnion,
    KwRpc,
    KwRest,
    KwGet,
    KwPost,
    KwPut,
    KwPatch,
    KwDelete,
    KwExec,
    KwMeta,
    KwDoc,
    KwDeprecated,
    KwOptional,
    KwRepeated,
    KwMap,
    KwOneof,
    KwReturns,
    KwStream,
    KwConst,
    KwDefault,

    KwString,
    KwInt32,
    KwInt64,
    KwUint32,
    KwUint64,
    KwFloat32,
    KwFloat64,
    KwBool,
    KwBytes,
    KwTimestamp,
    KwDuration,
    KwVoid,

    KwTrue,
    KwFalse,
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

inline const char* TokenKindName(TokenKind k) {
    switch (k) {
#define CASE(x) case TokenKind::x: return #x
        CASE(Eof); CASE(Ident); CASE(LitString); CASE(LitInt);
        CASE(LitFloat); CASE(LitVersion); CASE(LParen); CASE(RParen);
        CASE(LBrace); CASE(RBrace); CASE(LBracket); CASE(RBracket);
        CASE(LAngle); CASE(RAngle); CASE(Comma); CASE(Colon);
        CASE(Semicolon); CASE(Dot); CASE(Arrow); CASE(FatArrow);
        CASE(Question); CASE(Equals); CASE(Pipe); CASE(Ampersand);
        CASE(At); CASE(Hash); default: return "Unknown";
#undef CASE
    }
}

} // namespace dssl
