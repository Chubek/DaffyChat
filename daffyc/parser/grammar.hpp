#pragma once

#include <tao/pegtl.hpp>

namespace daffyscript::grammar {

namespace pegtl = tao::pegtl;

struct line_comment : pegtl::seq<pegtl::string<'-','-'>, pegtl::until<pegtl::eolf>> {};
struct ws : pegtl::sor<pegtl::space, line_comment> {};
struct _ : pegtl::star<ws> {};
struct __ : pegtl::plus<ws> {};

struct kw_let       : pegtl::keyword<'l','e','t'> {};
struct kw_mutable   : pegtl::keyword<'m','u','t','a','b','l','e'> {};
struct kw_fn        : pegtl::keyword<'f','n'> {};
struct kw_return    : pegtl::keyword<'r','e','t','u','r','n'> {};
struct kw_if        : pegtl::keyword<'i','f'> {};
struct kw_else      : pegtl::keyword<'e','l','s','e'> {};
struct kw_for       : pegtl::keyword<'f','o','r'> {};
struct kw_while     : pegtl::keyword<'w','h','i','l','e'> {};
struct kw_match     : pegtl::keyword<'m','a','t','c','h'> {};
struct kw_in        : pegtl::keyword<'i','n'> {};
struct kw_struct    : pegtl::keyword<'s','t','r','u','c','t'> {};
struct kw_enum      : pegtl::keyword<'e','n','u','m'> {};
struct kw_type      : pegtl::keyword<'t','y','p','e'> {};
struct kw_import    : pegtl::keyword<'i','m','p','o','r','t'> {};
struct kw_pub       : pegtl::keyword<'p','u','b'> {};
struct kw_true      : pegtl::keyword<'t','r','u','e'> {};
struct kw_false     : pegtl::keyword<'f','a','l','s','e'> {};
struct kw_none      : pegtl::keyword<'n','o','n','e'> {};
struct kw_and       : pegtl::keyword<'a','n','d'> {};
struct kw_or        : pegtl::keyword<'o','r'> {};
struct kw_not       : pegtl::keyword<'n','o','t'> {};
struct kw_try       : pegtl::keyword<'t','r','y'> {};
struct kw_catch     : pegtl::keyword<'c','a','t','c','h'> {};
struct kw_raise     : pegtl::keyword<'r','a','i','s','e'> {};

struct kw_module    : pegtl::keyword<'m','o','d','u','l','e'> {};
struct kw_emit      : pegtl::keyword<'e','m','i','t'> {};
struct kw_expect    : pegtl::keyword<'e','x','p','e','c','t'> {};
struct kw_hook      : pegtl::keyword<'h','o','o','k'> {};
struct kw_exports   : pegtl::keyword<'e','x','p','o','r','t','s'> {};

struct kw_program   : pegtl::keyword<'p','r','o','g','r','a','m'> {};
struct kw_command   : pegtl::keyword<'c','o','m','m','a','n','d'> {};
struct kw_intercept : pegtl::keyword<'i','n','t','e','r','c','e','p','t'> {};
struct kw_message   : pegtl::keyword<'m','e','s','s','a','g','e'> {};
struct kw_every     : pegtl::keyword<'e','v','e','r','y'> {};
struct kw_at        : pegtl::keyword<'a','t'> {};
struct kw_timezone  : pegtl::keyword<'t','i','m','e','z','o','n','e'> {};
struct kw_room      : pegtl::keyword<'r','o','o','m'> {};
struct kw_event     : pegtl::keyword<'e','v','e','n','t'> {};

struct kw_recipe     : pegtl::keyword<'r','e','c','i','p','e'> {};
struct kw_service    : pegtl::keyword<'s','e','r','v','i','c','e'> {};
struct kw_version    : pegtl::keyword<'v','e','r','s','i','o','n'> {};
struct kw_author     : pegtl::keyword<'a','u','t','h','o','r'> {};
struct kw_description : pegtl::keyword<'d','e','s','c','r','i','p','t','i','o','n'> {};
struct kw_config     : pegtl::keyword<'c','o','n','f','i','g'> {};
struct kw_autostart  : pegtl::keyword<'a','u','t','o','s','t','a','r','t'> {};
struct kw_roles      : pegtl::keyword<'r','o','l','e','s'> {};
struct kw_role       : pegtl::keyword<'r','o','l','e'> {};
struct kw_can        : pegtl::keyword<'c','a','n'> {};
struct kw_cannot     : pegtl::keyword<'c','a','n','n','o','t'> {};
struct kw_default_role : pegtl::keyword<'d','e','f','a','u','l','t','_','r','o','l','e'> {};
struct kw_webhooks   : pegtl::keyword<'w','e','b','h','o','o','k','s'> {};
struct kw_post       : pegtl::keyword<'p','o','s','t'> {};
struct kw_to         : pegtl::keyword<'t','o'> {};
struct kw_headers    : pegtl::keyword<'h','e','a','d','e','r','s'> {};
struct kw_when       : pegtl::keyword<'w','h','e','n'> {};
struct kw_on         : pegtl::keyword<'o','n'> {};
struct kw_init       : pegtl::keyword<'i','n','i','t'> {};
struct kw_from       : pegtl::keyword<'f','r','o','m'> {};

struct kw_enabled    : pegtl::keyword<'e','n','a','b','l','e','d'> {};
struct kw_disabled   : pegtl::keyword<'d','i','s','a','b','l','e','d'> {};
struct kw_strict     : pegtl::keyword<'s','t','r','i','c','t'> {};
struct kw_moderate   : pegtl::keyword<'m','o','d','e','r','a','t','e'> {};
struct kw_relaxed    : pegtl::keyword<'r','e','l','a','x','e','d'> {};

struct ident_char  : pegtl::sor<pegtl::alnum, pegtl::one<'_'>> {};
struct ident_start : pegtl::sor<pegtl::alpha, pegtl::one<'_'>> {};
struct identifier  : pegtl::seq<ident_start, pegtl::star<ident_char>> {};

struct escaped_char : pegtl::seq<pegtl::one<'\\'>, pegtl::any> {};
struct string_char  : pegtl::sor<escaped_char, pegtl::not_one<'"', '\\'>> {};
struct string_lit   : pegtl::seq<pegtl::one<'"'>, pegtl::star<string_char>, pegtl::one<'"'>> {};

struct digits      : pegtl::plus<pegtl::digit> {};
struct integer_lit : pegtl::seq<pegtl::opt<pegtl::one<'-'>>, digits> {};
struct float_lit   : pegtl::seq<pegtl::opt<pegtl::one<'-'>>, digits, pegtl::one<'.'>, digits> {};
struct version_lit : pegtl::seq<digits, pegtl::one<'.'>, digits, pegtl::one<'.'>, digits> {};

struct time_unit   : pegtl::sor<
    pegtl::keyword<'d','a','y','s'>,
    pegtl::keyword<'h','o','u','r','s'>,
    pegtl::keyword<'m','i','n','u','t','e','s'>,
    pegtl::keyword<'s','e','c','o','n','d','s'>,
    pegtl::keyword<'m','s'>> {};
struct duration_lit : pegtl::seq<integer_lit, pegtl::one<'.'>, time_unit> {};

struct builtin_type : pegtl::sor<
    pegtl::keyword<'s','t','r'>,
    pegtl::keyword<'i','n','t'>,
    pegtl::keyword<'f','l','o','a','t'>,
    pegtl::keyword<'b','o','o','l'>,
    pegtl::keyword<'b','y','t','e','s'>> {};

struct type_ref;
struct optional_type : pegtl::seq<type_ref, pegtl::one<'?'>> {};
struct list_type     : pegtl::seq<pegtl::one<'['>, _, type_ref, _, pegtl::one<']'>> {};
struct map_type      : pegtl::seq<pegtl::one<'{'>, _, type_ref, _, pegtl::one<':'>, _, type_ref, _, pegtl::one<'}'>> {};
struct named_type    : identifier {};
struct simple_type   : pegtl::sor<builtin_type, list_type, map_type, named_type> {};
struct type_ref      : pegtl::seq<simple_type, pegtl::opt<pegtl::one<'?'>>> {};

struct dotted_name : pegtl::list<identifier, pegtl::one<'.'>> {};

struct expr;
struct block;

struct struct_init_field : pegtl::seq<identifier, _, pegtl::one<':'>, _, expr> {};
struct struct_init       : pegtl::seq<identifier, _, pegtl::one<'{'>, _, pegtl::opt<pegtl::list<struct_init_field, pegtl::one<','>, ws>>, _, pegtl::opt<pegtl::one<','>>, _, pegtl::one<'}'>> {};

struct list_literal : pegtl::seq<pegtl::one<'['>, _, pegtl::opt<pegtl::list<expr, pegtl::one<','>, ws>>, _, pegtl::opt<pegtl::one<','>>, _, pegtl::one<']'>> {};
struct map_literal  : pegtl::seq<pegtl::one<'{'>, _, pegtl::opt<pegtl::list<pegtl::seq<expr, _, pegtl::one<':'>, _, expr>, pegtl::one<','>, ws>>, _, pegtl::opt<pegtl::one<','>>, _, pegtl::one<'}'>> {};

struct call_args : pegtl::seq<pegtl::one<'('>, _, pegtl::opt<pegtl::list<expr, pegtl::one<','>, ws>>, _, pegtl::one<')'>> {};
struct index_access : pegtl::seq<pegtl::one<'['>, _, expr, _, pegtl::one<']'>> {};
struct field_access : pegtl::seq<pegtl::one<'.'>, identifier> {};
struct method_call  : pegtl::seq<pegtl::one<'.'>, identifier, call_args> {};

struct atom : pegtl::sor<
    float_lit,
    integer_lit,
    string_lit,
    kw_true, kw_false, kw_none,
    struct_init,
    list_literal,
    map_literal,
    pegtl::seq<pegtl::one<'('>, _, expr, _, pegtl::one<')'>>,
    pegtl::seq<dotted_name, call_args>,
    dotted_name> {};

struct postfix : pegtl::seq<atom, pegtl::star<pegtl::sor<method_call, field_access, index_access, call_args>>> {};

struct unary_op : pegtl::sor<pegtl::one<'-'>, kw_not> {};
struct unary    : pegtl::seq<pegtl::opt<pegtl::seq<unary_op, _>>, postfix> {};

struct mul_op : pegtl::sor<pegtl::one<'*'>, pegtl::one<'/'>, pegtl::one<'%'>> {};
struct mul_expr : pegtl::seq<unary, pegtl::star<pegtl::seq<_, mul_op, _, unary>>> {};

struct add_op : pegtl::sor<pegtl::one<'+'>, pegtl::one<'-'>, pegtl::string<'+','+'>> {};
struct add_expr : pegtl::seq<mul_expr, pegtl::star<pegtl::seq<_, add_op, _, mul_expr>>> {};

struct cmp_op : pegtl::sor<
    pegtl::string<'=','='>,
    pegtl::string<'!','='>,
    pegtl::string<'<','='>,
    pegtl::string<'>','='>,
    pegtl::one<'<'>,
    pegtl::one<'>'>> {};
struct cmp_expr : pegtl::seq<add_expr, pegtl::opt<pegtl::seq<_, cmp_op, _, add_expr>>> {};

struct and_expr : pegtl::seq<cmp_expr, pegtl::star<pegtl::seq<_, kw_and, _, cmp_expr>>> {};
struct or_expr  : pegtl::seq<and_expr, pegtl::star<pegtl::seq<_, kw_or, _, and_expr>>> {};

struct nullcoal_op : pegtl::string<'?','?'> {};
struct nullcoal_expr : pegtl::seq<or_expr, pegtl::opt<pegtl::seq<_, nullcoal_op, _, or_expr>>> {};

struct expr : nullcoal_expr {};

struct param_decl : pegtl::seq<identifier, _, pegtl::one<':'>, _, type_ref> {};
struct param_list : pegtl::seq<pegtl::one<'('>, _, pegtl::opt<pegtl::list<param_decl, pegtl::one<','>, ws>>, _, pegtl::one<')'>> {};

struct stmt;
struct block : pegtl::seq<pegtl::one<'{'>, _, pegtl::star<pegtl::seq<stmt, _>>, pegtl::one<'}'>> {};

struct let_stmt    : pegtl::seq<kw_let, _, pegtl::opt<kw_mutable>, _, identifier, pegtl::opt<pegtl::seq<_, pegtl::one<':'>, _, type_ref>>, _, pegtl::one<'='>, _, expr> {};
struct assign_stmt : pegtl::seq<dotted_name, pegtl::star<pegtl::seq<pegtl::one<'['>, _, expr, _, pegtl::one<']'>>>, _, pegtl::one<'='>, _, expr> {};
struct return_stmt : pegtl::seq<kw_return, pegtl::opt<pegtl::seq<__, expr>>> {};
struct raise_stmt  : pegtl::seq<kw_raise, __, expr> {};
struct expr_stmt   : expr {};

struct if_stmt     : pegtl::seq<kw_if, __, expr, _, block, pegtl::opt<pegtl::seq<_, kw_else, _, pegtl::sor<if_stmt, block>>>> {};
struct for_stmt    : pegtl::seq<kw_for, __, pegtl::opt<pegtl::seq<identifier, _, pegtl::one<','>, _>>, identifier, __, kw_in, __, expr, _, block> {};
struct while_stmt  : pegtl::seq<kw_while, __, expr, _, block> {};

struct match_arm   : pegtl::seq<_, expr, _, pegtl::string<'=','>'>, _, block> {};
struct match_stmt  : pegtl::seq<kw_match, __, expr, _, pegtl::one<'{'>, pegtl::star<match_arm>, _, pegtl::one<'}'>> {};

struct catch_block : pegtl::seq<kw_catch, _, identifier, _, block> {};
struct try_stmt    : pegtl::seq<kw_try, _, block, _, catch_block> {};

struct emit_fields : pegtl::seq<pegtl::one<'{'>, _, pegtl::opt<pegtl::list<pegtl::seq<identifier, _, pegtl::one<':'>, _, expr>, pegtl::one<','>, ws>>, _, pegtl::opt<pegtl::one<','>>, _, pegtl::one<'}'>> {};
struct emit_stmt   : pegtl::seq<kw_emit, __, string_lit, _, emit_fields> {};

struct stmt : pegtl::sor<let_stmt, return_stmt, raise_stmt, if_stmt, for_stmt, while_stmt, match_stmt, try_stmt, emit_stmt, assign_stmt, expr_stmt> {};

struct fn_return_type : pegtl::seq<pegtl::string<'-','>'>, _, type_ref> {};
struct fn_decl : pegtl::seq<pegtl::opt<kw_pub>, _, kw_fn, __, identifier, _, param_list, pegtl::opt<pegtl::seq<_, fn_return_type>>, _, block> {};

struct struct_field : pegtl::seq<identifier, _, pegtl::one<':'>, _, type_ref> {};
struct struct_decl  : pegtl::seq<kw_struct, __, identifier, _, pegtl::one<'{'>, _, pegtl::opt<pegtl::list<struct_field, pegtl::one<','>, ws>>, _, pegtl::opt<pegtl::one<','>>, _, pegtl::one<'}'>> {};
struct enum_decl    : pegtl::seq<kw_enum, __, identifier, _, pegtl::one<'{'>, _, pegtl::opt<pegtl::list<identifier, pegtl::one<','>, ws>>, _, pegtl::opt<pegtl::one<','>>, _, pegtl::one<'}'>> {};

struct import_path  : pegtl::seq<dotted_name, pegtl::opt<pegtl::seq<_, pegtl::one<'{'>, _, pegtl::list<identifier, pegtl::one<','>, ws>, _, pegtl::one<'}'>>>>  {};
struct import_decl  : pegtl::seq<kw_import, __, import_path> {};

struct command_decl   : pegtl::seq<kw_command, __, string_lit, _, param_list, _, block> {};
struct intercept_decl : pegtl::seq<kw_intercept, __, string_lit, _, param_list, _, block> {};
struct every_decl     : pegtl::seq<kw_every, __, integer_lit, pegtl::one<'.'>, time_unit, _, block> {};
struct at_decl        : pegtl::seq<kw_at, __, string_lit, pegtl::opt<pegtl::seq<__, kw_timezone, __, string_lit>>, _, block> {};
struct on_event_decl  : pegtl::seq<kw_on, __, kw_event, __, string_lit, _, param_list, _, block> {};
struct on_hook_decl   : pegtl::seq<kw_on, __, kw_hook, __, string_lit, _, param_list, _, block> {};
struct expect_hook    : pegtl::seq<kw_expect, __, kw_hook, __, string_lit> {};

struct export_item  : pegtl::sor<pegtl::seq<kw_on, __, kw_hook, __, string_lit>, identifier> {};
struct exports_decl : pegtl::seq<kw_exports, _, pegtl::one<'{'>, _, pegtl::opt<pegtl::list<export_item, pegtl::one<','>, ws>>, _, pegtl::opt<pegtl::one<','>>, _, pegtl::one<'}'>> {};

struct module_header  : pegtl::seq<kw_module, __, identifier> {};
struct program_header : pegtl::seq<kw_program, __, identifier> {};
struct recipe_header  : pegtl::seq<kw_recipe, __, string_lit> {};

struct version_decl : pegtl::seq<kw_version, __, version_lit> {};

struct module_item : pegtl::sor<import_decl, fn_decl, struct_decl, enum_decl, on_hook_decl, expect_hook, emit_stmt, exports_decl> {};
struct module_file : pegtl::seq<_, module_header, _, version_decl, _, pegtl::star<pegtl::seq<module_item, _>>, _, pegtl::eof> {};

struct program_item : pegtl::sor<import_decl, fn_decl, struct_decl, enum_decl, command_decl, intercept_decl, every_decl, at_decl, on_event_decl> {};
struct program_file : pegtl::seq<_, program_header, _, version_decl, _, pegtl::star<pegtl::seq<program_item, _>>, _, pegtl::eof> {};

struct recipe_kv : pegtl::seq<identifier, _, pegtl::one<':'>, _, pegtl::sor<duration_lit, float_lit, integer_lit, string_lit, kw_true, kw_false, kw_enabled, kw_disabled, kw_strict, kw_moderate, kw_relaxed, list_literal, identifier>> {};
struct recipe_block : pegtl::seq<pegtl::one<'{'>, _, pegtl::star<pegtl::seq<recipe_kv, _, pegtl::opt<pegtl::one<','>>, _>>, pegtl::one<'}'>> {};

struct room_decl : pegtl::seq<kw_room, _, recipe_block> {};

struct service_config : pegtl::seq<kw_config, _, recipe_block> {};
struct service_field  : pegtl::sor<service_config, recipe_kv> {};
struct service_decl   : pegtl::seq<kw_service, __, string_lit, _, pegtl::one<'{'>, _, pegtl::star<pegtl::seq<service_field, _, pegtl::opt<pegtl::one<','>>, _>>, pegtl::one<'}'>> {};

struct recipe_program_decl : pegtl::seq<kw_program, __, string_lit, _, pegtl::one<'{'>, _, recipe_kv, _, pegtl::one<'}'>> {};
struct recipe_module_decl  : pegtl::seq<kw_module, __, string_lit, _, pegtl::one<'{'>, _, recipe_kv, _, pegtl::one<'}'>> {};

struct permission_list : pegtl::seq<pegtl::one<'['>, _, pegtl::opt<pegtl::list<identifier, pegtl::one<','>, ws>>, _, pegtl::one<']'>> {};
struct role_can    : pegtl::seq<kw_can, _, pegtl::one<':'>, _, permission_list> {};
struct role_cannot : pegtl::seq<kw_cannot, _, pegtl::one<':'>, _, permission_list> {};
struct role_decl   : pegtl::seq<kw_role, __, string_lit, _, pegtl::one<'{'>, _, pegtl::star<pegtl::seq<pegtl::sor<role_can, role_cannot>, _>>, pegtl::one<'}'>> {};
struct default_role_decl : pegtl::seq<kw_default_role, _, pegtl::one<':'>, _, string_lit> {};
struct roles_decl  : pegtl::seq<kw_roles, _, pegtl::one<'{'>, _, pegtl::star<pegtl::seq<pegtl::sor<role_decl, default_role_decl>, _>>, pegtl::one<'}'>> {};

struct webhook_decl : pegtl::seq<kw_on, __, kw_event, __, string_lit, __, kw_post, __, kw_to, __, string_lit> {};
struct webhooks_decl : pegtl::seq<kw_webhooks, _, pegtl::one<'{'>, _, pegtl::star<pegtl::seq<webhook_decl, _>>, pegtl::one<'}'>> {};

struct env_cond : pegtl::seq<pegtl::keyword<'e','n','v'>, pegtl::one<'.'>, identifier, _, pegtl::sor<pegtl::string<'=','='>, pegtl::string<'!','='>>, _, string_lit> {};
struct when_cond : pegtl::sor<env_cond, kw_true, kw_false> {};
struct when_block : pegtl::seq<kw_when, __, when_cond, _, pegtl::one<'{'>, _, pegtl::star<pegtl::seq<pegtl::sor<service_decl, recipe_program_decl, recipe_module_decl>, _>>, pegtl::one<'}'>> {};

struct on_init_block : pegtl::seq<kw_on, __, kw_init, _, block> {};

struct recipe_item : pegtl::sor<room_decl, service_decl, recipe_program_decl, recipe_module_decl, roles_decl, webhooks_decl, when_block, on_init_block> {};
struct recipe_file : pegtl::seq<_, recipe_header, _, version_decl, _, pegtl::opt<pegtl::seq<kw_author, __, string_lit, _>>, pegtl::opt<pegtl::seq<kw_description, __, string_lit, _>>, pegtl::star<pegtl::seq<recipe_item, _>>, _, pegtl::eof> {};

struct any_file : pegtl::sor<module_file, program_file, recipe_file> {};

} // namespace daffyscript::grammar
