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
    // Name should already be escaped if needed by builder functions
    return name;
}

std::string VerilogWriter::format_expr(const Expr& e) {
    struct ExprFormatter {
        VerilogWriter& writer;
        
        std::string operator()(const Identifier& id) const {
            return writer.format_identifier(id.name);
        }
        
        std::string operator()(const IdentifierIndexed& ii) const {
            return writer.format_identifier(ii.name) + "[" + writer.format_number(ii.index) + "]";
        }
        
        std::string operator()(const IdentifierSliced& is) const {
            return writer.format_identifier(is.name) + writer.format_range(is.range);
        }
        
        std::string operator()(const std::shared_ptr<Concatenation>& concat) const {
            std::string result = "{";
            bool first = true;
            for (const auto& elem : concat->elements) {
                if (!first) result += ", ";
                first = false;
                result += writer.format_expr(elem);
            }
            result += "}";
            return result;
        }
    };
    
    return std::visit(ExprFormatter{*this}, e);
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
    
    // Write inout declarations
    if (!m.inout_declarations.empty()) {
        if (need_blank_line) write_newline();
        for (const auto& decl : m.inout_declarations) {
            write_indent();
            out_ << "inout";
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
        out_ << format_identifier(inst.module_name) << " " 
             << format_identifier(inst.instance_name);
        
        // Write port connections
        if (!inst.ports_pos.empty()) {
            // Positional connections
            out_ << "(";
            for (size_t i = 0; i < inst.ports_pos.size(); ++i) {
                if (i > 0) out_ << ", ";
                out_ << format_expr(inst.ports_pos[i]);
            }
            out_ << ");";
            write_newline();
        } else if (!inst.ports_named.empty()) {
            // Named connections - each on separate line
            out_ << "(";
            write_newline();
            indent_level_++;
            
            size_t idx = 0;
            for (const auto& [port_name, port_expr] : inst.ports_named) {
                write_indent();
                out_ << "." << port_name << "(" << format_expr(port_expr) << ")";
                if (idx < inst.ports_named.size() - 1) {
                    out_ << ",";
                }
                write_newline();
                idx++;
            }
            
            indent_level_--;
            write_indent();
            out_ << ");";
            write_newline();
        } else {
            // No connections
            out_ << "();";
            write_newline();
        }
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
    indent_level_--;
    
    out_ << "endmodule";
    write_newline();
    
    return out_.str();
}

std::string VerilogWriter::write(const Netlist& nl) {
    std::string result;
    
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
