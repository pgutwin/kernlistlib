#include "veriloglib.hpp"
#include "verilog_builder.hpp"
#include "verilog_writer.hpp"
#include <gtest/gtest.h>

using namespace verilog;

// The connection to a port, by name; nullptr if there is none
static const Connection* conn(const ModuleInstance& inst, const std::string& port) {
  for (const auto& c : inst.connections) if (c.port == port) return &c;
  return nullptr;
}

static std::string id_of(const Expr& e) {
  if (std::holds_alternative<Identifier>(e)) return std::get<Identifier>(e).name;
  return expr_to_string(e);
}

TEST(Parse, SimpleModule) {
  std::string data = R"(
kernlist_format 1;
module top(a,b,c,d);
  input a, b;
  output c, d;
  wire w1;
  assign c = a;
endmodule
)";
  auto nl = parse_string(data);
  EXPECT_EQ(nl.format_version, 1);
  ASSERT_EQ(nl.modules.size(), 1u);
  const auto& m = nl.modules[0];
  EXPECT_EQ(m.module_name, "top");
  EXPECT_EQ(m.port_list.size(), 4u);
  EXPECT_EQ(m.input_declarations.size(), 2u);
  EXPECT_EQ(m.input_declarations[0].net_name, "a");
  EXPECT_EQ(m.input_declarations[1].net_name, "b");
  EXPECT_EQ(m.output_declarations.size(), 2u);
  EXPECT_EQ(m.output_declarations[0].net_name, "c");
  EXPECT_EQ(m.output_declarations[1].net_name, "d");
  EXPECT_EQ(m.net_declarations.size(), 1u);
  EXPECT_EQ(m.assignments.size(), 1u);
  // locations: line 3 is the module, line 6 the wire, line 7 the assign
  EXPECT_EQ(m.location.line, 3);
  EXPECT_EQ(m.net_declarations[0].location.line, 6);
  EXPECT_EQ(m.assignments[0].location.line, 7);
}

TEST(Parse, FormatLineRequired) {
  EXPECT_THROW(parse_string("module m(); endmodule"), parse_error);
  try {
    parse_string("\nmodule m(); endmodule");
    FAIL();
  } catch (const parse_error& e) {
    EXPECT_EQ(e.line, 2);
    EXPECT_NE(std::string(e.what()).find("kernlist_format"), std::string::npos);
  }
}

TEST(Parse, FormatVersionRead) {
  // The reader reads any version; checking refuses versions it does not know (KL03)
  auto nl = parse_string("kernlist_format 2; module m(); endmodule");
  EXPECT_EQ(nl.format_version, 2);
}

TEST(Parse, InoutRejected) {
  EXPECT_THROW(parse_string("kernlist_format 1;\nmodule test(A);\n  inout A;\nendmodule\n"), parse_error);
}

TEST(Parse, EscapedNames) {
  std::string data = R"(
kernlist_format 1;
module top(a,\b_$123 ,c,\in.d );
  input a, \b_$123 ;
  output c, \in.d ;
endmodule
)";
  auto nl = parse_string(data);
  ASSERT_EQ(nl.modules.size(), 1u);
  const auto& m = nl.modules[0];
  EXPECT_EQ(m.port_list.size(), 4u);
  EXPECT_EQ(m.input_declarations[1].net_name, "b_$123");
  EXPECT_EQ(m.output_declarations[1].net_name, "in.d");
}

TEST(Parse, KeywordsAreNotNames) {
  EXPECT_THROW(parse_string("kernlist_format 1; module m(); wire variant; endmodule"), parse_error);
  EXPECT_THROW(parse_string("kernlist_format 1; module m(); wire model; endmodule"), parse_error);
  // but names that start with a keyword are names
  auto nl = parse_string("kernlist_format 1; module m(); wire variant_x, model_y; endmodule");
  EXPECT_EQ(nl.modules[0].net_declarations.size(), 2u);
}

