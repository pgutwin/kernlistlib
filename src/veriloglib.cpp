#include "veriloglib.hpp"
#include "verilog_grammar.hpp"
#include "verilog_actions.hpp"
#include <tao/pegtl.hpp>
#include <fstream>
#include <cctype>
#include <memory>

using namespace tao::pegtl;

namespace verilog {

  static int digit_to_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return 0;
  }

  int64_t Number::as_integer() const {
    int base_v = 10;
    if (base.has_value()) {
      switch(std::tolower(base.value())) {
      case 'h': base_v = 16; break;
      case 'b': base_v = 2;  break;
      case 'd': base_v = 10; break;
      case 'o': base_v = 8;  break;
      default: base_v = 10;  break;
      }
    }
    int64_t v = 0; bool neg = false;
    for (size_t i=0;i<mantissa.size();++i) {
      char c = mantissa[i];
      if (i==0 && (c=='+' || c=='-')) { neg = (c=='-'); continue; }
      v = v*base_v + digit_to_val(c);
    }
    return neg ? -v : v;
  }

  std::vector<int64_t> Range::to_indices() const {
    int64_t s = start.as_integer();
    int64_t e = end.as_integer();
    std::vector<int64_t> out;
    if (s >= e) { for (int64_t i=s;i>=e;--i) out.push_back(i); }
    else { for (int64_t i=e;i>=s;--i) out.push_back(i); }
    return out;
  }

  std::string expr_to_string(const Expr& e) {
    struct V {
      std::string operator()(const Identifier& x) const { return x.name; }
      std::string operator()(const IdentifierIndexed& x) const { return x.name + "[" + std::to_string(x.index.as_integer()) + "]"; }
      // std::string operator()(const IdentifierSliced& x) const { return x.name + "[..]"; }
      std::string operator()(const IdentifierSliced& x) const {
	// Render explicit msb:lsb to faithfully represent the slice.
	return x.name + "[" +
	  std::to_string(x.range.start.as_integer()) + ":" +
	  std::to_string(x.range.end.as_integer()) + "]";
      }
      std::string operator()(const std::shared_ptr<Concatenation>& x) const {
	std::string s = "{"; bool first=true;
	for (auto& el : x->elements) { if (!first) s += ", "; first=false; s += expr_to_string(el); }
	s += "}"; return s;
      }
    };
    return std::visit(V{}, e);
  }

  std::string Module::summary() const {
    std::ostringstream oss;
    oss << "module " << module_name << "(";
    for (size_t i=0;i<port_list.size();++i) { if (i) oss << ", "; oss << port_list[i]; }
    oss << ");\n";
    oss << "  inputs:  " << input_declarations.size() << "\n";
    oss << "  outputs: " << output_declarations.size() << "\n";
    oss << "  inouts:  " << inout_declarations.size() << "\n";
    oss << "  wires:   " << net_declarations.size() << "\n";
    oss << "  assigns: " << assignments.size() << "\n";
    oss << "  insts:   " << module_instances.size() << "\n";
    oss << "endmodule\n";
    return oss.str();
  }

  Netlist parse_string(std::string_view text) {
    using grammar::start;
    using verilog::actions::action;
    using verilog::actions::State;

    memory_input in(text.data(), text.size(), "verilog_string");
    State st;
    try {
      if (!tao::pegtl::parse< start, action >(in, st)) {
	throw parse_error("parse returned false");
      }
    } catch (const tao::pegtl::parse_error& e) {
      throw parse_error(e.what());
    }
    Netlist nl; nl.modules = std::move(st.modules_accum);
    return nl;
  }

  Netlist parse_file(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs) throw parse_error("could not open file: " + path);
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    return parse_string(content);
  }

} // namespace verilog
