#include "verilog_writer.hpp"
#include "verilog_builder.hpp"
#include <set>
#include <algorithm>

namespace verilog {

  // ============================================================================
  // Helper: Extract identifier name from expression
  // ============================================================================

  static std::string get_identifier_from_expr(const Expr& e) {
    if (auto num = std::get_if<Number>(&e)) {
      // Numbers don't reference signals
      return "";
    } else if (auto id = std::get_if<Identifier>(&e)) {
      return id->name;
    } else if (auto ii = std::get_if<IdentifierIndexed>(&e)) {
      return ii->name;
    } else if (auto is = std::get_if<IdentifierSliced>(&e)) {
      return is->name;
    }
    return "";
  }

  static void collect_expr_identifiers(const Expr& e, std::set<std::string>& ids) {
    if (auto concat = std::get_if<std::shared_ptr<Concatenation>>(&e)) {
      for (const auto& elem : (*concat)->elements) {
	collect_expr_identifiers(elem, ids);
      }
    } else {
      std::string id = get_identifier_from_expr(e);
      if (!id.empty()) {
	ids.insert(id);
      }
    }
  }

  // ============================================================================
  // Module Validation
  // ============================================================================

  std::vector<ValidationError> validate_module(const Module& m) {
    std::vector<ValidationError> errors;
    std::string mod_loc = "module " + m.module_name;
    
    // Check for nested sub_modules
    if (!m.sub_modules.empty()) {
      errors.emplace_back(
			  "Module contains nested sub_modules which are not supported",
			  mod_loc,
			  ValidationError::Severity::ERROR
			  );
    }
    
    // Collect all declared signal names
    std::set<std::string> all_signals;
    std::set<std::string> port_signals;
    
    // Check for duplicate inputs
    for (const auto& decl : m.input_declarations) {
      if (all_signals.count(decl.net_name)) {
	errors.emplace_back(
			    "Duplicate signal name: " + decl.net_name,
			    mod_loc,
			    ValidationError::Severity::ERROR
			    );
      }
      all_signals.insert(decl.net_name);
      port_signals.insert(decl.net_name);
    }
    
    // Check for duplicate outputs
    for (const auto& decl : m.output_declarations) {
      if (all_signals.count(decl.net_name)) {
	errors.emplace_back(
			    "Duplicate signal name: " + decl.net_name,
			    mod_loc,
			    ValidationError::Severity::ERROR
			    );
      }
      all_signals.insert(decl.net_name);
      port_signals.insert(decl.net_name);
    }
    
    // Check for duplicate inouts
    for (const auto& decl : m.inout_declarations) {
      if (all_signals.count(decl.net_name)) {
	errors.emplace_back(
			    "Duplicate signal name: " + decl.net_name,
			    mod_loc,
			    ValidationError::Severity::ERROR
			    );
      }
      all_signals.insert(decl.net_name);
      port_signals.insert(decl.net_name);
    }
    
    // Check for duplicate wires
    for (const auto& decl : m.net_declarations) {
      if (all_signals.count(decl.net_name)) {
	errors.emplace_back(
			    "Duplicate signal name: " + decl.net_name,
			    mod_loc,
			    ValidationError::Severity::ERROR
			    );
      }
      all_signals.insert(decl.net_name);
    }
    
    // Check port_list for duplicates
    std::set<std::string> port_list_set;
    for (const auto& port : m.port_list) {
      if (port_list_set.count(port)) {
	errors.emplace_back(
			    "Duplicate port in port list: " + port,
			    mod_loc,
			    ValidationError::Severity::ERROR
			    );
      }
      port_list_set.insert(port);
    }
    
    // Check that all port_list entries have declarations
    for (const auto& port : m.port_list) {
      if (!port_signals.count(port)) {
	errors.emplace_back(
			    "Port '" + port + "' in port list has no corresponding declaration",
			    mod_loc,
			    ValidationError::Severity::ERROR
			    );
      }
    }
    
    // Check that all port declarations are in port_list
    for (const auto& port : port_signals) {
      if (!port_list_set.count(port)) {
	errors.emplace_back(
			    "Port '" + port + "' is declared but not in port list",
			    mod_loc,
			    ValidationError::Severity::WARNING
			    );
      }
    }
    
    // Check instance names are unique
    std::set<std::string> instance_names;
    for (const auto& inst : m.module_instances) {
      if (instance_names.count(inst.instance_name)) {
	errors.emplace_back(
			    "Duplicate instance name: " + inst.instance_name,
			    mod_loc,
			    ValidationError::Severity::ERROR
			    );
      }
      instance_names.insert(inst.instance_name);
        
      // Check for mixing positional and named connections
      if (!inst.ports_pos.empty() && !inst.ports_named.empty()) {
	errors.emplace_back(
			    "Instance '" + inst.instance_name + "' mixes positional and named port connections",
			    mod_loc + ".instance " + inst.instance_name,
			    ValidationError::Severity::WARNING
			    );
      }
        
      // Check that connection expressions reference valid signals
      std::set<std::string> used_signals;
      for (const auto& port_expr : inst.ports_pos) {
	collect_expr_identifiers(port_expr, used_signals);
      }
      for (const auto& [port_name, port_expr] : inst.ports_named) {
	collect_expr_identifiers(port_expr, used_signals);
      }
        
      for (const auto& sig : used_signals) {
	if (!all_signals.count(sig)) {
	  errors.emplace_back(
			      "Instance '" + inst.instance_name + "' references undeclared signal: " + sig,
			      mod_loc + ".instance " + inst.instance_name,
			      ValidationError::Severity::ERROR
			      );
	}
      }
    }
    
    // Validate assignments
    for (size_t i = 0; i < m.assignments.size(); ++i) {
      const auto& ca = m.assignments[i];
      for (const auto& [lhs, rhs] : ca.assignments) {
	std::set<std::string> lhs_signals, rhs_signals;
	collect_expr_identifiers(lhs, lhs_signals);
	collect_expr_identifiers(rhs, rhs_signals);
            
	for (const auto& sig : lhs_signals) {
	  if (!all_signals.count(sig)) {
	    errors.emplace_back(
				"Assignment LHS references undeclared signal: " + sig,
				mod_loc + ".assign[" + std::to_string(i) + "]",
				ValidationError::Severity::ERROR
				);
	  }
	}
            
	for (const auto& sig : rhs_signals) {
	  if (!all_signals.count(sig)) {
	    errors.emplace_back(
				"Assignment RHS references undeclared signal: " + sig,
				mod_loc + ".assign[" + std::to_string(i) + "]",
				ValidationError::Severity::ERROR
				);
	  }
	}
      }
    }
    
    return errors;
  }

  // ============================================================================
  // Netlist Validation
  // ============================================================================

  std::vector<ValidationError> validate_netlist(const Netlist& nl) {
    std::vector<ValidationError> errors;
    
    // Validate each module
    for (const auto& mod : nl.modules) {
      auto mod_errors = validate_module(mod);
      errors.insert(errors.end(), mod_errors.begin(), mod_errors.end());
    }
    
    // Check for duplicate module names
    std::set<std::string> module_names;
    for (const auto& mod : nl.modules) {
      if (module_names.count(mod.module_name)) {
	errors.emplace_back(
			    "Duplicate module name: " + mod.module_name,
			    "netlist",
			    ValidationError::Severity::ERROR
			    );
      }
      module_names.insert(mod.module_name);
    }
    
    // Check that instantiated modules exist in netlist
    for (const auto& mod : nl.modules) {
      for (const auto& inst : mod.module_instances) {
	if (!module_names.count(inst.module_name)) {
	  errors.emplace_back(
			      "Instance '" + inst.instance_name + "' references undefined module: " + inst.module_name,
			      "module " + mod.module_name,
			      ValidationError::Severity::WARNING
			      );
	}
      }
    }
    
    return errors;
  }

} // namespace verilog
