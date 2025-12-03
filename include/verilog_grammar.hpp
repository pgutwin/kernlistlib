#pragma once
#include <tao/pegtl.hpp>

namespace verilog {
  namespace grammar {
    using namespace tao::pegtl;

    // Whitespace/comments
    struct space : sor< one<' '>, one<'\t'>, one<'\r'>, one<'\n'> > {};
    struct line_comment : if_must< string<'/','/'>, until< eolf > > {};
    struct block_comment1 : if_must< string<'/','*'>, until< string<'*','/'> > > {};
    struct block_comment2 : if_must< string<'(','*'>, until< string<'*',')'> > > {};
    struct sep  : star< sor< space, line_comment, block_comment1, block_comment2 > > {};
    struct seps : plus< sor< space, line_comment, block_comment1, block_comment2 > > {};
    template<char C>
    struct sym : tao::pegtl::seq< sep, tao::pegtl::one<C>, sep > {};

    // Identifiers
    struct ident_start : sor< alpha, one<'_'> > {};
    struct ident_more  : sor< alnum, one<'_','$'> > {};
    struct ident_norm  : seq< ident_start, star< ident_more > > {};
    struct ident_esc   : seq< one<'\\'>, plus< not_one<' ','\t','\r','\n','[',']','{','}','(',')','.',',',';','='> > > {};

    // Numbers
    struct DECDIG : ranges<'0','9'> {};  // Decimal digits only
    // struct HEXDIG : ranges<'0','9','a','f','A','F'> {};
    struct HEXDIG : ranges<'0','9','a','f','A','F','x','z','X','Z'> {};
    struct unsigned_hex_str : plus< HEXDIG > {};
    struct sign : one<'+','-'> {};
    struct signed_hex_str : seq< sign, unsigned_hex_str > {};
    struct base_char : one<'b','B','h','H','o','O','d','D'> {};
    struct base : seq< one<'\''>, base_char > {};
    // struct number_1 : sor<
    //   unsigned_hex_str,
    //   signed_hex_str,
    //   seq< unsigned_hex_str, base, unsigned_hex_str >,
    //   seq< base, unsigned_hex_str >
    //   > {};

    // Match numbers WITH base notation first (these are longer/more specific)
    struct sized_number : seq< unsigned_hex_str, base, unsigned_hex_str > {};
    struct unsized_number : seq< base, unsigned_hex_str > {};
    struct signed_number : signed_hex_str {};
    struct plain_number : plus< DECDIG > {};  // Plain numbers are decimal only

    struct number_1 : sor<
      sized_number,     // Match "2'hx" pattern first
      unsized_number,   // Match "'hx" pattern  
      signed_number,    // Match "+123" or "-456"
      plain_number      // Match plain "123" last
      > {};

    // struct number_1 : sor<
    //   seq< unsigned_hex_str >,
    //   seq< base, unsigned_hex_str >,
    //   signed_hex_str,
    //   unsigned_hex_str
    //   > {};

    // Ranges
    struct lbrack : sym<'['> {}; struct rbrack : sym<']'> {}; struct colon : sym<':'> {};
    // --- allow [idx] and [msb:lsb] after identifiers ---

    // If expression is defined later in the file, leave this forward declare:
    struct expression;

    struct identifier_raw : tao::pegtl::sor< ident_esc, ident_norm > {};
    // --- identifier selects: allow [idx] and [msb:lsb] in expressions ---
    struct bit_select      : tao::pegtl::seq< lbrack, number_1, rbrack > {};
    struct range_slice     : tao::pegtl::seq< lbrack, number_1, sep, colon, sep, number_1, rbrack > {};
    struct range_or_select : tao::pegtl::sor< range_slice, bit_select > {};


    // zero-or-more selects lets you do foo[3], foo[7:0], foo[i][j], etc.
    struct identifier : tao::pegtl::seq< identifier_raw, tao::pegtl::star< range_or_select > > {};

    // header_port_ident is used in module port lists - it's just an identifier
    struct header_port_ident : identifier {};

