# Verilog Writer/Builder API Documentation

## Overview

This extension to the Verilog parser adds comprehensive capabilities for creating, modifying, and writing Verilog modules programmatically. The API consists of three main components:

1. **Builder API** (`verilog_builder.hpp`) - Construction and modification helpers
2. **Writer API** (`verilog_writer.hpp`) - Conversion to Verilog text with validation
3. **Validation API** - Structure validation and error reporting

## Quick Start

### Creating a Module from Scratch

```cpp
#include "verilog_builder.hpp"
#include "verilog_writer.hpp"

using namespace verilog;

// Create a simple buffer module
Module m = make_module("buffer");
add_input(m, "in", make_range(7, 0));
add_output(m, "out", make_range(7, 0));
add_assignment(m, make_identifier("out"), make_identifier("in"));

// Write to Verilog
std::string verilog = write_module(m);
```

Output:
```verilog
module buffer(in, out);
  input [7:0] in;

  output [7:0] out;

  assign out = in;
endmodule
```

### Modifying Parsed Verilog

```cpp
#include "veriloglib.hpp"
#include "verilog_builder.hpp"
#include "verilog_writer.hpp"

using namespace verilog;

// Parse existing Verilog
Netlist nl = parse_file("design.v");
Module& m = nl.modules[0];

// Add a new wire
add_wire(m, "debug_signal", make_range(3, 0));

// Add an assignment
add_assignment(m, 
    make_identifier("debug_signal"), 
    make_sliced("data_bus", 3, 0));

// Validate and write
auto errors = validate_module(m);
if (errors.empty()) {
    std::string output = write_netlist(nl);
}
```

## Construction Helpers

### Numbers

```cpp
// Simple integer (no base notation)
Number n1 = make_number(42);

// Sized hex number: 8'hFF
Number n2 = make_hex_number(8, "FF");

// Sized binary number: 4'b1010
Number n3 = make_bin_number(4, "1010");

// Custom base: 12'o377 (octal)
Number n4 = make_number(12, 'o', "377");
```

### Ranges

```cpp
// [7:0]
Range r = make_range(7, 0);
```

### Expressions

```cpp
// Simple identifier
Expr e1 = make_identifier("signal");

// Bit-select: bus[3]
Expr e2 = make_indexed("bus", 3);

// Part-select: data[7:0]
Expr e3 = make_sliced("data", 7, 0);

// Concatenation: {a, b, c[1:0]}
Expr e4 = make_concatenation({
    make_identifier("a"),
    make_identifier("b"),
    make_sliced("c", 1, 0)
});
```

### Identifier Escaping

The API automatically escapes identifiers that need it:

```cpp
// Automatically escaped to \123 
Expr e1 = make_identifier("123");

// Automatically escaped to \my-signal 
Expr e2 = make_identifier("my-signal");

// Keywords automatically escaped
Expr e3 = make_identifier("module");  // becomes \module 

// Valid identifiers not modified
Expr e4 = make_identifier("valid_name");  // stays as valid_name
```

Check if escaping is needed:
```cpp
bool needs_it = needs_escaping("123");  // true
std::string escaped = escape_identifier("123");  // "\\123 "
```

## Module Construction and Modification

### Creating Modules

```cpp
Module m = make_module("my_design");
```

### Adding Declarations

All port declarations are automatically added to the module's port list:

```cpp
// Add input (automatically added to port_list)
add_input(m, "clk");
add_input(m, "data", make_range(7, 0));

// Add output (automatically added to port_list)
add_output(m, "result", make_range(15, 0));

// Add inout (automatically added to port_list)
add_inout(m, "bidirectional");

// Add internal wire (NOT added to port_list)
add_wire(m, "internal_signal", make_range(3, 0));
```

**Note:** Adding a signal that already exists throws `std::invalid_argument`.

### Adding Assignments

```cpp
// Simple assignment: result = data
add_assignment(m, make_identifier("result"), make_identifier("data"));

// With bit-select: out = bus[5]
add_assignment(m, make_identifier("out"), make_indexed("bus", 5));

// With part-select: slice = bus[7:4]
add_assignment(m, 
    make_identifier("slice"), 
    make_sliced("bus", 7, 4));
```

### Adding Module Instances

#### Positional Port Connections

```cpp
ModuleInstance inst = make_instance("adder", "u0", {
    make_identifier("a"),
    make_identifier("b"),
    make_identifier("sum")
});
add_instance(m, inst);
```

Output:
```verilog
adder u0(a, b, sum);
```

#### Named Port Connections

```cpp
ModuleInstance inst = make_instance_named("adder", "u1", {
    {"a", make_identifier("input1")},
    {"b", make_identifier("input2")},
    {"sum", make_identifier("output1")}
});
add_instance(m, inst);
```

