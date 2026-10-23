# kernlistlib

The parser, writer and validator for the **kernlist**, KERN's
description of a design as motif instances, the logic between them and
their connections. A fork of veriloglib at commit `6f02bcc`; the
kernlist format extends veriloglib's structural Verilog subset with
KERN's concepts. The format: `doc/KERNLIST_format.md` and
`doc/kernlist.bnf`; decisions: `doc/KERNLIST_decisions.md`.

The reader now reads the kernlist format, not Verilog: a file starts
with `kernlist_format 1;`, and instances take parameters, `variant`,
and named connections only. The data model is in
`include/veriloglib.hpp`. What follows below the line is veriloglib's
README as it was at the fork; its description of the grammar and the
data model (inout, positional connections, attributes as comments) is
out of date and will be rewritten once the format settles.

    ./build/vparse path/to/file.kl

Python (KL19), from the build directory or with it on `PYTHONPATH`:

```python
import kernlist
nl = kernlist.read("design.kl")              # kernlist.ParseError has .line and .column
for p in kernlist.check(nl):                 # every problem, with its line
    print(p.line, p.message)
m = nl.modules[0]
for g in kernlist.logic_groups(m):           # the logic groups, named after what they drive
    print(g.name, g.inputs, g.outputs)
m.module_instances[0].variant = "circular_buffer"   # changes the netlist in place
print(kernlist.write(nl))
```

The bindings are built by default and need Python 3.8 or later with its
development files; `-DKERNLISTLIB_PYTHON=OFF` leaves them out.

Build environment: `containers/kernlistlib.def` (Apptainer; Rocky
Linux 8, glibc 2.28, GCC 13):

    apptainer build kernlistlib.sif containers/kernlistlib.def
    apptainer exec kernlistlib.sif cmake -S . -B build
    apptainer exec kernlistlib.sif cmake --build build -j
    apptainer exec kernlistlib.sif ctest --test-dir build --output-on-failure

---

# Verilog Parser & Writer (C++ / PEGTL)

