#pragma once
// The kernlist data model (doc/KERNLIST_format.md), extended from veriloglib's.
// Names are held as written, without the backslash of an escaped identifier;
// the writer escapes them again where needed.
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

  // Where a statement starts in its file (line and column from 1); 0 when made by the builder.
  struct Location {
    int line = 0;
    int column = 0;
  };

  struct Number {
    std::optional<int> length;      // sized constants: 2'd3 has length 2
    std::optional<char> base;       // b, o, d or h, as written
    std::string mantissa;           // the digits; an unsized number has only these
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
  struct Replication;
  struct UnaryOp;
  struct BinaryOp;
  struct Choice;
  // model(instance.name) or model(instance.name[i]) or model(instance.name[msb:lsb]), in invariants (KL10)
  struct ModelRef {
    std::string instance;
    std::string name;
    std::optional<Number> index;
    std::optional<Range> range;
  };
  using Expr = std::variant<Number, Identifier, IdentifierIndexed, IdentifierSliced,
                            std::shared_ptr<Concatenation>,
                            std::shared_ptr<Replication>,
                            std::shared_ptr<UnaryOp>,
                            std::shared_ptr<BinaryOp>,
                            std::shared_ptr<Choice>,
                            ModelRef>;
  struct Concatenation { std::vector<Expr> elements; };
  // {count{elements}}
  struct Replication { Number count; std::vector<Expr> elements; };
  // op is one of: ~ & | ^   (& | ^ are reductions)
  struct UnaryOp { std::string op; Expr operand; };
  // op is one of: | ^ & == != < <= > >= + - ->   (see KL08, KL10, KL13 for where each is allowed)
  struct BinaryOp { std::string op; Expr lhs; Expr rhs; };
  // condition ? if_true : if_false
  struct Choice { Expr condition; Expr if_true; Expr if_false; };

  struct NetDeclaration {
    std::string net_name;
    std::optional<Range> range;
    Location location;
  };
  struct OutputDeclaration : NetDeclaration {};
  struct InputDeclaration  : NetDeclaration {};

  // One assign statement, with one or more assignments
  struct ContinuousAssign {
    std::vector<std::pair<Expr, Expr>> assignments;
    Location location;
  };

  struct Parameter {
    std::string name;
    int64_t value = 0;
  };

  // .port(expression), or .port() when left unconnected
  struct Connection {
    std::string port;
    std::optional<Expr> expr;
  };

  // A motif instance, or an instance of a kernlist module (KL06)
  struct ModuleInstance {
    std::string module_name;               // the motif family, or the module
    std::string instance_name;
    std::vector<Parameter> parameters;     // in the order written
    std::optional<std::string> variant;    // KERN's choice; none while not yet chosen
    std::vector<Connection> connections;   // in the order written
    Location location;
  };

  // invariant name: expression;  (KL10)
  struct Invariant {
    std::string name;
    Expr expr;
    Location location;
  };

  struct Module {
    std::string module_name;
    std::vector<std::string> port_list;
    std::vector<NetDeclaration> net_declarations;
    std::vector<OutputDeclaration> output_declarations;
    std::vector<InputDeclaration>  input_declarations;
    std::vector<ModuleInstance>    module_instances;
    std::vector<ContinuousAssign>  assignments;
    std::vector<Invariant>         invariants;
    std::vector<Module>            sub_modules;
    Location location;
    std::string summary() const;
  };

  struct Netlist {
    int format_version = 1;                // from the line kernlist_format N; (KL03)
    std::vector<Module> modules;
  };

  // A syntax error (KL14: reading stops at the first). line and column are from 1.
  struct parse_error : std::runtime_error {
    using std::runtime_error::runtime_error;
    int line = 0;
    int column = 0;
  };

  Netlist parse_string(std::string_view text, const std::string& source = "string");
  Netlist parse_file(const std::string& path);

  std::string expr_to_string(const Expr& e);

} // namespace verilog