TEST(Parse, NumbersAndConcatenation) {
  std::string data = R"(
kernlist_format 1;
module test4();
  wire [3:0] a, b;
  wire [1:0] c;
  assign a = {2'h0, 2'hx}, b = {c, 2'b01};
endmodule
)";
  auto nl = parse_string(data);
  const auto& m = nl.modules[0];
  ASSERT_TRUE(m.net_declarations[0].range.has_value());
  EXPECT_EQ(m.net_declarations[0].range->start.as_integer(), 3);
  EXPECT_EQ(m.net_declarations[0].range->end.as_integer(), 0);
  ASSERT_EQ(m.assignments.size(), 1u);
  ASSERT_EQ(m.assignments[0].assignments.size(), 2u);
  const auto& rhs = m.assignments[0].assignments[0].second;
  ASSERT_TRUE(std::holds_alternative<std::shared_ptr<Concatenation>>(rhs));
  const auto& c = std::get<std::shared_ptr<Concatenation>>(rhs);
  ASSERT_EQ(c->elements.size(), 2u);
  ASSERT_TRUE(std::holds_alternative<Number>(c->elements[1]));
  const auto& n = std::get<Number>(c->elements[1]);
  EXPECT_EQ(n.length.value(), 2);
  EXPECT_EQ(n.base.value(), 'h');
  EXPECT_EQ(n.mantissa, "x");
}

TEST(Parse, Comments) {
  std::string data = R"(
// before the format line
/* also before */ kernlist_format 1; // after
module /*blk*/ test ( /*h*/ a /*h*/, b ) /*blk*/ ;
  // single line comment
  /* block comment */ wire w1; /* another */
  input /*c*/ a /*c*/ ;
  output b;
  assign /*A*/ b /*B*/ = /*C*/ a /*D*/ ;
endmodule
)";
  auto nl = parse_string(data);
  const auto& m = nl.modules[0];
  EXPECT_EQ(m.port_list.size(), 2u);
  EXPECT_EQ(m.net_declarations.size(), 1u);
  EXPECT_EQ(m.input_declarations.size(), 1u);
  EXPECT_EQ(m.output_declarations.size(), 1u);
  EXPECT_EQ(id_of(m.assignments[0].assignments[0].second), "a");
}

TEST(Parse, AttributesAreNotComments) {
  // veriloglib read (* ... *) as a comment; the kernlist format has no attributes
  EXPECT_THROW(parse_string("kernlist_format 1; module m(); (* keep *) wire w; endmodule"), parse_error);
}

TEST(Parse, UnclosedComment) {
  EXPECT_THROW(parse_string("kernlist_format 1; module m(); /* wire w; endmodule"), parse_error);
}

TEST(Parse, Instantiation) {
  std::string data = R"(
kernlist_format 1;
module leaf(x, y);
  input x; output y;
endmodule

module top(a, b);
  input a; output b;
  wire n1;
  leaf u1 (.x(n1), .y(b));
  leaf u2 (.x(a), .y());
  leaf u3 ();
endmodule
)";
  auto nl = parse_string(data);
  ASSERT_EQ(nl.modules.size(), 2u);
  const auto& top = nl.modules[1];
  ASSERT_EQ(top.module_instances.size(), 3u);
  const auto& u1 = top.module_instances[0];
  EXPECT_EQ(u1.module_name, "leaf");
  EXPECT_EQ(u1.instance_name, "u1");
  EXPECT_TRUE(u1.parameters.empty());
  EXPECT_FALSE(u1.variant.has_value());
  ASSERT_EQ(u1.connections.size(), 2u);
  EXPECT_EQ(u1.connections[0].port, "x");          // in the order written
  EXPECT_EQ(id_of(*conn(u1, "x")->expr), "n1");
  EXPECT_EQ(id_of(*conn(u1, "y")->expr), "b");
  const auto& u2 = top.module_instances[1];
  ASSERT_NE(conn(u2, "y"), nullptr);
  EXPECT_FALSE(conn(u2, "y")->expr.has_value());   // left unconnected
  EXPECT_TRUE(top.module_instances[2].connections.empty());
  EXPECT_EQ(u1.location.line, 10);
}

TEST(Parse, PositionalConnectionsRejected) {
  EXPECT_THROW(parse_string("kernlist_format 1; module m(a); input a; leaf u0 (a); endmodule"), parse_error);
}

TEST(Parse, ParametersAndVariant) {
  std::string data = R"(
kernlist_format 1;
module top();
  fifo #(.DEPTH(2), .W(8)) variant circular_buffer requests_0 (.enqValid(1'b0));
  fifo #(.DEPTH(4), .W(8)) requests_1 ();
  probe variant passive watch ();
endmodule
)";
  auto nl = parse_string(data);
  const auto& insts = nl.modules[0].module_instances;
  ASSERT_EQ(insts.size(), 3u);
  ASSERT_EQ(insts[0].parameters.size(), 2u);
  EXPECT_EQ(insts[0].parameters[0].name, "DEPTH");
  EXPECT_EQ(insts[0].parameters[0].value, 2);
  EXPECT_EQ(insts[0].parameters[1].name, "W");
  EXPECT_EQ(insts[0].parameters[1].value, 8);
  EXPECT_EQ(insts[0].variant.value(), "circular_buffer");
  EXPECT_EQ(insts[0].instance_name, "requests_0");
  EXPECT_FALSE(insts[1].variant.has_value());
  EXPECT_TRUE(insts[2].parameters.empty());
  EXPECT_EQ(insts[2].variant.value(), "passive");
}

