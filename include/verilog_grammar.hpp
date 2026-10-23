#pragma once
// The kernlist grammar in PEGTL, following doc/KERNLIST_format.md and doc/kernlist.bnf rule by
// rule. Every token rule consumes the white space and comments after it; the file starts with
// skipping them. Rules named in kernlist_reader.cpp's selector become nodes of the parse tree.
#include <tao/pegtl.hpp>

namespace verilog {
  namespace grammar {
    using namespace tao::pegtl;

    // ---- white space and comments (only // and /* */)
    struct line_comment : seq< two<'/'>, until< eolf > > {};
    struct block_comment_end : string<'*','/'> {};
    struct block_comment : if_must< string<'/','*'>, until< block_comment_end > > {};
    struct sp : star< sor< plus< space >, line_comment, block_comment > > {};

    // ---- identifiers: normal or escaped; keywords are not identifiers
    struct ident_first : ranges<'a','z','A','Z','_','_'> {};
    struct ident_other : ranges<'a','z','A','Z','0','9','_','_','$','$'> {};
    struct keyword : seq< sor< TAO_PEGTL_STRING("kernlist_format"), TAO_PEGTL_STRING("endmodule"),
                               TAO_PEGTL_STRING("module"), TAO_PEGTL_STRING("input"),
                               TAO_PEGTL_STRING("output"), TAO_PEGTL_STRING("wire"),
                               TAO_PEGTL_STRING("assign"), TAO_PEGTL_STRING("invariant"),
                               TAO_PEGTL_STRING("variant"), TAO_PEGTL_STRING("model") >,
                          not_at< ident_other > > {};
    struct ident_normal : seq< not_at< keyword >, ident_first, star< ident_other > > {};
    // Verilog: a backslash, then any printable characters, ended by white space
    struct ident_escaped : seq< one<'\\'>, plus< not_one<' ','\t','\r','\n'> > > {};
    struct identifier : sor< ident_escaped, ident_normal > {};

    // Names in their roles (the parse tree keeps these)
    struct module_name : identifier {};
    struct port_name : identifier {};
    struct declared_name : identifier {};
    struct type_name : identifier {};
    struct parameter_name : identifier {};
    struct variant_name : identifier {};
    struct instance_name : identifier {};
    struct connection_port : identifier {};
    struct invariant_name : identifier {};
    struct model_instance : identifier {};
    struct model_name : identifier {};
    struct signal_name : identifier {};

    template< typename R > struct token : seq< R, sp > {};

    // ---- punctuation
    struct semi : token< one<';'> > {};
    struct comma : token< one<','> > {};
    struct lparen : token< one<'('> > {};
    struct rparen : token< one<')'> > {};
    struct lbrack : token< one<'['> > {};
    struct rbrack : token< one<']'> > {};
    struct lbrace : token< one<'{'> > {};
    struct rbrace : token< one<'}'> > {};
    struct colon : token< one<':'> > {};
    struct dot : token< one<'.'> > {};
    struct hash : token< one<'#'> > {};
    struct qmark : token< one<'?'> > {};
    struct equals : token< seq< one<'='>, not_at< one<'='> > > > {};

    // ---- keywords
    template< typename S > struct kw : seq< S, not_at< ident_other >, sp > {};
    struct kw_format : kw< TAO_PEGTL_STRING("kernlist_format") > {};
    struct kw_module : kw< TAO_PEGTL_STRING("module") > {};
    struct kw_endmodule : kw< TAO_PEGTL_STRING("endmodule") > {};
    struct kw_input : kw< TAO_PEGTL_STRING("input") > {};
    struct kw_output : kw< TAO_PEGTL_STRING("output") > {};
    struct kw_wire : kw< TAO_PEGTL_STRING("wire") > {};
    struct kw_assign : kw< TAO_PEGTL_STRING("assign") > {};
    struct kw_variant : kw< TAO_PEGTL_STRING("variant") > {};
    struct kw_invariant : kw< TAO_PEGTL_STRING("invariant") > {};
    struct kw_model : kw< TAO_PEGTL_STRING("model") > {};

    // ---- numbers
    struct decimal : plus< digit > {};
    struct sized_number : seq< decimal, one<'\''>, one<'b','B','o','O','d','D','h','H'>,
                               plus< sor< xdigit, one<'x','X','z','Z','_'> > > > {};
    struct unsized_number : seq< decimal, not_at< one<'\''> > > {};
    struct format_version : decimal {};
    struct range_bound : decimal {};
    struct select_bound : decimal {};
    struct parameter_value : decimal {};
    struct replication_count : decimal {};

    // ---- selects and ranges: [i] or [msb:lsb], constant indices
    struct select : if_must< lbrack, token< select_bound >, opt< colon, must< token< select_bound > > >, rbrack > {};
    struct range : if_must< lbrack, token< range_bound >, colon, token< range_bound >, rbrack > {};

