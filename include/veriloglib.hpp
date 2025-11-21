#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <variant>
#include <optional>
#include <map>
#include <stdexcept>
#include <cstdint>
#include <sstream>
#include <memory>

namespace verilog {

  struct Number {
    std::optional<int> length;
    std::optional<char> base;
    std::string mantissa;
    int64_t as_integer() const;
  };

  struct Range {
    Number start;
    Number end;
    std::vector<int64_t> to_indices() const;
  };

  struct Identifier { std::string name; };
  struct IdentifierIndexed { std::string name; Number index; };
  struct IdentifierSliced { std::string name; Range range; };
  struct Concatenation;
  using Expr = std::variant<Identifier, IdentifierIndexed, IdentifierSliced, std::shared_ptr<Concatenation>>;
  struct Concatenation { std::vector<Expr> elements; };

  struct NetDeclaration {
    std::string net_name;
    std::optional<Range> range;
  };
  struct OutputDeclaration : NetDeclaration {};
  struct InputDeclaration  : NetDeclaration {};
  struct InoutDeclaration  : NetDeclaration {};

  struct ContinuousAssign {
    std::vector<std::pair<Expr, Expr>> assignments;
  };

  struct ModuleInstance {
    std::string module_name;
    std::string instance_name;
    std::vector<Expr> ports_pos;
    std::map<std::string, Expr> ports_named;
  };

  struct Module {
    std::string module_name;
    std::vector<std::string> port_list;
    std::vector<NetDeclaration> net_declarations;
    std::vector<OutputDeclaration> output_declarations;
    std::vector<InputDeclaration>  input_declarations;
    std::vector<InoutDeclaration>  inout_declarations;
    std::vector<ModuleInstance>    module_instances;
    std::vector<ContinuousAssign>  assignments;
    std::vector<Module>            sub_modules;
    std::string summary() const;
  };

  struct Netlist { std::vector<Module> modules; };

  struct parse_error : std::runtime_error { using std::runtime_error::runtime_error; };

  Netlist parse_string(std::string_view text);
  Netlist parse_file(const std::string& path);

  std::string expr_to_string(const Expr& e);

} // namespace verilog