TEST(Parse, VariantErrors) {
  // variant without a name: the instance name is then missing
  EXPECT_THROW(parse_string("kernlist_format 1; module m(); sink #(.W(1)) variant consume (); endmodule"), parse_error);
  // variant before the parameters
  EXPECT_THROW(parse_string("kernlist_format 1; module m(); sink variant plain #(.W(8)) consume (); endmodule"), parse_error);
  // a parameter that is not a decimal number, or not by name
  EXPECT_THROW(parse_string("kernlist_format 1; module m(); sink #(.W(N)) consume (); endmodule"), parse_error);
  EXPECT_THROW(parse_string("kernlist_format 1; module m(); sink #(8) consume (); endmodule"), parse_error);
}

TEST(Parse, Selects) {
  std::string data = R"(
kernlist_format 1;
module top(sig, bus, counter_out);
  input sig;
  input [7:0] bus;
  output [1:0] counter_out;
  BUF2 u0 (.A(sig), .Y(counter_out[1]), .Z(bus[7:4]));
endmodule
)";
  auto nl = parse_string(data);
  const auto& u0 = nl.modules[0].module_instances[0];
  const Expr& y = *conn(u0, "Y")->expr;
  ASSERT_TRUE(std::holds_alternative<IdentifierIndexed>(y));
  EXPECT_EQ(std::get<IdentifierIndexed>(y).name, "counter_out");
  EXPECT_EQ(std::get<IdentifierIndexed>(y).index.as_integer(), 1);
  const Expr& z = *conn(u0, "Z")->expr;
  ASSERT_TRUE(std::holds_alternative<IdentifierSliced>(z));
  EXPECT_EQ(std::get<IdentifierSliced>(z).range.start.as_integer(), 7);
  EXPECT_EQ(std::get<IdentifierSliced>(z).range.end.as_integer(), 4);
}

// The expression as the writer prints it, with all parentheses that show grouping
static std::string grouped(const Expr& e) {
  if (auto b = std::get_if<std::shared_ptr<BinaryOp>>(&e))
    return "(" + grouped((*b)->lhs) + " " + (*b)->op + " " + grouped((*b)->rhs) + ")";
  if (auto u = std::get_if<std::shared_ptr<UnaryOp>>(&e))
    return "(" + (*u)->op + grouped((*u)->operand) + ")";
  if (auto c = std::get_if<std::shared_ptr<Choice>>(&e))
    return "(" + grouped((*c)->condition) + " ? " + grouped((*c)->if_true) + " : " + grouped((*c)->if_false) + ")";
  return expr_to_string(e);
}

static std::string read_rhs(const std::string& rhs) {
  auto nl = parse_string("kernlist_format 1; module m(); assign y = " + rhs + "; endmodule");
  return grouped(nl.modules[0].assignments[0].assignments[0].second);
}

TEST(Parse, OperatorPrecedence) {
  EXPECT_EQ(read_rhs("a | b ^ c & d"), "(a | (b ^ (c & d)))");
  EXPECT_EQ(read_rhs("a & b == c"), "(a & (b == c))");
  EXPECT_EQ(read_rhs("a == b + c"), "(a == (b + c))");
  EXPECT_EQ(read_rhs("a < b + 1"), "(a < (b + 1))");
  EXPECT_EQ(read_rhs("a - b - c"), "((a - b) - c)");
  EXPECT_EQ(read_rhs("c ? a | b : d"), "(c ? (a | b) : d)");
  EXPECT_EQ(read_rhs("a -> b == 2'd3"), "(a -> (b == 2'd3))");
  EXPECT_EQ(read_rhs("~a & &b"), "((~a) & (&b))");
  EXPECT_EQ(read_rhs("(a | b) & c"), "((a | b) & c)");
  EXPECT_EQ(read_rhs("{2{a, b[1:0]}}"), "{2{a, b[1:0]}}");
  EXPECT_EQ(read_rhs("model(credits.taken) + 1"), "(model(credits.taken) + 1)");
  EXPECT_EQ(read_rhs("a->b"), "(a -> b)");
}