    // ---- expressions (KL13: one language everywhere)
    // Precedence, lowest first:  ->  ?:  |  ^  &  == !=  < <= > >=  + -  unary  primary
    struct expression;
    struct op_implies : token< string<'-','>'> > {};
    struct op_or : token< seq< one<'|'>, not_at< one<'|'> > > > {};
    struct op_xor : token< one<'^'> > {};
    struct op_and : token< seq< one<'&'>, not_at< one<'&'> > > > {};
    struct op_equality : token< sor< string<'=','='>, string<'!','='> > > {};
    struct op_ordered : token< sor< string<'<','='>, string<'>','='>,
                                     seq< one<'<'>, not_at< one<'<'> > >,
                                     seq< one<'>'>, not_at< one<'>'> > > > > {};
    struct op_sum : token< sor< one<'+'>, seq< one<'-'>, not_at< one<'>'> > > > > {};
    struct op_unary : token< sor< one<'~'>, seq< one<'&'>, not_at< one<'&'> > >,
                                   seq< one<'|'>, not_at< one<'|'> > >, one<'^'> > > {};

    struct name_ref : seq< token< signal_name >, opt< select > > {};
    struct model_ref : if_must< kw_model, lparen, token< model_instance >, dot, token< model_name >, opt< select >, rparen > {};
    struct expression_list : list_must< expression, comma > {};
    struct replication : seq< lbrace, token< replication_count >, lbrace, must< expression_list, rbrace, rbrace > > {};
    struct concatenation : if_must< lbrace, expression_list, rbrace > {};
    struct parenthesized : if_must< lparen, expression, rparen > {};
    struct primary : sor< model_ref, replication, concatenation, parenthesized,
                          token< sized_number >, token< unsized_number >, name_ref > {};
    struct unary_expr : seq< opt< op_unary >, primary > {};
    struct sum_expr : seq< unary_expr, star< op_sum, must< unary_expr > > > {};
    struct ordered_expr : seq< sum_expr, opt< op_ordered, must< sum_expr > > > {};
    struct equality_expr : seq< ordered_expr, opt< op_equality, must< ordered_expr > > > {};
    struct and_expr : seq< equality_expr, star< op_and, must< equality_expr > > > {};
    struct xor_expr : seq< and_expr, star< op_xor, must< and_expr > > > {};
    struct or_expr : seq< xor_expr, star< op_or, must< xor_expr > > > {};
    struct choice_expr : seq< or_expr, opt< qmark, must< choice_expr, colon, choice_expr > > > {};
    struct expression : seq< choice_expr, opt< op_implies, must< choice_expr > > > {};

    // ---- statements
    struct format_line : if_must< kw_format, token< format_version >, semi > {};

    struct port_list : list_must< token< port_name >, comma > {};
    struct direction : sor< kw_input, kw_output, kw_wire > {};
    struct declared_names : list_must< token< declared_name >, comma > {};
    struct declaration : if_must< direction, opt< range >, declared_names, semi > {};

    struct lvalue;
    struct lvalue_list : list_must< lvalue, comma > {};
    struct lvalue : sor< if_must< lbrace, lvalue_list, rbrace >, name_ref > {};
    struct assigned_expression : expression {};
    struct assignment : if_must< lvalue, equals, assigned_expression > {};
    struct assignments : list_must< assignment, comma > {};
    struct continuous_assign : if_must< kw_assign, assignments, semi > {};

    struct parameter : if_must< dot, token< parameter_name >, lparen, token< parameter_value >, rparen > {};
    struct parameter_list : list_must< parameter, comma > {};
    struct parameters : if_must< hash, lparen, parameter_list, rparen > {};
    struct variant : if_must< kw_variant, token< variant_name > > {};
    struct connection : if_must< dot, token< connection_port >, lparen, opt< expression >, rparen > {};
    struct connections_end : rparen {};
    struct instantiation : if_must< token< type_name >, opt< parameters >, opt< variant >,
                                    token< instance_name >, lparen, opt< list_must< connection, comma > >,
                                    connections_end, semi > {};

    struct invariant : if_must< kw_invariant, token< invariant_name >, colon, expression, semi > {};

    struct module_item : sor< declaration, continuous_assign, invariant, instantiation > {};
    struct module_end : kw_endmodule {};
    struct module : if_must< kw_module, token< module_name >, lparen, opt< port_list >, rparen, semi,
                             star< module_item >, module_end > {};
    struct more_modules : star< module > {};
    struct file_end : eof {};
    struct file : must< sp, format_line, module, more_modules, file_end > {};

  } // namespace grammar
} // namespace verilog