This repository provides a lightweight Verilog parser and writer implemented in C++20 using [PEGTL](https://github.com/taocpp/PEGTL) and tested with GoogleTest. It is aimed at **structural Verilog** use‑cases: module headers, declarations, continuous assignments, and simple module instantiations.

> **Scope note**: This is a focused subset — it is *not* a complete Verilog-2005 parser.

---

## Overview

- **Language**: C++20
- **Parser**: PEGTL (Parsing Expression Grammar Template Library)
- **Testing**: GoogleTest
- **Capabilities**: 
  - **Parse** Verilog files into C++ data structures
  - **Create** Verilog modules programmatically
  - **Modify** existing parsed structures
  - **Write** data structures back to Verilog text
  - **Validate** module and netlist structures
- **Folders**:  
  - `include/` – public API headers
  - `src/` – implementation and CLI (`vparse`)  
  - `tests/` – unit tests (GoogleTest)
  - `doc/` – documentation and grammar specification

Build & test:
```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

CLI:
```bash
./build/vparse path/to/file.v
```

---

## Quick Start Examples

### Parse Verilog

```cpp
#include "veriloglib.hpp"

using namespace verilog;

Netlist nl = parse_file("design.v");
for (const auto& module : nl.modules) {
    std::cout << module.summary();
}
```

### Create Verilog from Scratch

```cpp
#include "verilog_builder.hpp"
#include "verilog_writer.hpp"

using namespace verilog;

Module m = make_module("buffer");
add_input(m, "in", make_range(7, 0));
add_output(m, "out", make_range(7, 0));
add_wire(m, "default_val", make_range(3, 0));

// Assign with number literal (NEW: numbers can be RHS expressions)
add_assignment(m, make_identifier("default_val"), make_hex_number(4, "x"));
add_assignment(m, make_identifier("out"), make_identifier("in"));

std::string verilog = write_module(m);
std::cout << verilog;
```

Output:
```verilog
module buffer(in, out);
  input [7:0] in;

  output [7:0] out;

  wire [3:0] default_val;

  assign default_val = 4'hx;
  assign out = in;
endmodule
```

### Modify and Write

```cpp
#include "veriloglib.hpp"
#include "verilog_builder.hpp"
#include "verilog_writer.hpp"

using namespace verilog;

// Parse existing design
Netlist nl = parse_file("design.v");
Module& m = nl.modules[0];

// Add debug signals
add_wire(m, "debug_signal", make_range(3, 0));
add_assignment(m, make_identifier("debug_signal"), make_sliced("bus", 3, 0));

// Validate and write
auto errors = validate_module(m);
if (errors.empty()) {
    std::cout << write_netlist(nl);
}
```

---

## Public API

### Parser API (veriloglib.hpp)

#### Data structures

```cpp
struct Number {
  std::optional<int>  length;     // optional size to the left of the base (e.g., 8'hFF)
  std::optional<char> base;       // 'b'|'o'|'d'|'h' or std::nullopt
  std::string         mantissa;   // digits as written
  int64_t as_integer() const;     // best-effort integer conversion
};

struct Range {                     // [msb:lsb]
  Number start;
  Number end;
  std::vector<int64_t> to_indices() const;
};

struct Identifier          { std::string name; };
struct IdentifierIndexed   { std::string name; Number index; };                 // a[3]
struct IdentifierSliced    { std::string name; Range range; };                  // a[7:0]
struct Concatenation;                                                           // {a, b[1], c[3:0]}
using Expr = std::variant<Number, Identifier, IdentifierIndexed, IdentifierSliced, std::shared_ptr<Concatenation>>;
struct Concatenation { std::vector<Expr> elements; };

struct NetDeclaration   { std::string net_name; std::optional<Range> range; };
struct OutputDeclaration: NetDeclaration {};
struct InputDeclaration : NetDeclaration {};
struct InoutDeclaration : NetDeclaration {};

struct ContinuousAssign { std::vector<std::pair<Expr, Expr>> assignments; };

struct ModuleInstance {
  std::string module_name;        // type, e.g., "leaf"
  std::string instance_name;      // e.g., "u0"
  std::vector<Expr>         ports_pos;   // positional connections
  std::map<std::string,Expr> ports_named; // named connections
};

struct Module {
  std::string module_name;
  std::vector<std::string> port_list;
  std::vector<NetDeclaration>    net_declarations;
  std::vector<OutputDeclaration> output_declarations;
  std::vector<InputDeclaration>  input_declarations;
  std::vector<InoutDeclaration>  inout_declarations;
  std::vector<ModuleInstance>    module_instances;
  std::vector<ContinuousAssign>  assignments;

  std::string summary() const;
};

struct Netlist { std::vector<Module> modules; };
```

#### Parse functions

```cpp
Netlist parse_string(std::string_view text);     // throws verilog::parse_error on failure
Netlist parse_file(const std::string& path);     // throws verilog::parse_error on failure
```

### Builder API (verilog_builder.hpp)

#### Construction helpers

```cpp
// Numbers
Number make_number(int64_t value);
Number make_hex_number(int length, const std::string& hex_digits);
Number make_bin_number(int length, const std::string& bin_digits);

// Ranges
Range make_range(int64_t msb, int64_t lsb);

// Expressions (identifiers automatically escaped if needed)
Expr make_identifier(const std::string& name);
Expr make_indexed(const std::string& name, int64_t index);
Expr make_sliced(const std::string& name, int64_t msb, int64_t lsb);
Expr make_concatenation(const std::vector<Expr>& elements);

// Module construction
Module make_module(const std::string& name);
```

#### Modification functions

```cpp
// Add declarations (ports automatically added to port_list)
void add_input(Module& m, const std::string& name, std::optional<Range> range = std::nullopt);
void add_output(Module& m, const std::string& name, std::optional<Range> range = std::nullopt);
void add_inout(Module& m, const std::string& name, std::optional<Range> range = std::nullopt);
void add_wire(Module& m, const std::string& name, std::optional<Range> range = std::nullopt);

// Add assignments and instances
void add_assignment(Module& m, const Expr& lhs, const Expr& rhs);
void add_instance(Module& m, const ModuleInstance& inst);

// Create instances
ModuleInstance make_instance(const std::string& module_name,
                             const std::string& instance_name,
                             const std::vector<Expr>& positional_ports = {});
ModuleInstance make_instance_named(const std::string& module_name,
                                   const std::string& instance_name,
                                   const std::map<std::string, Expr>& named_ports);

// Remove elements
bool remove_input(Module& m, const std::string& name);
bool remove_output(Module& m, const std::string& name);
bool remove_wire(Module& m, const std::string& name);
bool remove_instance(Module& m, const std::string& instance_name);

// Query functions
bool has_signal(const Module& m, const std::string& name);
const ModuleInstance* find_instance(const Module& m, const std::string& instance_name);

// Netlist operations
void add_module(Netlist& nl, const Module& m);
Module* find_module(Netlist& nl, const std::string& module_name);
```

### Writer API (verilog_writer.hpp)

#### Writer options

```cpp
struct WriterOptions {
    int indent_size = 2;                    // spaces per indent level
    bool use_tabs = false;                  // use tabs instead of spaces
    int max_port_list_length = 80;          // wrap port lists longer than this
    bool one_declaration_per_line = true;   // split declarations to separate lines
    bool validate_before_write = false;     // validate structure before writing
};
```

#### Write functions

```cpp
std::string write_module(const Module& m, const WriterOptions& opts = {});
std::string write_netlist(const Netlist& nl, const WriterOptions& opts = {});
std::string write_number(const Number& n);
std::string write_range(const Range& r);
std::string write_expr(const Expr& e);
```

#### Validation

```cpp
struct ValidationError {
    enum class Severity { ERROR, WARNING };
    std::string message;
    std::string location;
    Severity severity;
};

std::vector<ValidationError> validate_module(const Module& m);
std::vector<ValidationError> validate_netlist(const Netlist& nl);
```

---

## Supported Verilog Syntax (Detailed)

This section documents **exactly** what the parser accepts. For a formal grammar, see `doc/verilog_subset.bnf`.

### Modules
- Form:
  ```verilog
  module <identifier> ( <port_list_opt> );
    <module_item>*
  endmodule
  ```
- `<port_list_opt>` is zero or more identifiers separated by commas (no directions in the header — directions appear as declarations inside the body).

### Declarations
- Supported kinds (each ends with `;`):
  ```verilog
  input   [<msb>:<lsb>] <id> ( , <id> )* ;
  output  [<msb>:<lsb>] <id> ( , <id> )* ;
  inout   [<msb>:<lsb>] <id> ( , <id> )* ;
  wire    [<msb>:<lsb>] <id> ( , <id> )* ;
  ```
- Notes:
  - Range bounds are numbers (see **Numbers**). Expressions in ranges are **not** supported.
  - Internally (per tests), each statement is recorded as **one** declaration entry (names collapsed).

### Continuous assignments
- Form:
  ```verilog
  assign <lhs>=<rhs> ( , <lhs>=<rhs> )* ;
  ```
- `<lhs>` / `<rhs>` are limited to the **expression forms** below (no operators).

### Module instantiation
- Forms:
  ```verilog
  <module_type> <instance_name> ( <port_connections_opt> ) ;
  ```
  where `<port_connections_opt>` is one of:
  - Positional: `expr , expr , ...`
  - Named: `.port_name( expr ) , ...`
- Mixing positional and named is **not validated** (the grammar allows a mix; semantic checks are out of scope).

### Expressions (subset)
- Allowed primary expressions:
  - Number literal: `2'hx`, `8'hFF`, `123`, `'b1010` (see **Numbers** section)
  - Identifier: `a`
  - Bit-select: `a[<number>]`
  - Part-select: `a[<msb>:<lsb>]` (numeric bounds only)
  - Concatenation: `{ expr , expr , ... }` (can contain numbers and identifiers)
- **Not supported**: unary/binary operators (`~ & | ^ + - * / % << >>`, comparisons, ternary, parentheses for grouping).

### Identifiers
- Normal identifiers: `[A-Za-z_][A-Za-z0-9_$]*`
- Escaped identifiers (Verilog-2005 style): `\` followed by a run of non‑whitespace characters excluding `[]{}().,;=` (trailing whitespace ends the identifier).

### Numbers
- Supported forms:
  - Decimal (plain): `123`, `456` (digits 0-9 only)
  - Signed: `+123`, `-456`
  - Sized with base: `8'hFF`, `2'b10`, `4'd15`
    - Size: decimal digits (e.g., `8`)
    - Base: `'b|'B|'o|'O|'d|'D|'h|'H`
    - Digits: hex digits `[0-9A-Fa-fxzXZ]` (supports don't-care `x`/`z` values)
  - Unsized with base: `'hFF`, `'b1010`, `'o777`
- **x/z values**: Supported in based numbers (e.g., `2'hx`, `4'bzz0x`)
- **Usage**: Numbers can appear in:
  - Ranges: `wire [7:0] data;`
  - Bit-selects: `signal[3]`
  - Part-selects: `bus[15:8]`
  - **Assignments**: `assign a = 2'hx;` (NEW)
  - **Concatenations**: `{8'h00, data, 2'hx}` (NEW)

### Comments & whitespace
- Line comments: `// ...` to end of line
- Block comments: `/* ... */`
- Attribute-style blocks (treated as comments): `(* ... *)`
- Whitespace/comments may appear between all tokens.

### Known limitations
- No procedural blocks (`always`, `initial`), no parameters/defparams, no generate blocks, no attributes binding semantics, no operators in expressions, no multi-dimensional arrays, no tasks/functions, no time/scaling, no `assign` drive strengths/delays.

---

## Output Format

The writer produces consistently formatted Verilog with:
- Configurable indentation (default: 2 spaces)
- One declaration per line
- Blank lines between declaration sections
- Named port connections on separate lines
- Ranges in `[msb:lsb]` format
- Preserved number notation (base and size when available)

Example output:
```verilog
module example(clk, data_in, data_out);
  input clk;
  input [7:0] data_in;

  output [7:0] data_out;

  wire [7:0] internal;

  assign internal = data_in;
  assign data_out = internal;

  register_slice u0(
    .clk(clk),
    .data(internal),
    .q(data_out)
  );
endmodule
```

---

## Validation

The validation API checks for:
- **Errors** (will prevent code generation):
  - Duplicate signal names
  - Undeclared signal references
  - Duplicate instance names
  - Port list inconsistencies
  - Duplicate module names
- **Warnings** (informational):
  - Mixed positional/named connections
  - Undeclared module types
  - Ports declared but not in port_list

Example:
```cpp
auto errors = validate_module(m);
for (const auto& err : errors) {
    std::cerr << (err.severity == ValidationError::Severity::ERROR ? "ERROR" : "WARNING")
              << " at " << err.location << ": " << err.message << "\n";
}
```

---

## Documentation

- **API Reference**: See `doc/WRITER_API.md` for comprehensive API documentation with examples
- **Grammar Specification**: See `doc/verilog_subset.bnf` for the formal BNF grammar
- **Implementation Details**: See `IMPLEMENTATION_SUMMARY.md` for technical details

---

## Complete Example

```cpp
#include "verilog_builder.hpp"
#include "verilog_writer.hpp"

using namespace verilog;

int main() {
    // Create netlist
    Netlist nl;
    
    // Create a register slice module
    Module reg_slice = make_module("register_slice");
    add_input(reg_slice, "clk");
    add_input(reg_slice, "data_in", make_range(7, 0));
    add_output(reg_slice, "data_out", make_range(7, 0));
    add_module(nl, reg_slice);
    
    // Create top-level module
    Module top = make_module("design_top");
    add_input(top, "sys_clk");
    add_input(top, "input_data", make_range(7, 0));
    add_output(top, "output_data", make_range(7, 0));
    add_wire(top, "stage1", make_range(7, 0));
    
    // Add instance
    auto u0 = make_instance_named("register_slice", "stage1_reg", {
        {"clk", make_identifier("sys_clk")},
        {"data_in", make_identifier("input_data")},
        {"data_out", make_identifier("stage1")}
    });
    add_instance(top, u0);
    
    // Final connection
    add_assignment(top, make_identifier("output_data"), make_identifier("stage1"));
    add_module(nl, top);
    
    // Validate
    auto errors = validate_netlist(nl);
    if (!errors.empty()) {
        for (const auto& err : errors) {
            std::cerr << err.message << "\n";
        }
        return 1;
    }
    
    // Write output
    std::cout << write_netlist(nl);
    return 0;
}
```

---

## License

[Your license here]

---

## Contributing

[Your contributing guidelines here]