TEST(Parse, OperatorsNotInTheFormat) {
  // Forms in no part of the format are syntax errors (KL13)
  for (const char* rhs : { "a && b", "a || b", "a << 1", "a >> 1", "a * b", "~&a", "a === b", "a -> b -> c" }) {
    EXPECT_THROW(read_rhs(rhs), parse_error) << rhs;
  }
}

TEST(Parse, Invariants) {
  std::string data = R"(
kernlist_format 1;
module m(take);
  input take;
  wire available;
  invariant implication: take -> available;
  invariant accounted:
    model(credits_0.taken) == model(bank.inside[0]) + model(responses_0.held);
endmodule
)";
  auto nl = parse_string(data);
  const auto& invs = nl.modules[0].invariants;
  ASSERT_EQ(invs.size(), 2u);
  EXPECT_EQ(invs[0].name, "implication");
  EXPECT_EQ(grouped(invs[0].expr), "(take -> available)");
  EXPECT_EQ(invs[1].location.line, 7);
  EXPECT_EQ(grouped(invs[1].expr), "(model(credits_0.taken) == (model(bank.inside[0]) + model(responses_0.held)))");
}

TEST(Parse, SyntaxErrorPosition) {
  try {
    parse_string("kernlist_format 1;\nmodule m(a, y);\n  input a;\n  output y;\n  assign y = a << 1;\nendmodule\n");
    FAIL();
  } catch (const parse_error& e) {
    EXPECT_EQ(e.line, 5);
    EXPECT_EQ(e.column, 16);
    EXPECT_NE(std::string(e.what()).find("expected ';'"), std::string::npos) << e.what();
  }
}

// ============================================================================
// Builder API Tests
// ============================================================================

TEST(Builder, NumberConstruction) {
  using namespace verilog;
  
  // Simple integer
  auto n1 = make_number(42);
  EXPECT_EQ(n1.as_integer(), 42);
  EXPECT_FALSE(n1.base.has_value());
  
  // Hex number
  auto n2 = make_hex_number(8, "FF");
  EXPECT_EQ(n2.length.value(), 8);
  EXPECT_EQ(n2.base.value(), 'h');
  EXPECT_EQ(n2.mantissa, "FF");
  
  // Binary number
  auto n3 = make_bin_number(4, "1010");
  EXPECT_EQ(n3.length.value(), 4);
  EXPECT_EQ(n3.base.value(), 'b');
  EXPECT_EQ(n3.mantissa, "1010");
}

TEST(Builder, RangeConstruction) {
  using namespace verilog;
  
  auto r = make_range(7, 0);
  EXPECT_EQ(r.start.as_integer(), 7);
  EXPECT_EQ(r.end.as_integer(), 0);
}

TEST(Builder, IdentifierEscaping) {
  using namespace verilog;
  
  EXPECT_FALSE(needs_escaping("valid_name"));
  EXPECT_FALSE(needs_escaping("_underscore"));
  EXPECT_FALSE(needs_escaping("name123"));
  EXPECT_FALSE(needs_escaping("my$signal"));
  
  EXPECT_TRUE(needs_escaping("123invalid"));
  EXPECT_TRUE(needs_escaping("my-signal"));
  EXPECT_TRUE(needs_escaping("module"));
  EXPECT_TRUE(needs_escaping("wire"));
  
  EXPECT_EQ(escape_identifier("valid"), "valid");
  EXPECT_EQ(escape_identifier("123"), "\\123 ");
  EXPECT_EQ(escape_identifier("my-signal"), "\\my-signal ");
}

TEST(Builder, ExpressionConstruction) {
  using namespace verilog;
  
  // Simple identifier
  auto e1 = make_identifier("signal");
  ASSERT_TRUE(std::holds_alternative<Identifier>(e1));
  EXPECT_EQ(std::get<Identifier>(e1).name, "signal");
  
  // Indexed
  auto e2 = make_indexed("bus", 3);
  ASSERT_TRUE(std::holds_alternative<IdentifierIndexed>(e2));
  auto& ii = std::get<IdentifierIndexed>(e2);
  EXPECT_EQ(ii.name, "bus");
  EXPECT_EQ(ii.index.as_integer(), 3);
  
  // Sliced
  auto e3 = make_sliced("data", 7, 0);
  ASSERT_TRUE(std::holds_alternative<IdentifierSliced>(e3));
  auto& is = std::get<IdentifierSliced>(e3);
  EXPECT_EQ(is.name, "data");
  EXPECT_EQ(is.range.start.as_integer(), 7);
  EXPECT_EQ(is.range.end.as_integer(), 0);
  
  // Concatenation
  auto e4 = make_concatenation({make_identifier("a"), make_identifier("b")});
  ASSERT_TRUE(std::holds_alternative<std::shared_ptr<Concatenation>>(e4));
  auto& concat = std::get<std::shared_ptr<Concatenation>>(e4);
  EXPECT_EQ(concat->elements.size(), 2u);
}