Output:
```verilog
adder u1(
  .a(input1),
  .b(input2),
  .sum(output1)
);
```

### Removing Elements

```cpp
// Remove declarations (also removes from port_list)
bool removed = remove_input(m, "old_signal");
remove_output(m, "unused_output");
remove_wire(m, "unused_wire");

// Remove instance
remove_instance(m, "old_instance");

// Remove from port list only (keeps declaration)
remove_from_port_list(m, "signal");
```

### Query Functions

```cpp
// Check if signals exist
bool has_it = has_port(m, "clk");        // checks input/output/inout
bool is_wire = has_wire(m, "internal");   // checks wire only
bool exists = has_signal(m, "any");       // checks all declarations

// Find instances
const ModuleInstance* inst = find_instance(m, "u0");
std::vector<const ModuleInstance*> adders = 
    find_instances_of_type(m, "adder");
```

## Netlist Operations

```cpp
Netlist nl;

// Add modules
Module m1 = make_module("leaf");
Module m2 = make_module("top");

add_module(nl, m1);  // Throws if duplicate module name
add_module(nl, m2);

// Find modules
Module* found = find_module(nl, "leaf");
if (found) {
    // Modify found module
    add_wire(*found, "new_wire");
}
```

## Writer API

### Basic Writing

```cpp
// Write single module
std::string verilog = write_module(m);

// Write entire netlist
std::string verilog = write_netlist(nl);
```

### Writer Options

```cpp
WriterOptions opts;
opts.indent_size = 4;              // Default: 2
opts.use_tabs = false;             // Default: false
opts.max_port_list_length = 80;   // Default: 80
opts.one_declaration_per_line = true;  // Default: true
opts.validate_before_write = false;    // Default: false

std::string verilog = write_module(m, opts);
```

### Formatting Individual Components

```cpp
Number n = make_hex_number(8, "FF");
std::string num_str = write_number(n);  // "8'hFF"

Range r = make_range(7, 0);
std::string range_str = write_range(r);  // "[7:0]"

Expr e = make_indexed("bus", 3);
std::string expr_str = write_expr(e);  // "bus[3]"
```

## Validation API

### Validating Structures

```cpp
// Validate single module
std::vector<ValidationError> errors = validate_module(m);

// Validate entire netlist
std::vector<ValidationError> errors = validate_netlist(nl);

// Check errors
for (const auto& err : errors) {
    if (err.severity == ValidationError::Severity::ERROR) {
        std::cerr << "ERROR at " << err.location << ": " 
                  << err.message << "\n";
    } else {
        std::cerr << "WARNING at " << err.location << ": " 
                  << err.message << "\n";
    }
}
```

### Automatic Validation

```cpp
WriterOptions opts;
opts.validate_before_write = true;

try {
    std::string verilog = write_module(m, opts);
} catch (const parse_error& e) {
    std::cerr << "Validation failed: " << e.what() << "\n";
}
```

### Validation Rules

**Module-level checks (ERROR):**
- No duplicate signal names across all declarations
- No duplicate names in port_list
- All port_list entries have corresponding declarations
- Module name is valid identifier
- No nested sub_modules

**Module-level checks (WARNING):**
- Port declarations not in port_list

**Instance validation (ERROR):**
- Instance names unique within module
- Connection expressions reference declared signals

**Instance validation (WARNING):**
- No mixing positional and named connections

**Assignment validation (ERROR):**
- LHS/RHS expressions reference declared signals

**Netlist-level checks (ERROR):**
- No duplicate module names

**Netlist-level checks (WARNING):**
- Instantiated modules exist in netlist

## Canonical Output Style

The writer produces consistently formatted Verilog:

```verilog
module module_name(port1, port2, port3);
  input [7:0] port1;
  input port2;

  output port3;

  wire [3:0] internal_wire;

  assign port3 = port2;

  submodule_type inst_name(
    .port_a(port1),
    .port_b(internal_wire[2])
  );

  another_module u0(port1, port2);
endmodule
```

**Style characteristics:**
- 2-space indentation (configurable)
- One declaration per line
- Blank lines between declaration types
- Named connections: one per line with indent
- Positional connections: one line if short
- Ranges always in `[msb:lsb]` format
- Numbers preserve original base notation

## Complete Example: Creating a Full Design

