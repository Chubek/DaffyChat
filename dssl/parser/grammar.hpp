#pragma once

#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/analyze.hpp>

namespace dssl::grammar {

namespace pegtl = tao::pegtl;

struct line_comment : pegtl::seq<pegtl::string<'/', '/'>, pegtl::not_at<pegtl::one<'/'> >, pegtl::until<pegtl::eolf>> {};
struct block_comment : pegtl::seq<pegtl::string<'/','*'>, pegtl::until<pegtl::string<'*','/'>>> {};
struct ws : pegtl::sor<pegtl::space, line_comment, block_comment> {};
struct _ : pegtl::star<ws> {};
struct __ : pegtl::plus<ws> {};

struct kw_service    : pegtl::keyword<'s','e','r','v','i','c','e'> {};
struct kw_import     : pegtl::keyword<'i','m','p','o','r','t'> {};
struct kw_struct     : pegtl::keyword<'s','t','r','u','c','t'> {};
struct kw_enum       : pegtl::keyword<'e','n','u','m'> {};
struct kw_union      : pegtl::keyword<'u','n','i','o','n'> {};
struct kw_rpc        : pegtl::keyword<'r','p','c'> {};
struct kw_rest       : pegtl::keyword<'r','e','s','t'> {};
struct kw_exec       : pegtl::keyword<'e','x','e','c'> {};
struct kw_meta       : pegtl::keyword<'m','e','t','a'> {};
struct kw_deprecated : pegtl::keyword<'d','e','p','r','e','c','a','t','e','d'> {};
struct kw_optional   : pegtl::keyword<'o','p','t','i','o','n','a','l'> {};
struct kw_repeated   : pegtl::keyword<'r','e','p','e','a','t','e','d'> {};
struct kw_map        : pegtl::keyword<'m','a','p'> {};
struct kw_oneof      : pegtl::keyword<'o','n','e','o','f'> {};
struct kw_returns    : pegtl::keyword<'r','e','t','u','r','n','s'> {};
struct kw_stream     : pegtl::keyword<'s','t','r','e','a','m'> {};
struct kw_const      : pegtl::keyword<'c','o','n','s','t'> {};
struct kw_default    : pegtl::keyword<'d','e','f','a','u','l','t'> {};
struct kw_get        : pegtl::keyword<'G','E','T'> {};
struct kw_post       : pegtl::keyword<'P','O','S','T'> {};
struct kw_put        : pegtl::keyword<'P','U','T'> {};
struct kw_patch_meth : pegtl::keyword<'P','A','T','C','H'> {};
struct kw_delete     : pegtl::keyword<'D','E','L','E','T','E'> {};
struct kw_true       : pegtl::keyword<'t','r','u','e'> {};
struct kw_false      : pegtl::keyword<'f','a','l','s','e'> {};

struct kw_string    : pegtl::keyword<'s','t','r','i','n','g'> {};
struct kw_int32     : pegtl::keyword<'i','n','t','3','2'> {};
struct kw_int64     : pegtl::keyword<'i','n','t','6','4'> {};
struct kw_uint32    : pegtl::keyword<'u','i','n','t','3','2'> {};
struct kw_uint64    : pegtl::keyword<'u','i','n','t','6','4'> {};
struct kw_float32   : pegtl::keyword<'f','l','o','a','t','3','2'> {};
struct kw_float64   : pegtl::keyword<'f','l','o','a','t','6','4'> {};
struct kw_bool      : pegtl::keyword<'b','o','o','l'> {};
struct kw_bytes     : pegtl::keyword<'b','y','t','e','s'> {};
struct kw_timestamp : pegtl::keyword<'t','i','m','e','s','t','a','m','p'> {};
struct kw_duration  : pegtl::keyword<'d','u','r','a','t','i','o','n'> {};
struct kw_void      : pegtl::keyword<'v','o','i','d'> {};

struct all_keywords : pegtl::sor<
    kw_service, kw_import, kw_struct, kw_enum, kw_union, kw_rpc,
    kw_rest, kw_exec, kw_meta, kw_deprecated, kw_optional,
    kw_repeated, kw_map, kw_oneof, kw_returns, kw_stream,
    kw_const, kw_default, kw_get, kw_post, kw_put, kw_patch_meth,
    kw_delete, kw_true, kw_false,
    kw_string, kw_int32, kw_int64, kw_uint32, kw_uint64,
    kw_float32, kw_float64, kw_bool, kw_bytes, kw_timestamp,
    kw_duration, kw_void> {};

struct ident_start : pegtl::sor<pegtl::alpha, pegtl::one<'_'>> {};
struct ident_char  : pegtl::sor<pegtl::alnum, pegtl::one<'_'>> {};
struct identifier  : pegtl::seq<pegtl::not_at<pegtl::seq<all_keywords, pegtl::not_at<ident_char>>>, ident_start, pegtl::star<ident_char>> {};

struct escaped_char : pegtl::seq<pegtl::one<'\\'>, pegtl::any> {};
struct string_char  : pegtl::sor<escaped_char, pegtl::not_one<'"', '\\'>> {};
struct string_lit   : pegtl::seq<pegtl::one<'"'>, pegtl::star<string_char>, pegtl::one<'"'>> {};

struct digits      : pegtl::plus<pegtl::digit> {};
struct integer_lit : pegtl::seq<pegtl::opt<pegtl::one<'-'>>, digits> {};
struct float_lit   : pegtl::seq<pegtl::opt<pegtl::one<'-'>>, digits, pegtl::one<'.'>, digits> {};
struct version_lit : pegtl::seq<digits, pegtl::one<'.'>, digits, pegtl::one<'.'>, digits> {};

struct docstring : pegtl::seq<pegtl::string<'/', '/', '/'>, pegtl::until<pegtl::eolf>> {};
struct doc_block : pegtl::plus<pegtl::seq<_, docstring>> {};

struct builtin_type : pegtl::sor<
    kw_string, kw_int32, kw_int64, kw_uint32, kw_uint64,
    kw_float32, kw_float64, kw_bool, kw_bytes, kw_timestamp,
    kw_duration, kw_void> {};

struct type_ref;
struct optional_type : pegtl::seq<kw_optional, _, pegtl::one<'<'>, _, type_ref, _, pegtl::one<'>'>> {};
struct repeated_type : pegtl::seq<kw_repeated, _, pegtl::one<'<'>, _, type_ref, _, pegtl::one<'>'>> {};
struct map_type      : pegtl::seq<kw_map, _, pegtl::one<'<'>, _, type_ref, _, pegtl::one<','>, _, type_ref, _, pegtl::one<'>'>> {};
struct stream_type   : pegtl::seq<kw_stream, _, pegtl::one<'<'>, _, type_ref, _, pegtl::one<'>'>> {};
struct named_type    : pegtl::seq<identifier> {};
struct type_ref      : pegtl::sor<optional_type, repeated_type, map_type, stream_type, builtin_type, named_type> {};

struct field_number  : pegtl::seq<pegtl::one<'='>, _, integer_lit> {};
struct default_value : pegtl::seq<_, pegtl::one<'['>, _, kw_default, _, pegtl::one<'='>, _, pegtl::sor<string_lit, float_lit, integer_lit, kw_true, kw_false>, _, pegtl::one<']'>> {};
struct field_decl    : pegtl::seq<pegtl::opt<doc_block>, _, pegtl::opt<kw_deprecated>, _, identifier, _, pegtl::one<':'>, _, type_ref, pegtl::opt<pegtl::seq<_, field_number>>, pegtl::opt<default_value>, _, pegtl::one<';'>> {};

struct struct_body : pegtl::seq<pegtl::one<'{'>, _, pegtl::star<field_decl>, _, pegtl::one<'}'>> {};
struct struct_name : pegtl::seq<identifier> {};
struct struct_decl : pegtl::seq<pegtl::opt<doc_block>, _, kw_struct, __, struct_name, _, struct_body> {};

struct enum_name     : pegtl::seq<identifier> {};
struct enum_variant  : pegtl::seq<pegtl::opt<doc_block>, _, identifier, pegtl::opt<pegtl::seq<_, pegtl::one<'='>, _, integer_lit>>, _, pegtl::one<';'>> {};
struct enum_body     : pegtl::seq<pegtl::one<'{'>, _, pegtl::star<enum_variant>, _, pegtl::one<'}'>> {};
struct enum_decl     : pegtl::seq<pegtl::opt<doc_block>, _, kw_enum, __, enum_name, _, enum_body> {};

struct union_name    : pegtl::seq<identifier> {};
struct union_variant : pegtl::seq<pegtl::opt<doc_block>, _, identifier, _, pegtl::one<':'>, _, type_ref, _, pegtl::one<';'>> {};
struct union_body    : pegtl::seq<pegtl::one<'{'>, _, pegtl::star<union_variant>, _, pegtl::one<'}'>> {};
struct union_decl    : pegtl::seq<pegtl::opt<doc_block>, _, kw_union, __, union_name, _, union_body> {};

struct rpc_param     : pegtl::seq<identifier, _, pegtl::one<':'>, _, type_ref> {};
struct rpc_params    : pegtl::seq<pegtl::one<'('>, _, pegtl::opt<pegtl::list<rpc_param, pegtl::one<','>, ws>>, _, pegtl::one<')'>> {};
struct rpc_return    : pegtl::seq<kw_returns, _, type_ref> {};
struct rpc_decl      : pegtl::seq<pegtl::opt<doc_block>, _, pegtl::opt<kw_deprecated>, _, kw_rpc, __, identifier, _, rpc_params, _, pegtl::opt<rpc_return>, _, pegtl::one<';'>> {};

struct http_method   : pegtl::sor<kw_get, kw_post, kw_put, kw_patch_meth, kw_delete> {};
struct url_path      : pegtl::seq<string_lit> {};
struct rest_decl     : pegtl::seq<pegtl::opt<doc_block>, _, kw_rest, __, http_method, __, url_path, _, rpc_params, _, pegtl::opt<rpc_return>, _, pegtl::one<';'>> {};

struct exec_command  : pegtl::seq<string_lit> {};
struct exec_decl     : pegtl::seq<pegtl::opt<doc_block>, _, kw_exec, __, identifier, _, exec_command, _, pegtl::one<';'>> {};

struct const_value   : pegtl::sor<string_lit, float_lit, integer_lit, kw_true, kw_false> {};  
struct const_decl    : pegtl::seq<pegtl::opt<doc_block>, _, kw_const, __, identifier, _, pegtl::one<':'>, _, type_ref, _, pegtl::one<'='>, _, const_value, _, pegtl::one<';'>> {};

struct meta_field    : pegtl::seq<_, identifier, _, pegtl::one<':'>, _, pegtl::sor<string_lit, integer_lit, kw_true, kw_false>, _, pegtl::one<';'>> {};
struct meta_body     : pegtl::seq<pegtl::one<'{'>, _, pegtl::star<meta_field>, _, pegtl::one<'}'>> {};
struct meta_decl     : pegtl::seq<kw_meta, _, meta_body> {};

struct dotted_ident  : pegtl::list<identifier, pegtl::one<'.'>> {};
struct import_decl   : pegtl::seq<kw_import, __, dotted_ident, _, pegtl::one<';'>> {};

struct service_decl    : pegtl::seq<pegtl::opt<doc_block>, _, kw_service, __, identifier, __, version_lit, _, pegtl::one<';'>> {};

struct top_level_item : pegtl::sor<struct_decl, enum_decl, union_decl, rpc_decl, rest_decl, exec_decl, const_decl, meta_decl> {};

struct dssl_file : pegtl::seq<_, pegtl::opt<service_decl>, _, pegtl::star<import_decl>, _, pegtl::star<pegtl::seq<top_level_item, _>>, _, pegtl::eof> {};

} // namespace dssl::grammar