TEST(Builder, ModuleConstruction) {
  using namespace verilog;
  
  auto m = make_module("test_mod");
  EXPECT_EQ(m.module_name, "test_mod");
  EXPECT_TRUE(m.port_list.empty());
  EXPECT_TRUE(m.input_declarations.empty());
}

TEST(Builder, AddDeclarations) {
  using namespace verilog;
  
  Module m = make_module("top");
  
  // Add input without range
  add_input(m, "clk");
  EXPECT_EQ(m.input_declarations.size(), 1u);
  EXPECT_EQ(m.input_declarations[0].net_name, "clk");
  EXPECT_FALSE(m.input_declarations[0].range.has_value());
  EXPECT_EQ(m.port_list.size(), 1u);
  EXPECT_EQ(m.port_list[0], "clk");
  
  // Add output with range
  add_output(m, "data", make_range(7, 0));
  EXPECT_EQ(m.output_declarations.size(), 1u);
  EXPECT_EQ(m.output_declarations[0].net_name, "data");
  EXPECT_TRUE(m.output_declarations[0].range.has_value());
  EXPECT_EQ(m.port_list.size(), 2u);
  
  // Add wire (not in port list)
  add_wire(m, "internal");
  EXPECT_EQ(m.net_declarations.size(), 1u);
  EXPECT_EQ(m.port_list.size(), 2u); // Still 2, wire not added to port list
  
  // Try to add duplicate - should throw
  EXPECT_THROW(add_input(m, "clk"), std::invalid_argument);
}

TEST(Builder, AddAssignmentAndInstance) {
  using namespace verilog;
  
  Module m = make_module("top");
  add_input(m, "a");
  add_output(m, "b");
  
  // Add assignment
  add_assignment(m, make_identifier("b"), make_identifier("a"));
  EXPECT_EQ(m.assignments.size(), 1u);
  EXPECT_EQ(m.assignments[0].assignments.size(), 1u);
  
  // Add instance
  auto inst = make_instance("buffer", "u0", {Connection{"in", make_identifier("a")}, Connection{"out", make_identifier("b")}});
  add_instance(m, inst);
  EXPECT_EQ(m.module_instances.size(), 1u);
  EXPECT_EQ(m.module_instances[0].instance_name, "u0");
  
  // Try to add duplicate instance
  EXPECT_THROW(add_instance(m, inst), std::invalid_argument);
}

TEST(Builder, InstanceConstruction) {
  using namespace verilog;
  
  // In the order given
  auto inst1 = make_instance("leaf", "u0", {Connection{"y", make_identifier("b")}, Connection{"x", make_identifier("a")}});
  EXPECT_EQ(inst1.module_name, "leaf");
  EXPECT_EQ(inst1.instance_name, "u0");
  ASSERT_EQ(inst1.connections.size(), 2u);
  EXPECT_EQ(inst1.connections[0].port, "y");
  
  // Named
  auto inst2 = make_instance_named("leaf", "u1", {
    {"x", make_identifier("a")},
    {"y", make_identifier("b")}
  });
  EXPECT_EQ(inst2.module_name, "leaf");
  EXPECT_EQ(inst2.instance_name, "u1");
  EXPECT_EQ(inst2.connections.size(), 2u);
}

TEST(Builder, RemoveOperations) {
  using namespace verilog;
  
  Module m = make_module("top");
  add_input(m, "a");
  add_output(m, "b");
  add_wire(m, "w");
  
  EXPECT_TRUE(remove_input(m, "a"));
  EXPECT_EQ(m.input_declarations.size(), 0u);
  EXPECT_FALSE(remove_input(m, "a")); // Already removed
  
  EXPECT_TRUE(remove_output(m, "b"));
  EXPECT_EQ(m.output_declarations.size(), 0u);
  
  EXPECT_TRUE(remove_wire(m, "w"));
  EXPECT_EQ(m.net_declarations.size(), 0u);
}

