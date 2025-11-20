# Verilog Writer/Builder Implementation Summary

## Overview
This implementation extends the existing Verilog parser to support creating, modifying, and writing Verilog modules programmatically. The implementation follows all specified requirements and includes comprehensive unit tests.

## New Files Created

### Header Files
1. **include/verilog_builder.hpp**
   - Construction helpers for Numbers, Ranges, Expressions
   - Module creation and modification API
   - Identifier escaping utilities
   - Netlist operations
   - Query functions for checking signal/instance existence

2. **include/verilog_writer.hpp**
   - WriterOptions configuration struct
   - ValidationError reporting struct
   - Writer API for converting data structures to Verilog text
   - Validation API for structure verification
   - VerilogWriter class for internal formatting

### Implementation Files
3. **src/verilog_builder.cpp**
   - Implementation of all construction helpers
   - Automatic identifier escaping
   - Signal existence checking
   - Add/remove/query operations with proper error handling

4. **src/verilog_writer.cpp**
   - VerilogWriter class implementation
   - Canonical formatting with configurable indentation
   - Number/range/expression formatting
   - Module and netlist writing

5. **src/verilog_validator.cpp**
   - Module structure validation
   - Netlist-level validation
   - Error and warning generation
   - Signal reference checking

### Documentation
6. **doc/WRITER_API.md**
   - Comprehensive API documentation
   - Usage examples for all features
   - Best practices guide
   - Complete working examples

## Modified Files

1. **CMakeLists.txt**
   - Added new source files to veriloglib target

2. **tests/test_verilog.cpp**
   - Added includes for new headers
   - Added 30+ new unit tests covering:
     - Number/range/expression construction
     - Module creation and modification
     - Instance construction (positional and named)
     - Add/remove/query operations
     - Writer API with various options
     - Validation API
     - Round-trip testing (parse -> modify -> write -> parse)
     - Error handling

## Implementation Details

### Design Decisions Implemented

1. **API Style**: Direct construction with helper functions
   - `make_*()` functions for creating objects
   - `add_*()` functions for modifying modules
   - Clean, functional-style API

2. **Validation**: Two-level approach
   - Construction-time validation: Throws `std::invalid_argument` for immediate errors
   - Explicit validation: Returns `std::vector<ValidationError>` for inspection
   - Optional automatic validation before write

3. **Formatting**: Canonical style with 2-space indentation
   - Consistent output format
   - Configurable via WriterOptions
   - Proper indentation and spacing

4. **Identifier Handling**: Automatic escaping
   - Keywords automatically escaped
   - Invalid characters detected and escaped
   - Transparent to user

