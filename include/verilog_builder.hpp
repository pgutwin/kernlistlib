#pragma once
#include "veriloglib.hpp"
#include <stdexcept>

namespace verilog {

  // ============================================================================
  // Construction Helpers for Basic Types
  // ============================================================================

  // Number construction
  Number make_number(int64_t value);
  Number make_number(int length, char base, const std::string& mantissa);
  Number make_hex_number(int length, const std::string& hex_digits);
  Number make_bin_number(int length, const std::string& bin_digits);

  // Range construction
  Range make_range(int64_t msb, int64_t lsb);

  // ============================================================================
  // Expression Construction Helpers
  // ============================================================================

  // Check if identifier needs escaping and escape it. Names are held unescaped in the data
  // model; the writer escapes them with these.
  bool needs_escaping(const std::string& name);
  std::string escape_identifier(const std::string& name);

  // Simple identifier
  Expr make_identifier(const std::string& name);

  // Bit-select: name[index]
  Expr make_indexed(const std::string& name, int64_t index);
  Expr make_indexed(const std::string& name, const Number& index);

  // Part-select: name[msb:lsb]
  Expr make_sliced(const std::string& name, int64_t msb, int64_t lsb);
  Expr make_sliced(const std::string& name, const Range& range);

  // Concatenation: {expr1, expr2, ...}
  Expr make_concatenation(const std::vector<Expr>& elements);

  // Replication {count{expr1, ...}}, operators, choice, model value (KL08, KL10)
  Expr make_replication(int64_t count, const std::vector<Expr>& elements);
  Expr make_unary(const std::string& op, const Expr& operand);
  Expr make_binary(const std::string& op, const Expr& lhs, const Expr& rhs);
  Expr make_choice(const Expr& condition, const Expr& if_true, const Expr& if_false);
  Expr make_model_ref(const std::string& instance, const std::string& name);

  // Add an invariant statement (KL10)
  void add_invariant(Module& m, const std::string& name, const Expr& expr);

  // ============================================================================
  // Module Construction
  // ============================================================================

  // Create empty module
  Module make_module(const std::string& name);

  // ============================================================================
  // Module Modification API - Adding Elements
  // ============================================================================

  // Add port declarations (automatically adds to port_list)
  // Throws std::invalid_argument if name already exists
  void add_input(Module& m, const std::string& name, std::optional<Range> range = std::nullopt);
  void add_output(Module& m, const std::string& name, std::optional<Range> range = std::nullopt);

  // Add internal wire (not added to port list)
  // Throws std::invalid_argument if name already exists
  void add_wire(Module& m, const std::string& name, std::optional<Range> range = std::nullopt);

  // Add continuous assignment
  void add_assignment(Module& m, const Expr& lhs, const Expr& rhs);

  // Add module instance
  // Throws std::invalid_argument if instance_name already exists
  void add_instance(Module& m, const ModuleInstance& inst);

  // ============================================================================
  // Module Instance Construction
  // ============================================================================

  // Create module instance with connections in the given order (all connections are by name)
  ModuleInstance make_instance(
			       const std::string& module_name,
			       const std::string& instance_name,
			       const std::vector<Connection>& connections = {}
			       );

  // Create module instance with named port connections, in the order of the port names
  ModuleInstance make_instance_named(
				     const std::string& module_name,
				     const std::string& instance_name,
				     const std::map<std::string, Expr>& named_ports
				     );

  // ============================================================================
  // Module Modification API - Removing Elements
  // ============================================================================

  // Remove declarations (also removes from port_list if present)
  // Returns true if found and removed
  bool remove_input(Module& m, const std::string& name);
  bool remove_output(Module& m, const std::string& name);
  bool remove_wire(Module& m, const std::string& name);

  // Remove instance by name
  bool remove_instance(Module& m, const std::string& instance_name);

  // Remove from port list only (doesn't remove declaration)
  bool remove_from_port_list(Module& m, const std::string& name);

  // ============================================================================
  // Query Functions
  // ============================================================================

  // Check if signal exists
  bool has_port(const Module& m, const std::string& name);
  bool has_wire(const Module& m, const std::string& name);
  bool has_signal(const Module& m, const std::string& name); // any input/output/wire

  // Find instances
  const ModuleInstance* find_instance(const Module& m, const std::string& instance_name);
  std::vector<const ModuleInstance*> find_instances_of_type(const Module& m, const std::string& module_type);

  // ============================================================================
  // Netlist Operations
  // ============================================================================

  // Add module to netlist
  // Throws std::invalid_argument if module name already exists
  void add_module(Netlist& nl, const Module& m);

  // Find module in netlist
  const Module* find_module(const Netlist& nl, const std::string& module_name);
  Module* find_module(Netlist& nl, const std::string& module_name);

} // namespace verilog
