TEST FILE FIXES
===============

Fixed the existing test_verilog.cpp file to work with the updated AST.

ISSUES FIXED
------------

1. NetDeclaration field access
   OLD: m.net_declarations[0].msb
   OLD: m.net_declarations[0].lsb
   
   NEW: m.net_declarations[0].range.has_value()
   NEW: m.net_declarations[0].range.value().start.as_integer()
   NEW: m.net_declarations[0].range.value().end.as_integer()
   
   REASON: NetDeclaration stores an optional<Range>, not direct msb/lsb fields

2. Concatenation type name
   OLD: std::holds_alternative<verilog::Concat>(expr)
   OLD: std::get<verilog::Concat>(expr)
   
   NEW: std::holds_alternative<std::shared_ptr<verilog::Concatenation>>(expr)
   NEW: std::get<std::shared_ptr<verilog::Concatenation>>(expr)
   
   REASON: The type is std::shared_ptr<Concatenation>, not Concat

3. Concatenation access
   OLD: concat.elements
   NEW: concat_ptr->elements
   
   REASON: Now using a pointer, so need -> instead of .


TESTS FIXED
-----------

1. TEST(Parse, NumberWithXValue)
   - Fixed: .msb/.lsb → .range access

2. TEST(Parse, ConcatenationOnRHS)
   - Fixed: Concat → std::shared_ptr<Concatenation>
   - Fixed: concat.elements → concat_ptr->elements

3. TEST(Parse, NumbersInConcatenation)
   - Fixed: .msb/.lsb → .range access
   - Fixed: Concat → std::shared_ptr<Concatenation>
   - Fixed: concat.elements → concat_ptr->elements


INSTALLATION
------------

Replace your existing tests/test_verilog.cpp with the fixed version:

  cp test_verilog.cpp /path/to/veriloglib/tests/

Then recompile and test:

  cd build
  make
  make test


WHAT NOW WORKS
--------------

All tests should now compile and run successfully, including:
✓ Number literal tests
✓ Concatenation tests
✓ All existing tests with proper field access