TEST(Builder, QueryFunctions) {
  using namespace verilog;
  
  Module m = make_module("top");
  add_input(m, "a");
  add_output(m, "b");
  add_wire(m, "w");
  
  EXPECT_TRUE(has_port(m, "a"));
  EXPECT_TRUE(has_port(m, "b"));
  EXPECT_FALSE(has_port(m, "w"));
  
  EXPECT_TRUE(has_wire(m, "w"));
  EXPECT_FALSE(has_wire(m, "a"));
  
  EXPECT_TRUE(has_signal(m, "a"));
  EXPECT_TRUE(has_signal(m, "b"));
  EXPECT_TRUE(has_signal(m, "w"));
  EXPECT_FALSE(has_signal(m, "x"));
}

TEST(Builder, NetlistOperations) {
  using namespace verilog;
  
  Netlist nl;
  Module m1 = make_module("mod1");
  Module m2 = make_module("mod2");
  
  add_module(nl, m1);
  EXPECT_EQ(nl.modules.size(), 1u);
  
  add_module(nl, m2);
  EXPECT_EQ(nl.modules.size(), 2u);
  
  // Try to add duplicate
  EXPECT_THROW(add_module(nl, m1), std::invalid_argument);
  
  // Find module
  auto* found = find_module(nl, "mod1");
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->module_name, "mod1");
  
  auto* not_found = find_module(nl, "nonexistent");
  EXPECT_EQ(not_found, nullptr);
}

// ============================================================================
// Writer API Tests
// ============================================================================

TEST(Writer, SimpleModule) {
  using namespace verilog;
  
  Module m = make_module("simple");
  add_input(m, "a");
  add_output(m, "b");
  
  std::string verilog = write_module(m);
  
  EXPECT_NE(verilog.find("module simple"), std::string::npos);
  EXPECT_NE(verilog.find("input a;"), std::string::npos);
  EXPECT_NE(verilog.find("output b;"), std::string::npos);
  EXPECT_NE(verilog.find("endmodule"), std::string::npos);
}

TEST(Writer, ModuleWithRanges) {
  using namespace verilog;
  
  Module m = make_module("ranged");
  add_input(m, "data_in", make_range(7, 0));
  add_output(m, "data_out", make_range(15, 0));
  
  std::string verilog = write_module(m);
  
  EXPECT_NE(verilog.find("input [7:0] data_in"), std::string::npos);
  EXPECT_NE(verilog.find("output [15:0] data_out"), std::string::npos);
}

TEST(Writer, ModuleWithAssignment) {
  using namespace verilog;
  
  Module m = make_module("assign_test");
  add_input(m, "a");
  add_output(m, "b");
  add_assignment(m, make_identifier("b"), make_identifier("a"));
  
  std::string verilog = write_module(m);
  
  EXPECT_NE(verilog.find("assign b = a;"), std::string::npos);
}

TEST(Writer, ModuleWithInstance) {
  using namespace verilog;
  
  Module m = make_module("top");
  add_input(m, "x");
  add_output(m, "y");
  
  auto inst = make_instance_named("leaf", "u0", {
    {"a", make_identifier("x")},
    {"b", make_identifier("y")}
  });
  add_instance(m, inst);
  
  std::string verilog = write_module(m);
  
  EXPECT_NE(verilog.find("leaf u0 ("), std::string::npos);
  EXPECT_NE(verilog.find(".a(x)"), std::string::npos);
  EXPECT_NE(verilog.find(".b(y)"), std::string::npos);
}

TEST(Writer, ComplexExpressions) {
  using namespace verilog;
  
  Module m = make_module("complex");
  add_input(m, "bus", make_range(7, 0));
  add_output(m, "bit_out");
  add_output(m, "slice_out", make_range(3, 0));
  
  // Bit select
  add_assignment(m, make_identifier("bit_out"), make_indexed("bus", 5));
  
  // Part select
  add_assignment(m, make_identifier("slice_out"), make_sliced("bus", 3, 0));
  
  std::string verilog = write_module(m);
  
  EXPECT_NE(verilog.find("assign bit_out = bus[5];"), std::string::npos);
  EXPECT_NE(verilog.find("assign slice_out = bus[3:0];"), std::string::npos);
}

TEST(Writer, NumberFormatting) {
  using namespace verilog;
  
  // Simple number
  auto n1 = make_number(42);
  EXPECT_EQ(write_number(n1), "42");
  
  // Hex with size
  auto n2 = make_hex_number(8, "FF");
  EXPECT_EQ(write_number(n2), "8'hFF");
  
  // Binary with size
  auto n3 = make_bin_number(4, "1010");
  EXPECT_EQ(write_number(n3), "4'b1010");
}