```cpp
#include "verilog_builder.hpp"
#include "verilog_writer.hpp"

using namespace verilog;

int main() {
    Netlist nl;
    
    // Create leaf module
    Module leaf = make_module("register_slice");
    add_input(leaf, "clk");
    add_input(leaf, "reset");
    add_input(leaf, "data_in", make_range(7, 0));
    add_output(leaf, "data_out", make_range(7, 0));
    add_module(nl, leaf);
    
    // Create top module
    Module top = make_module("design_top");
    add_input(top, "sys_clk");
    add_input(top, "sys_reset");
    add_input(top, "input_data", make_range(7, 0));
    add_output(top, "output_data", make_range(7, 0));
    add_wire(top, "stage1", make_range(7, 0));
    add_wire(top, "stage2", make_range(7, 0));
    
    // Add instances
    auto u0 = make_instance_named("register_slice", "stage1_reg", {
        {"clk", make_identifier("sys_clk")},
        {"reset", make_identifier("sys_reset")},
        {"data_in", make_identifier("input_data")},
        {"data_out", make_identifier("stage1")}
    });
    add_instance(top, u0);
    
    auto u1 = make_instance_named("register_slice", "stage2_reg", {
        {"clk", make_identifier("sys_clk")},
        {"reset", make_identifier("sys_reset")},
        {"data_in", make_identifier("stage1")},
        {"data_out", make_identifier("stage2")}
    });
    add_instance(top, u1);
    
    // Final assignment
    add_assignment(top, 
        make_identifier("output_data"), 
        make_identifier("stage2"));
    
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

## Error Handling

### Construction-time Errors

Operations that detect invalid state during construction throw `std::invalid_argument`:

```cpp
try {
    add_input(m, "signal");
    add_output(m, "signal");  // Throws: duplicate signal
} catch (const std::invalid_argument& e) {
    std::cerr << "Construction error: " << e.what() << "\n";
}
```

### Validation Errors

Validation returns a vector of errors that can be inspected:

```cpp
auto errors = validate_module(m);
for (const auto& err : errors) {
    std::cerr << "[" << (err.severity == ValidationError::Severity::ERROR 
                          ? "ERROR" : "WARNING")
              << "] " << err.location << ": " << err.message << "\n";
}
```

### Write-time Errors

Writing with validation enabled throws `parse_error` on validation failure:

```cpp
WriterOptions opts;
opts.validate_before_write = true;

try {
    std::string verilog = write_module(m, opts);
} catch (const parse_error& e) {
    std::cerr << "Write failed: " << e.what() << "\n";
}
```

Nested sub_modules always throw during write:

```cpp
try {
    std::string verilog = write_module(m);  // Throws if m.sub_modules not empty
} catch (const parse_error& e) {
    std::cerr << "Error: " << e.what() << "\n";
}
```

## Round-Trip Workflow

The API supports full round-trip processing:

```cpp
// Parse -> Modify -> Write -> Parse
Netlist nl = parse_file("original.v");

// Modify
Module& m = nl.modules[0];
add_wire(m, "debug");
add_assignment(m, make_identifier("debug"), make_identifier("some_signal"));

// Write
std::string modified_verilog = write_netlist(nl);

// Parse again to verify
Netlist nl2 = parse_string(modified_verilog);
assert(nl2.modules[0].net_declarations.size() > nl.modules[0].net_declarations.size());
```

## Best Practices

1. **Always validate before writing critical output:**
   ```cpp
   auto errors = validate_module(m);
   if (errors.empty()) {
       write_module(m);
   }
   ```

2. **Use construction helpers for consistency:**
   ```cpp
   // Good: Uses helpers
   add_input(m, "signal", make_range(7, 0));
   
   // Avoid: Direct manipulation
   InputDeclaration d;
   d.net_name = "signal";
   m.input_declarations.push_back(d);
   m.port_list.push_back("signal");  // Easy to forget!
   ```

3. **Check for existence before operations:**
   ```cpp
   if (!has_signal(m, "new_signal")) {
       add_wire(m, "new_signal");
   }
   ```

4. **Handle escaped identifiers consistently:**
   ```cpp
   // The API handles escaping, just use original names
   add_input(m, "123_signal");  // Automatically escaped
   ```

5. **Use named connections for clarity:**
   ```cpp
   // Preferred: Clear port mappings
   auto inst = make_instance_named("module", "u0", {
       {"in", make_identifier("sig1")},
       {"out", make_identifier("sig2")}
   });
   ```

## File Organization

```
include/
  veriloglib.hpp          - Core parser API (existing)
  verilog_builder.hpp     - Construction/modification API (new)
  verilog_writer.hpp      - Writer and validation API (new)

src/
  veriloglib.cpp          - Core parser implementation (existing)
  verilog_builder.cpp     - Construction/modification implementation (new)
  verilog_writer.cpp      - Writer implementation (new)
  verilog_validator.cpp   - Validation implementation (new)
```
