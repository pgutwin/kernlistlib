// Reading a kernlist file (doc/KERNLIST_format.md): the grammar (verilog_grammar.hpp) builds a
// parse tree, which is converted to the data model (veriloglib.hpp). A syntax error stops reading
// and is reported with its line and column (KL14).
#include "veriloglib.hpp"
#include "verilog_grammar.hpp"
#include "verilog_writer.hpp"
#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/parse_tree.hpp>
#include <fstream>
#include <cctype>
#include <memory>

namespace verilog {

  // ==========================================================================
  // Numbers and ranges
  // ==========================================================================

  static int digit_to_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return 0;
  }

  int64_t Number::as_integer() const {
    int base_v = 10;
    if (base.has_value()) {
      switch (std::tolower(static_cast<unsigned char>(base.value()))) {
      case 'h': base_v = 16; break;
      case 'b': base_v = 2;  break;
      case 'd': base_v = 10; break;
      case 'o': base_v = 8;  break;
      default: base_v = 10;  break;
      }
    }
    int64_t v = 0; bool neg = false;
    for (size_t i = 0; i < mantissa.size(); ++i) {
      char c = mantissa[i];
      if (i == 0 && (c == '+' || c == '-')) { neg = (c == '-'); continue; }
      if (c == '_') continue;
      v = v * base_v + digit_to_val(c);
    }
    return neg ? -v : v;
  }

  std::vector<int64_t> Range::to_indices() const {
    int64_t s = start.as_integer();
    int64_t e = end.as_integer();
    std::vector<int64_t> out;
    if (s >= e) { for (int64_t i = s; i >= e; --i) out.push_back(i); }
    else { for (int64_t i = e; i >= s; --i) out.push_back(i); }
    return out;
  }

  std::string expr_to_string(const Expr& e) { return write_expr(e); }

  std::string Module::summary() const {
    std::ostringstream oss;
    oss << "module " << module_name << "(";
    for (size_t i = 0; i < port_list.size(); ++i) { if (i) oss << ", "; oss << port_list[i]; }
    oss << ");\n";
    oss << "  inputs:     " << input_declarations.size() << "\n";
    oss << "  outputs:    " << output_declarations.size() << "\n";
    oss << "  wires:      " << net_declarations.size() << "\n";
    oss << "  assigns:    " << assignments.size() << "\n";
    oss << "  instances:  " << module_instances.size() << "\n";
    oss << "  invariants: " << invariants.size() << "\n";
    oss << "endmodule\n";
    return oss.str();
  }

  // ==========================================================================
  // Error messages for syntax errors: one per rule that the grammar requires
  // ==========================================================================

  namespace reader {
    using namespace grammar;
    namespace pegtl = tao::pegtl;

    template< typename Rule > inline constexpr const char* message = nullptr;
#define KL_MESSAGE(RULE, TEXT) template<> inline constexpr const char* message< RULE > = TEXT
    KL_MESSAGE(format_line, "expected the format line, kernlist_format 1;, before anything else");
    KL_MESSAGE(token< format_version >, "expected the format version, a decimal number");
    KL_MESSAGE(module, "expected a module");
    KL_MESSAGE(file_end, "expected 'module' or the end of the file");
    KL_MESSAGE(pegtl::until< block_comment_end >, "a /* comment is not closed");
    KL_MESSAGE(semi, "expected ';'");
    KL_MESSAGE(lparen, "expected '('");
    KL_MESSAGE(rparen, "expected ')'");
    KL_MESSAGE(rbrack, "expected ']'");
    KL_MESSAGE(rbrace, "expected '}'");
    KL_MESSAGE(colon, "expected ':'");
    KL_MESSAGE(dot, "expected '.'");
    KL_MESSAGE(equals, "expected '='");
    KL_MESSAGE(token< select_bound >, "expected an index, a decimal number");
    KL_MESSAGE(token< range_bound >, "expected a decimal number");
    KL_MESSAGE(token< module_name >, "expected the module's name");
    KL_MESSAGE(token< port_name >, "expected a port name");
    KL_MESSAGE(module_end, "expected a statement or 'endmodule'");
    KL_MESSAGE(declared_names, "expected a name to declare");
    KL_MESSAGE(token< declared_name >, "expected a name to declare");
    KL_MESSAGE(lvalue_list, "expected a name to assign to");
    KL_MESSAGE(lvalue, "expected a name to assign to");
    KL_MESSAGE(assignments, "expected an assignment");
    KL_MESSAGE(assignment, "expected an assignment");
    KL_MESSAGE(assigned_expression, "expected an expression");
    KL_MESSAGE(expression, "expected an expression");
    KL_MESSAGE(expression_list, "expected an expression");
    KL_MESSAGE(choice_expr, "expected an expression");
    KL_MESSAGE(or_expr, "expected an operand");
    KL_MESSAGE(xor_expr, "expected an operand");
    KL_MESSAGE(and_expr, "expected an operand");
    KL_MESSAGE(equality_expr, "expected an operand");
    KL_MESSAGE(ordered_expr, "expected an operand");
    KL_MESSAGE(sum_expr, "expected an operand");
    KL_MESSAGE(unary_expr, "expected an operand");
    KL_MESSAGE(token< model_instance >, "expected an instance name");
    KL_MESSAGE(token< model_name >, "expected the name of a model value");
    KL_MESSAGE(parameter_list, "expected a parameter by name, as .W(8)");
    KL_MESSAGE(parameter, "expected a parameter by name, as .W(8)");
    KL_MESSAGE(token< parameter_name >, "expected a parameter name");
    KL_MESSAGE(token< parameter_value >, "expected the parameter's value, a decimal number");
    KL_MESSAGE(token< variant_name >, "expected the variant's name");
    KL_MESSAGE(token< instance_name >, "expected the instance name");
    KL_MESSAGE(connection, "expected a connection by name, as .port(signal)");
    KL_MESSAGE(connections_end, "expected a connection by name, as .port(signal), or ')'");
    KL_MESSAGE(token< connection_port >, "expected a port name");
    KL_MESSAGE(token< invariant_name >, "expected the invariant's name");
#undef KL_MESSAGE

    template< typename Rule >
    struct control : pegtl::normal< Rule > {
      template< typename ParseInput, typename... States >
      [[noreturn]] static void raise(const ParseInput& in, States&&...) {
        const char* m = message< Rule >;
        throw pegtl::parse_error(m ? m : "syntax error", in);
      }
    };

    template< typename Rule >
    using selector = pegtl::parse_tree::selector< Rule,
      pegtl::parse_tree::store_content::on<
        format_version, module, module_name, port_name,
        declaration, kw_input, kw_output, kw_wire, range_bound, declared_name,
        continuous_assign, assignment, lvalue, assigned_expression,
        name_ref, signal_name, select_bound,
        instantiation, type_name, parameter, parameter_name, parameter_value, variant_name,
        instance_name, connection, connection_port,
        invariant, invariant_name,
        expression, choice_expr, or_expr, xor_expr, and_expr, equality_expr, ordered_expr,
        sum_expr, unary_expr,
        op_implies, op_or, op_xor, op_and, op_equality, op_ordered, op_sum, op_unary,
        sized_number, unsized_number, model_ref, model_instance, model_name,
        replication, replication_count, concatenation > >;

    using Node = pegtl::parse_tree::node;

    // ---- conversion of the parse tree to the data model

    std::string name_of(const Node& n) {
      std::string s = n.string();
      if (!s.empty() && s[0] == '\\') s.erase(0, 1);
      return s;
    }

    // The operator a token holds, without the white space and comments it consumed
    std::string op_of(const Node& n) {
      std::string s = n.string();
      size_t i = 0;
      while (i < s.size() && std::string_view("~&|^=!<>+-").find(s[i]) != std::string_view::npos) ++i;
      return s.substr(0, i);
    }

    Location location_of(const Node& n) {
      auto p = n.begin();
      return Location{ static_cast<int>(p.line), static_cast<int>(p.column) };
    }

    Number decimal_number(const Node& n) {
      Number num; num.mantissa = n.string(); return num;
    }

    Number sized(const Node& n) {
      std::string s = n.string();
      size_t tick = s.find('\'');
      Number num;
      num.length = std::stoi(s.substr(0, tick));
      num.base = s[tick + 1];
      num.mantissa = s.substr(tick + 2);
      return num;
    }

    Expr expr(const Node& n);

    // name_ref: signal_name [select_bound [select_bound]]
    Expr to_name_ref(const Node& n) {
      std::string name = name_of(*n.children[0]);
      if (n.children.size() == 1) return Identifier{ name };
      if (n.children.size() == 2) return IdentifierIndexed{ name, decimal_number(*n.children[1]) };
      return IdentifierSliced{ name, Range{ decimal_number(*n.children[1]), decimal_number(*n.children[2]) } };
    }

    Expr binary_chain(const Node& n) {
      Expr left = expr(*n.children[0]);
      for (size_t i = 1; i + 1 < n.children.size(); i += 2) {
        auto b = std::make_shared<BinaryOp>();
        b->op = op_of(*n.children[i]);
        b->lhs = std::move(left);
        b->rhs = expr(*n.children[i + 1]);
        left = b;
      }
      return left;
    }

    Expr expr(const Node& n) {
      if (n.is_type< expression >() || n.is_type< assigned_expression >()
          || n.is_type< or_expr >() || n.is_type< xor_expr >() || n.is_type< and_expr >()
          || n.is_type< equality_expr >() || n.is_type< ordered_expr >() || n.is_type< sum_expr >()) {
        return binary_chain(n);
      }
      if (n.is_type< choice_expr >()) {
        if (n.children.size() == 1) return expr(*n.children[0]);
        auto c = std::make_shared<Choice>();
        c->condition = expr(*n.children[0]);
        c->if_true = expr(*n.children[1]);
        c->if_false = expr(*n.children[2]);
        return c;
      }
      if (n.is_type< unary_expr >()) {
        if (n.children.size() == 1) return expr(*n.children[0]);
        auto u = std::make_shared<UnaryOp>();
        u->op = op_of(*n.children[0]);
        u->operand = expr(*n.children[1]);
        return u;
      }
      if (n.is_type< name_ref >()) return to_name_ref(n);
      if (n.is_type< sized_number >()) return sized(n);
      if (n.is_type< unsized_number >()) return decimal_number(n);
      if (n.is_type< model_ref >()) {
        ModelRef m;
        m.instance = name_of(*n.children[0]);
        m.name = name_of(*n.children[1]);
        if (n.children.size() == 3) m.index = decimal_number(*n.children[2]);
        if (n.children.size() == 4) m.range = Range{ decimal_number(*n.children[2]), decimal_number(*n.children[3]) };
        return m;
      }
      if (n.is_type< concatenation >()) {
        auto c = std::make_shared<Concatenation>();
        for (const auto& ch : n.children) c->elements.push_back(expr(*ch));
        return c;
      }
      if (n.is_type< replication >()) {
        auto r = std::make_shared<Replication>();
        r->count = decimal_number(*n.children[0]);
        for (size_t i = 1; i < n.children.size(); ++i) r->elements.push_back(expr(*n.children[i]));
        return r;
      }
      throw std::logic_error("kernlist reader: unexpected node " + std::string(n.type));
    }

    Expr to_lvalue(const Node& n) {
      const Node& first = *n.children[0];
      if (first.is_type< name_ref >()) return to_name_ref(first);
      auto c = std::make_shared<Concatenation>();
      for (const auto& ch : n.children) c->elements.push_back(to_lvalue(*ch));
      return c;
    }

    void to_declaration(const Node& n, Module& m) {
      const Node& dir = *n.children[0];
      std::optional<Range> range;
      size_t i = 1;
      if (i < n.children.size() && n.children[i]->is_type< range_bound >()) {
        range = Range{ decimal_number(*n.children[1]), decimal_number(*n.children[2]) };
        i = 3;
      }
      Location loc = location_of(n);
      for (; i < n.children.size(); ++i) {
        std::string name = name_of(*n.children[i]);
        if (dir.is_type< kw_input >()) { InputDeclaration d; d.net_name = name; d.range = range; d.location = loc; m.input_declarations.push_back(d); }
        else if (dir.is_type< kw_output >()) { OutputDeclaration d; d.net_name = name; d.range = range; d.location = loc; m.output_declarations.push_back(d); }
        else { NetDeclaration d; d.net_name = name; d.range = range; d.location = loc; m.net_declarations.push_back(d); }
      }
    }

    void to_instantiation(const Node& n, Module& m) {
      ModuleInstance inst;
      inst.location = location_of(n);
      for (const auto& ch : n.children) {
        if (ch->is_type< type_name >()) inst.module_name = name_of(*ch);
        else if (ch->is_type< parameter >()) {
          inst.parameters.push_back(Parameter{ name_of(*ch->children[0]), decimal_number(*ch->children[1]).as_integer() });
        }
        else if (ch->is_type< variant_name >()) inst.variant = name_of(*ch);
        else if (ch->is_type< instance_name >()) inst.instance_name = name_of(*ch);
        else if (ch->is_type< connection >()) {
          Connection c;
          c.port = name_of(*ch->children[0]);
          if (ch->children.size() > 1) c.expr = expr(*ch->children[1]);
          inst.connections.push_back(std::move(c));
        }
      }
      m.module_instances.push_back(std::move(inst));
    }

    Module to_module(const Node& n) {
      Module m;
      m.location = location_of(n);
      for (const auto& ch : n.children) {
        if (ch->is_type< module_name >()) m.module_name = name_of(*ch);
        else if (ch->is_type< port_name >()) m.port_list.push_back(name_of(*ch));
        else if (ch->is_type< declaration >()) to_declaration(*ch, m);
        else if (ch->is_type< continuous_assign >()) {
          ContinuousAssign ca;
          ca.location = location_of(*ch);
          for (const auto& a : ch->children) ca.assignments.emplace_back(to_lvalue(*a->children[0]), expr(*a->children[1]));
          m.assignments.push_back(std::move(ca));
        }
        else if (ch->is_type< instantiation >()) to_instantiation(*ch, m);
        else if (ch->is_type< invariant >()) {
          m.invariants.push_back(Invariant{ name_of(*ch->children[0]), expr(*ch->children[1]), location_of(*ch) });
        }
      }
      return m;
    }

    Netlist to_netlist(const Node& root) {
      Netlist nl;
      for (const auto& ch : root.children) {
        if (ch->is_type< format_version >()) nl.format_version = static_cast<int>(decimal_number(*ch).as_integer());
        else if (ch->is_type< grammar::module >()) nl.modules.push_back(to_module(*ch));
      }
      return nl;
    }

    template< typename Input >
    Netlist read(Input&& in) {
      try {
        auto root = pegtl::parse_tree::parse< grammar::file, selector, pegtl::nothing, control >(in);
        if (!root) throw parse_error("syntax error");
        return to_netlist(*root);
      } catch (const pegtl::parse_error& e) {
        const auto& p = e.positions().front();
        parse_error err(p.source + ":" + std::to_string(p.line) + ":" + std::to_string(p.column)
                        + ": " + std::string(e.message()));
        err.line = static_cast<int>(p.line);
        err.column = static_cast<int>(p.column);
        throw err;
      }
    }
  } // namespace reader

  Netlist parse_string(std::string_view text, const std::string& source) {
    tao::pegtl::memory_input in(text.data(), text.size(), source);
    return reader::read(in);
  }

  Netlist parse_file(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs) throw parse_error("could not open file: " + path);
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    return parse_string(content, path);
  }

} // namespace verilog