TEST(Writer, RangeFormatting) {
  using namespace verilog;
  
  auto r = make_range(7, 0);
  EXPECT_EQ(write_range(r), "[7:0]");
}

TEST(Writer, ExpressionFormatting) {
  using namespace verilog;
  
  EXPECT_EQ(write_expr(make_identifier("sig")), "sig");
  EXPECT_EQ(write_expr(make_indexed("bus", 3)), "bus[3]");
  EXPECT_EQ(write_expr(make_sliced("data", 7, 0)), "data[7:0]");
  
  auto concat = make_concatenation({make_identifier("a"), make_identifier("b")});
  EXPECT_EQ(write_expr(concat), "{a, b}");
}

TEST(Writer, Netlist) {
  using namespace verilog;
  
  Netlist nl;
  
  Module m1 = make_module("leaf");
  add_input(m1, "x");
  add_output(m1, "y");
  
  Module m2 = make_module("top");
  add_input(m2, "a");
  add_output(m2, "b");
  auto inst = make_instance("leaf", "u0", {Connection{"x", make_identifier("a")}, Connection{"y", make_identifier("b")}});
  add_instance(m2, inst);
  
  add_module(nl, m1);
  add_module(nl, m2);
  
  std::string verilog = write_netlist(nl);
  
  EXPECT_EQ(verilog.rfind("kernlist_format 1;", 0), 0u);
  EXPECT_NE(verilog.find("module leaf"), std::string::npos);
  EXPECT_NE(verilog.find("module top"), std::string::npos);
  EXPECT_NE(verilog.find("leaf u0"), std::string::npos);
}

TEST(Writer, WriterOptions) {
  using namespace verilog;
  
  Module m = make_module("opts_test");
  add_input(m, "a");
  
  WriterOptions opts;
  opts.indent_size = 4;
  
  std::string verilog = write_module(m, opts);
  
  // Check that indentation is applied (4 spaces)
  EXPECT_NE(verilog.find("    input a;"), std::string::npos);
}

// ============================================================================
// Validation Tests
// ============================================================================

TEST(Validation, ValidModule) {
  using namespace verilog;
  
  Module m = make_module("valid");
  add_input(m, "a");
  add_output(m, "b");
  
  auto errors = validate_module(m);
  EXPECT_TRUE(errors.empty());
}

TEST(Validation, DuplicateSignalName) {
  using namespace verilog;
  
  Module m = make_module("dup");
  add_input(m, "signal");
  
  // Manually add duplicate (bypassing add_output which would throw)
  OutputDeclaration out;
  out.net_name = "signal";
  m.output_declarations.push_back(out);
  
  auto errors = validate_module(m);
  EXPECT_FALSE(errors.empty());
  
  bool found_dup_error = false;
  for (const auto& err : errors) {
    if (err.message.find("Duplicate") != std::string::npos) {
      found_dup_error = true;
      break;
    }
  }
  EXPECT_TRUE(found_dup_error);
}

TEST(Validation, UndeclaredSignalInAssignment) {
  using namespace verilog;
  
  Module m = make_module("undeclared");
  add_output(m, "b");
  
  // Reference undeclared signal 'a'
  add_assignment(m, make_identifier("b"), make_identifier("a"));
  
  auto errors = validate_module(m);
  EXPECT_FALSE(errors.empty());
  
  bool found_undeclared = false;
  for (const auto& err : errors) {
    if (err.message.find("undeclared") != std::string::npos) {
      found_undeclared = true;
      break;
    }
  }
  EXPECT_TRUE(found_undeclared);
}

TEST(Validation, NetlistDuplicateModuleName) {
  using namespace verilog;
  
  Netlist nl;
  Module m1 = make_module("dup_name");
  Module m2 = make_module("dup_name");
  
  nl.modules.push_back(m1);
  nl.modules.push_back(m2);
  
  auto errors = validate_netlist(nl);
  EXPECT_FALSE(errors.empty());
  
  bool found_dup = false;
  for (const auto& err : errors) {
    if (err.message.find("Duplicate module") != std::string::npos) {
      found_dup = true;
      break;
    }
  }
  EXPECT_TRUE(found_dup);
}

TEST(Validation, ValidateBeforeWrite) {
  using namespace verilog;
  
  Module m = make_module("invalid");
  add_output(m, "b");
  add_assignment(m, make_identifier("b"), make_identifier("undeclared"));
  
  WriterOptions opts;
  opts.validate_before_write = true;
  
  EXPECT_THROW(write_module(m, opts), parse_error);
}

