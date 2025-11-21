#pragma once
#include "veriloglib.hpp"
#include <vector>
#include <sstream>

namespace verilog {

  // ============================================================================
  // Writer Options
  // ============================================================================

  struct WriterOptions {
    int indent_size = 2;                    // spaces per indent level
    bool use_tabs = false;                  // use tabs instead of spaces
    int max_port_list_length = 80;          // wrap port lists longer than this
    bool one_declaration_per_line = true;   // split "input a, b;" into separate lines
    bool validate_before_write = false;     // run validation before writing
  };

  // ============================================================================
  // Validation Errors
  // ============================================================================

  struct ValidationError {
    enum class Severity { ERROR, WARNING };
    
    std::string message;
    std::string location;
    Severity severity;
    
    ValidationError(const std::string& msg, const std::string& loc, Severity sev = Severity::ERROR)
      : message(msg), location(loc), severity(sev) {}
  };

  // ============================================================================
  // Validation API
  // ============================================================================

  // Validate module structure
  std::vector<ValidationError> validate_module(const Module& m);

  // Validate entire netlist
  std::vector<ValidationError> validate_netlist(const Netlist& nl);

  // ============================================================================
  // Writer API
  // ============================================================================

  // Write module to string
  // Throws parse_error if structure is invalid (e.g., has sub_modules)
  // If opts.validate_before_write is true, throws on validation errors
  std::string write_module(const Module& m, const WriterOptions& opts = {});

  // Write entire netlist to string
  std::string write_netlist(const Netlist& nl, const WriterOptions& opts = {});

  // Write individual components (for testing/debugging)
  std::string write_number(const Number& n);
  std::string write_range(const Range& r);
  std::string write_expr(const Expr& e);

  // ============================================================================
  // Internal Writer Class
  // ============================================================================

  class VerilogWriter {
  private:
    WriterOptions opts_;
    std::ostringstream out_;
    int indent_level_;
    
    void write_indent();
    void write_newline();
    std::string current_indent() const;
    
    void write_module_header(const Module& m);
    void write_declarations(const Module& m);
    void write_assignments(const Module& m);
    void write_instances(const Module& m);
    
    void check_no_sub_modules(const Module& m);
    
  public:
    explicit VerilogWriter(const WriterOptions& opts);
    std::string write(const Module& m);
    std::string write(const Netlist& nl);
    
    // Public formatting methods for standalone use
    std::string format_number(const Number& n);
    std::string format_range(const Range& r);
    std::string format_expr(const Expr& e);
    std::string format_identifier(const std::string& name);
  };

} // namespace verilog