    // Expr (subset)
    struct lbrace : sym<'{'> {}; struct rbrace : sym<'}'> {};
    struct lparen : sym<'('> {}; struct rparen : sym<')'> {};
    struct dot    : sym<'.'> {}; struct comma  : sym<','> {};
    struct semi   : sym<';'> {}; struct equal  : sym<'='> {};
    struct expression;
    struct concat_list;
    struct concat : seq< lbrace, sep, concat_list, sep, rbrace > {};
    // identifier already supports zero-or-more selects: foo[3], foo[7:0], foo[i][j]
    // struct primary_expr : sor< concat, identifier > {};
    struct primary_expr : sor< concat, number_1, identifier > {};
    struct expression : primary_expr {};
    struct concat_list : list_must< expression, seq< sep, comma, sep > > {};

    // Ports in header
    // struct port_list : list< identifier, seq< sep, comma, sep > > {};
    struct port_list : tao::pegtl::list< header_port_ident, tao::pegtl::seq< sep, comma, sep > > {};

    // Keywords
    struct kw_wire   : TAO_PEGTL_STRING("wire") {};
    // ===== Declarations: width BEFORE name (e.g. "output [7:0] bus, a;") =====
    struct range_decl     : tao::pegtl::seq< lbrack, number_1, sep, colon, sep, number_1, rbrack > {};
    struct opt_range_decl : tao::pegtl::opt< tao::pegtl::seq< sep, range_decl, sep > > {};
    struct variable_name  : identifier_raw {};  // NOTE: raw, no post-selects
    struct list_of_variables
      : tao::pegtl::seq<
	opt_range_decl,
	tao::pegtl::list_must< variable_name, tao::pegtl::seq< sep, comma, sep > >
	> {};
    struct kw_input  : TAO_PEGTL_STRING("input") {};
    struct kw_output : TAO_PEGTL_STRING("output") {};
    struct kw_inout  : TAO_PEGTL_STRING("inout") {};
    struct kw_assign : TAO_PEGTL_STRING("assign") {};
    struct kw_module : TAO_PEGTL_STRING("module") {};
    struct kw_endmodule : TAO_PEGTL_STRING("endmodule") {};
    struct not_keyword : not_at< sor< kw_wire, kw_input, kw_output, kw_inout, kw_assign, kw_module, kw_endmodule > > {};

    // One or more names, separated by commas.
    struct net_declaration    : if_must< kw_wire,   seps, list_of_variables, sep, semi > {};
    struct input_declaration  : if_must< kw_input,  seps, list_of_variables, sep, semi > {};
    struct output_declaration : if_must< kw_output, seps, list_of_variables, sep, semi > {};
    struct inout_declaration  : if_must< kw_inout,  seps, list_of_variables, sep, semi > {};

    // assign
    struct assignment : if_must< expression, sep, equal, sep, expression > {};
    struct assignment_list : list_must< assignment, seq< sep, comma, sep > > {};
    struct continuous_assign : if_must< kw_assign, seps, assignment_list, sep, semi > {};

    // instantiation
    struct module_port_connection : expression {};
    // capture the named port name BEFORE parsing the expression
    struct named_port_name : identifier {};
    struct named_port_connection : if_must< dot, named_port_name, sep, lparen, sep, expression, sep, rparen > {};
    struct list_of_module_connections : list_must< sor< named_port_connection, module_port_connection >, seq< sep, comma, sep > > {};
    // split instance name out so actions can grab it before ports overwrite last_identifier
    struct instance_name_tok : identifier {};
    struct module_instance : if_must< instance_name_tok, sep, lparen, sep, opt< list_of_module_connections >, sep, rparen > {};
    struct module_instance_list : list_must< module_instance, seq< sep, comma, sep > > {};
    struct module_inst_head : identifier {};
    struct module_instantiation : if_must< not_keyword, module_inst_head, seps, module_instance_list, sep, semi > {};

    // module
    struct module_header_ports : if_must< lparen, sep, opt< port_list >, sep, rparen > {};
    struct module_name_tok : identifier {};
    struct module_header : if_must< kw_module, seps, module_name_tok, sep, opt< module_header_ports >, sep, semi > {};
    struct module_item : sor< input_declaration, output_declaration, inout_declaration, net_declaration, continuous_assign, module_instantiation > {};
    struct module : seq< module_header, star< seq< sep, module_item > >, sep, kw_endmodule > {};
    struct start : star< seq< sep, module, sep > > {};

  }
} // namespace verilog::grammar
