#include "verilog_writer.hpp"
#include "verilog_builder.hpp"
#include <sstream>
#include <iomanip>

namespace verilog {

  // ============================================================================
  // VerilogWriter Implementation
  // ============================================================================

  VerilogWriter::VerilogWriter(const WriterOptions& opts)
    : opts_(opts), indent_level_(0) {}

  void VerilogWriter::write_indent() {
    if (opts_.use_tabs) {
      for (int i = 0; i < indent_level_; ++i) {
	out_ << '\t';
      }
    } else {
      for (int i = 0; i < indent_level_ * opts_.indent_size; ++i) {
	out_ << ' ';
      }
    }
  }

  void VerilogWriter::write_newline() {
    out_ << '\n';
  }

  std::string VerilogWriter::current_indent() const {
    std::string indent;
    if (opts_.use_tabs) {
      for (int i = 0; i < indent_level_; ++i) {
	indent += '\t';
      }
    } else {
      for (int i = 0; i < indent_level_ * opts_.indent_size; ++i) {
	indent += ' ';
      }
    }
    return indent;
  }

  std::string VerilogWriter::format_number(const Number& n) {
    std::string result;
    
    // If we have base and length, use sized notation
    if (n.base.has_value() && n.length.has_value()) {
      result = std::to_string(n.length.value()) + "'" + n.base.value() + n.mantissa;
    } else if (n.base.has_value()) {
      // Base without length
      result = "'" + std::string(1, n.base.value()) + n.mantissa;
    } else {
      // Just the mantissa (decimal)
      result = n.mantissa;
    }
    
    return result;
  }

  std::string VerilogWriter::format_range(const Range& r) {
    return "[" + format_number(r.start) + ":" + format_number(r.end) + "]";
  }

  std::string VerilogWriter::format_identifier(const std::string& name) {
    // Names are held without escaping; escape where the format needs it
    return escape_identifier(name);
  }

  // Operator precedence, lowest first (doc/kernlist.bnf):
  //   1 ->   2 ?:   3 |   4 ^   5 &   6 == !=   7 < <= > >=   8 + -   9 unary   10 primary
  static int binary_level(const std::string& op) {
    if (op == "->") return 1;
    if (op == "|") return 3;
    if (op == "^") return 4;
    if (op == "&") return 5;
    if (op == "==" || op == "!=") return 6;
    if (op == "<" || op == "<=" || op == ">" || op == ">=") return 7;
    return 8;  // + -
  }

  std::string VerilogWriter::format_expr(const Expr& e) { return format_expr_at(e, 1); }

  // Writes e so that it reads back as one operand at the given precedence level,
  // adding parentheses only where needed.
  std::string VerilogWriter::format_expr_at(const Expr& e, int level) {
    struct ExprFormatter {
      VerilogWriter& writer;
      int& own_level;

      std::string operator()(const Number& num) const { return writer.format_number(num); }
      std::string operator()(const Identifier& id) const { return writer.format_identifier(id.name); }
      std::string operator()(const IdentifierIndexed& ii) const {
        return writer.format_identifier(ii.name) + "[" + writer.format_number(ii.index) + "]";
      }
      std::string operator()(const IdentifierSliced& is) const {
        return writer.format_identifier(is.name) + writer.format_range(is.range);
      }
      std::string list(const std::vector<Expr>& elements) const {
        std::string result;
        for (size_t i = 0; i < elements.size(); ++i) {
          if (i) result += ", ";
          result += writer.format_expr_at(elements[i], 1);
        }
        return result;
      }
      std::string operator()(const std::shared_ptr<Concatenation>& c) const { return "{" + list(c->elements) + "}"; }
      std::string operator()(const std::shared_ptr<Replication>& r) const {
        return "{" + writer.format_number(r->count) + "{" + list(r->elements) + "}}";
      }
      std::string operator()(const ModelRef& m) const {
        std::string s = "model(" + writer.format_identifier(m.instance) + "." + writer.format_identifier(m.name);
        if (m.index) s += "[" + writer.format_number(*m.index) + "]";
        if (m.range) s += writer.format_range(*m.range);
        return s + ")";
      }
      std::string operator()(const std::shared_ptr<UnaryOp>& u) const {
        own_level = 9;
        return u->op + writer.format_expr_at(u->operand, 10);
      }
      std::string operator()(const std::shared_ptr<BinaryOp>& b) const {
        int l = binary_level(b->op);
        own_level = l;
        // | ^ & + - group to the left; the others take no operand at their own level
        bool left_grouping = (l == 3 || l == 4 || l == 5 || l == 8);
        return writer.format_expr_at(b->lhs, left_grouping ? l : l + 1) + " " + b->op + " "
          + writer.format_expr_at(b->rhs, l + 1);
      }
      std::string operator()(const std::shared_ptr<Choice>& c) const {
        own_level = 2;
        return writer.format_expr_at(c->condition, 3) + " ? " + writer.format_expr_at(c->if_true, 2)
          + " : " + writer.format_expr_at(c->if_false, 2);
      }
    };

    int own_level = 10;
    std::string text = std::visit(ExprFormatter{*this, own_level}, e);
    return own_level < level ? "(" + text + ")" : text;
  }