5. **Number Representation**: Preserve original form
   - If base and length present, outputs sized notation (8'hFF)
   - Otherwise outputs simple decimal
   - Maintains user intent

6. **Modification Support**: Full API for structure manipulation
   - Add/remove signals, instances, assignments
   - Query functions for existence checking
   - Automatic port_list management

7. **Sub-modules**: Error on write if present
   - `check_no_sub_modules()` throws parse_error
   - Clear error message to user

8. **Error Handling**: Multi-level approach
   - Construction errors: throw immediately
   - Validation errors: return vector
   - Write errors: throw on structural issues

### Key Features

1. **Automatic Port List Management**
   - `add_input/output/inout` automatically adds to port_list
   - `add_wire` does NOT add to port_list
   - Automatic removal from port_list when ports removed

2. **Smart Identifier Escaping**
   - Detects keywords, invalid characters, numeric prefixes
   - Automatically adds backslash and space
   - Transparent to user

3. **Comprehensive Validation**
   - Duplicate signal detection
   - Undeclared signal reference detection
   - Port list consistency checking
   - Instance name uniqueness
   - Module name uniqueness in netlist
   - Mixed connection style warnings

4. **Flexible Expression Construction**
   - Simple identifiers
   - Bit-select (indexed)
   - Part-select (sliced)
   - Concatenations (nested support)

5. **Canonical Output Format**
   - Consistent indentation
   - Blank lines between sections
   - One declaration per line
   - Named ports on separate lines
   - Positional ports on one line (if short)

## Testing Coverage

### Unit Tests Added (30+ tests)

**Builder API Tests:**
- Number construction (simple, hex, binary, custom base)
- Range construction
- Identifier escaping (keywords, invalid chars)
- Expression construction (all types)
- Module creation
- Adding declarations (inputs, outputs, inouts, wires)
- Adding assignments and instances
- Instance construction (positional and named)
- Remove operations
- Query functions
- Netlist operations

**Writer API Tests:**
- Simple module writing
- Modules with ranges
- Modules with assignments
- Modules with instances
- Complex expressions (bit-select, part-select)
- Number formatting
- Range formatting
- Expression formatting
- Netlist writing
- Writer options (indentation)

**Validation Tests:**
- Valid module validation
- Duplicate signal detection
- Undeclared signal in assignment
- Duplicate module names in netlist
- Automatic validation before write

**Round-Trip Tests:**
- Parse -> modify -> write -> parse
- Create -> write -> parse

## Code Quality

1. **Compilation**: All source files compile cleanly with g++ -std=c++20
2. **Type Safety**: Strong typing throughout, proper use of std::optional
3. **Error Handling**: Consistent error handling strategy
4. **Documentation**: Comprehensive API documentation with examples
5. **Code Style**: Consistent with existing codebase

## API Summary

### Construction (verilog_builder.hpp)
```cpp
// Numbers & Ranges
Number make_number(int64_t value);
Number make_hex_number(int length, const std::string& hex_digits);
Range make_range(int64_t msb, int64_t lsb);

// Expressions
Expr make_identifier(const std::string& name);
Expr make_indexed(const std::string& name, int64_t index);
Expr make_sliced(const std::string& name, int64_t msb, int64_t lsb);
Expr make_concatenation(const std::vector<Expr>& elements);

// Module Construction
Module make_module(const std::string& name);
void add_input(Module& m, const std::string& name, std::optional<Range> range);
void add_output(Module& m, const std::string& name, std::optional<Range> range);
void add_wire(Module& m, const std::string& name, std::optional<Range> range);
void add_assignment(Module& m, const Expr& lhs, const Expr& rhs);
void add_instance(Module& m, const ModuleInstance& inst);

// Instances
ModuleInstance make_instance(const std::string& module_name,
                             const std::string& instance_name,
                             const std::vector<Expr>& positional_ports);
ModuleInstance make_instance_named(const std::string& module_name,
                                   const std::string& instance_name,
                                   const std::map<std::string, Expr>& named_ports);

// Removal
bool remove_input(Module& m, const std::string& name);
bool remove_output(Module& m, const std::string& name);
bool remove_wire(Module& m, const std::string& name);
bool remove_instance(Module& m, const std::string& instance_name);

// Queries
bool has_signal(const Module& m, const std::string& name);
const ModuleInstance* find_instance(const Module& m, const std::string& instance_name);

// Netlist
void add_module(Netlist& nl, const Module& m);
Module* find_module(Netlist& nl, const std::string& module_name);
```

### Writing & Validation (verilog_writer.hpp)
```cpp
// Writer Options
struct WriterOptions {
    int indent_size = 2;
    bool use_tabs = false;
    int max_port_list_length = 80;
    bool one_declaration_per_line = true;
    bool validate_before_write = false;
};

// Validation
struct ValidationError {
    enum class Severity { ERROR, WARNING };
    std::string message;
    std::string location;
    Severity severity;
};

std::vector<ValidationError> validate_module(const Module& m);
std::vector<ValidationError> validate_netlist(const Netlist& nl);

// Writing
std::string write_module(const Module& m, const WriterOptions& opts = {});
std::string write_netlist(const Netlist& nl, const WriterOptions& opts = {});
std::string write_number(const Number& n);
std::string write_range(const Range& r);
std::string write_expr(const Expr& e);
```

## Usage Example

```cpp
#include "verilog_builder.hpp"
#include "verilog_writer.hpp"

using namespace verilog;

int main() {
    // Create module
    Module m = make_module("adder");
    add_input(m, "a", make_range(7, 0));
    add_input(m, "b", make_range(7, 0));
    add_output(m, "sum", make_range(8, 0));
    
    // Validate
    auto errors = validate_module(m);
    if (!errors.empty()) {
        for (const auto& err : errors) {
            std::cerr << err.message << "\n";
        }
        return 1;
    }
    
    // Write
    std::cout << write_module(m);
    return 0;
}
```

## Next Steps

To build and test:
```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Note: Network access to GitHub is required for PEGTL and GoogleTest dependencies.

## Files Ready for Delivery

All files are in `/home/claude/veriloglib/`:
- include/verilog_builder.hpp
- include/verilog_writer.hpp
- src/verilog_builder.cpp
- src/verilog_writer.cpp
- src/verilog_validator.cpp
- doc/WRITER_API.md
- CMakeLists.txt (modified)
- tests/test_verilog.cpp (modified)