// ============================================================================
// Round-trip Tests (Parse -> Modify -> Write -> Parse)
// ============================================================================

TEST(RoundTrip, ParseModifyWrite) {
  using namespace verilog;
  
  std::string original = R"(
kernlist_format 1;
module original(a, b);
  input a;
  output b;
  assign b = a;
endmodule
)";
  
  // Parse
  auto nl = parse_string(original);
  ASSERT_EQ(nl.modules.size(), 1u);
  
  // Modify
  Module& m = nl.modules[0];
  add_wire(m, "internal");
  
  // Write
  std::string modified = write_netlist(nl);
  
  // Parse again
  auto nl2 = parse_string(modified);
  ASSERT_EQ(nl2.modules.size(), 1u);
  EXPECT_EQ(nl2.modules[0].net_declarations.size(), 1u);
  EXPECT_EQ(nl2.modules[0].net_declarations[0].net_name, "internal");
}

TEST(RoundTrip, CreateWriteParse) {
  using namespace verilog;
  
  // Create from scratch
  Module m = make_module("created");
  add_input(m, "clk");
  add_input(m, "data", make_range(7, 0));
  add_output(m, "result", make_range(7, 0));
  add_wire(m, "temp", make_range(7, 0));
  add_assignment(m, make_identifier("result"), make_identifier("data"));
  
  // Write
  std::string verilog = write_module(m);
  
  // Parse (write_module writes one module, without the format line)
  auto nl = parse_string("kernlist_format 1;\n" + verilog);
  ASSERT_EQ(nl.modules.size(), 1u);
  
  const auto& parsed = nl.modules[0];
  EXPECT_EQ(parsed.module_name, "created");
  EXPECT_EQ(parsed.input_declarations.size(), 2u);
  EXPECT_EQ(parsed.output_declarations.size(), 1u);
  EXPECT_EQ(parsed.net_declarations.size(), 1u);
  EXPECT_EQ(parsed.assignments.size(), 1u);
}

TEST(Writer, KernlistStatements) {
  Module m = make_module("top");
  add_input(m, "a");
  add_wire(m, "w", make_range(1, 0));
  add_wire(m, "123 odd");
  add_assignment(m, make_identifier("w"),
                 make_binary("&", make_binary("|", make_identifier("a"), make_identifier("a")),
                             make_unary("~", make_identifier("a"))));
  ModuleInstance inst = make_instance("fifo", "q", {Connection{"enqValid", make_identifier("a")}, Connection{"spare", std::nullopt}});
  inst.parameters = {Parameter{"DEPTH", 2}, Parameter{"W", 8}};
  inst.variant = "circular_buffer";
  add_instance(m, inst);
  add_invariant(m, "held", make_binary("->", make_identifier("a"),
                                       make_binary("==", make_model_ref("q", "count"), make_number(1))));
  std::string text = write_module(m);
  EXPECT_NE(text.find("assign w = (a | a) & ~a;"), std::string::npos) << text;
  EXPECT_NE(text.find("fifo #(.DEPTH(2), .W(8)) variant circular_buffer q ("), std::string::npos) << text;
  EXPECT_NE(text.find(".spare()"), std::string::npos) << text;
  EXPECT_NE(text.find("invariant held: a -> model(q.count) == 1;"), std::string::npos) << text;
  EXPECT_NE(text.find("wire \\123 odd ;"), std::string::npos) << text;
}

TEST(RoundTrip, KernlistStatements) {
  std::string original = R"(kernlist_format 1;
module top(a, y);
  input a;
  output [1:0] y;
  wire [3:0] g;
  wire \%c-2_i2 ;
  assign \%c-2_i2 = ~a;
  assign y = {g[3:2] != 2'd0, g[3] | g[1]}, g = {4{a}};
  arbiter #(.N(4)) variant round_robin arb (.req({a, a, a, \%c-2_i2 }), .gnt(g), .any());
  invariant one_hot: g[0] -> y == 2'd0;
  invariant counted: model(arb.waiting) <= 4 - 1;
endmodule
)";
  auto nl = parse_string(original);
  std::string written = write_netlist(nl);
  auto again = parse_string(written);
  EXPECT_EQ(write_netlist(again), written);
  const auto& m = again.modules[0];
  ASSERT_EQ(m.module_instances.size(), 1u);
  EXPECT_EQ(m.module_instances[0].variant.value(), "round_robin");
  EXPECT_EQ(m.invariants.size(), 2u);
  EXPECT_EQ(m.net_declarations[1].net_name, "%c-2_i2");
}