  void VerilogWriter::check_no_sub_modules(const Module& m) {
    if (!m.sub_modules.empty()) {
      throw parse_error("Module '" + m.module_name + "' contains nested sub_modules which are not supported for writing");
    }
  }

  void VerilogWriter::write_module_header(const Module& m) {
    out_ << "module " << format_identifier(m.module_name) << "(";
    
    // Calculate length of port list
    int port_list_len = 0;
    for (size_t i = 0; i < m.port_list.size(); ++i) {
      if (i > 0) port_list_len += 2; // ", "
      port_list_len += m.port_list[i].length();
    }
    
    // If port list is short, write on one line
    if (port_list_len <= opts_.max_port_list_length || m.port_list.size() <= 3) {
      for (size_t i = 0; i < m.port_list.size(); ++i) {
	if (i > 0) out_ << ", ";
	out_ << format_identifier(m.port_list[i]);
      }
    } else {
      // Write each port on separate line
      write_newline();
      indent_level_++;
      for (size_t i = 0; i < m.port_list.size(); ++i) {
	write_indent();
	out_ << format_identifier(m.port_list[i]);
	if (i < m.port_list.size() - 1) {
	  out_ << ",";
	}
	write_newline();
      }
      indent_level_--;
    }
    
    out_ << ");";
    write_newline();
  }

  void VerilogWriter::write_declarations(const Module& m) {
    bool need_blank_line = false;
    
    // Write input declarations
    if (!m.input_declarations.empty()) {
      for (const auto& decl : m.input_declarations) {
	write_indent();
	out_ << "input";
	if (decl.range.has_value()) {
	  out_ << " " << format_range(decl.range.value());
	}
	out_ << " " << format_identifier(decl.net_name) << ";";
	write_newline();
      }
      need_blank_line = true;
    }
    
    // Write output declarations
    if (!m.output_declarations.empty()) {
      if (need_blank_line) write_newline();
      for (const auto& decl : m.output_declarations) {
	write_indent();
	out_ << "output";
	if (decl.range.has_value()) {
	  out_ << " " << format_range(decl.range.value());
	}
	out_ << " " << format_identifier(decl.net_name) << ";";
	write_newline();
      }
      need_blank_line = true;
    }
    
    // Write wire declarations
    if (!m.net_declarations.empty()) {
      if (need_blank_line) write_newline();
      for (const auto& decl : m.net_declarations) {
	write_indent();
	out_ << "wire";
	if (decl.range.has_value()) {
	  out_ << " " << format_range(decl.range.value());
	}
	out_ << " " << format_identifier(decl.net_name) << ";";
	write_newline();
      }
    }
  }

  void VerilogWriter::write_assignments(const Module& m) {
    if (m.assignments.empty()) return;
    
    write_newline();
    for (const auto& ca : m.assignments) {
      for (const auto& [lhs, rhs] : ca.assignments) {
	write_indent();
	out_ << "assign " << format_expr(lhs) << " = " << format_expr(rhs) << ";";
	write_newline();
      }
    }
  }

  void VerilogWriter::write_instances(const Module& m) {
    if (m.module_instances.empty()) return;

    write_newline();
    for (const auto& inst : m.module_instances) {
      write_indent();
      out_ << format_identifier(inst.module_name);
      if (!inst.parameters.empty()) {
        out_ << " #(";
        for (size_t i = 0; i < inst.parameters.size(); ++i) {
          if (i) out_ << ", ";
          out_ << "." << format_identifier(inst.parameters[i].name) << "(" << inst.parameters[i].value << ")";
        }
        out_ << ")";
      }
      if (inst.variant) out_ << " variant " << format_identifier(*inst.variant);
      out_ << " " << format_identifier(inst.instance_name) << " (";
      if (inst.connections.empty()) {
        out_ << ");";
        write_newline();
        continue;
      }
      write_newline();
      indent_level_++;
      for (size_t i = 0; i < inst.connections.size(); ++i) {
        const auto& c = inst.connections[i];
        write_indent();
        out_ << "." << format_identifier(c.port) << "(" << (c.expr ? format_expr(*c.expr) : "") << ")";
        if (i + 1 < inst.connections.size()) out_ << ",";
        write_newline();
      }
      indent_level_--;
      write_indent();
      out_ << ");";
      write_newline();
    }
  }

  void VerilogWriter::write_invariants(const Module& m) {
    if (m.invariants.empty()) return;
    write_newline();
    for (const auto& inv : m.invariants) {
      write_indent();
      out_ << "invariant " << format_identifier(inv.name) << ": " << format_expr(inv.expr) << ";";
      write_newline();
    }
  }

  std::string VerilogWriter::write(const Module& m) {
    check_no_sub_modules(m);
    
    out_.str("");
    out_.clear();
    indent_level_ = 0;
    
    write_module_header(m);
    
    indent_level_++;
    write_declarations(m);
    write_assignments(m);
    write_instances(m);
    write_invariants(m);
    indent_level_--;
    
    out_ << "endmodule";
    write_newline();
    
    return out_.str();
  }

  std::string VerilogWriter::write(const Netlist& nl) {
    std::string result = "kernlist_format " + std::to_string(nl.format_version) + ";\n\n";

    for (size_t i = 0; i < nl.modules.size(); ++i) {
      if (i > 0) result += "\n";
      result += write(nl.modules[i]);  // Each returns independent string
    }
    return result;
  }
  //   std::string VerilogWriter::write(const Netlist& nl) {
  //     out_.str("");
  //     out_.clear();
  //     indent_level_ = 0;
    
  //     for (size_t i = 0; i < nl.modules.size(); ++i) {
  //         if (i > 0) write_newline(); // blank line between modules
  //         out_ << write(nl.modules[i]);
  //     }
    
  //     return out_.str();
  // }

  // ============================================================================
  // Public Writer Functions
  // ============================================================================

  std::string write_module(const Module& m, const WriterOptions& opts) {
    // Validate if requested
    if (opts.validate_before_write) {
      auto errors = validate_module(m);
      if (!errors.empty()) {
	std::string msg = "Validation failed for module '" + m.module_name + "':\n";
	for (const auto& err : errors) {
	  if (err.severity == ValidationError::Severity::ERROR) {
	    msg += "  ERROR: " + err.message + " (at " + err.location + ")\n";
	  }
	}
	throw parse_error(msg);
      }
    }
    
    VerilogWriter writer(opts);
    return writer.write(m);
  }

  std::string write_netlist(const Netlist& nl, const WriterOptions& opts) {
    // Validate if requested
    if (opts.validate_before_write) {
      auto errors = validate_netlist(nl);
      if (!errors.empty()) {
	std::string msg = "Validation failed for netlist:\n";
	for (const auto& err : errors) {
	  if (err.severity == ValidationError::Severity::ERROR) {
	    msg += "  ERROR: " + err.message + " (at " + err.location + ")\n";
	  }
	}
	throw parse_error(msg);
      }
    }
    
    VerilogWriter writer(opts);
    return writer.write(nl);
  }

  std::string write_number(const Number& n) {
    VerilogWriter writer(WriterOptions{});
    return writer.format_number(n);
  }

  std::string write_range(const Range& r) {
    VerilogWriter writer(WriterOptions{});
    return writer.format_range(r);
  }

  std::string write_expr(const Expr& e) {
    VerilogWriter writer(WriterOptions{});
    return writer.format_expr(e);
  }

} // namespace verilog
