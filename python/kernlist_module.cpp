// Python bindings (KL19): the kernlist data model as Python objects that mirror the C++ classes,
// for reading, checking, changing and writing kernlists.
//
//   import kernlist
//   nl = kernlist.read("design.kl")            # raises kernlist.ParseError (line, column)
//   problems = kernlist.check(nl)              # list of Problem
//   inst = nl.modules[0].module_instances[0]
//   inst.variant = "circular_buffer"           # changes the netlist in place
//   text = kernlist.write(nl)
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include "veriloglib.hpp"
#include "verilog_builder.hpp"
#include "verilog_writer.hpp"
#include "verilog_checker.hpp"
#include "verilog_groups.hpp"

namespace py = pybind11;
using namespace verilog;

// Lists that Python code changes in place are bound as their own types, so that
// nl.modules[0].module_instances[0].variant = "x" changes the netlist itself.
PYBIND11_MAKE_OPAQUE(std::vector<Module>);
PYBIND11_MAKE_OPAQUE(std::vector<ModuleInstance>);
PYBIND11_MAKE_OPAQUE(std::vector<Parameter>);
PYBIND11_MAKE_OPAQUE(std::vector<Connection>);
PYBIND11_MAKE_OPAQUE(std::vector<NetDeclaration>);
PYBIND11_MAKE_OPAQUE(std::vector<InputDeclaration>);
PYBIND11_MAKE_OPAQUE(std::vector<OutputDeclaration>);
PYBIND11_MAKE_OPAQUE(std::vector<ContinuousAssign>);
PYBIND11_MAKE_OPAQUE(std::vector<Invariant>);
PYBIND11_MAKE_OPAQUE(std::vector<std::string>);

template< typename T >
static std::string repr_expr(const T& x) { return "<" + write_expr(Expr(x)) + ">"; }

PYBIND11_MODULE(kernlist, m) {
  m.doc() = "Reading, checking, changing and writing kernlist files (kernlistlib).";

  // ---- errors
  // kernlist.ParseError, with the attributes line and column. The module keeps the type alive.
  static PyObject* parse_error_type = py::exception<parse_error>(m, "ParseError").ptr();
  py::register_exception_translator([](std::exception_ptr p) {
    try { if (p) std::rethrow_exception(p); }
    catch (const parse_error& e) {
      py::object err = py::reinterpret_borrow<py::object>(parse_error_type)(e.what());
      err.attr("line") = e.line;
      err.attr("column") = e.column;
      PyErr_SetObject(parse_error_type, err.ptr());
    }
  });

  py::class_<Location>(m, "Location")
    .def(py::init<>())
    .def_readwrite("line", &Location::line)
    .def_readwrite("column", &Location::column)
    .def("__repr__", [](const Location& l) { return "<line " + std::to_string(l.line) + ", column " + std::to_string(l.column) + ">"; });

  // ---- expressions
  py::class_<Number>(m, "Number")
    .def(py::init<>())
    .def(py::init([](int64_t value) { return make_number(value); }), py::arg("value"))
    .def(py::init([](int length, char base, std::string digits) { return make_number(length, base, digits); }),
         py::arg("length"), py::arg("base"), py::arg("digits"))
    .def_readwrite("length", &Number::length)
    .def_readwrite("base", &Number::base)
    .def_readwrite("mantissa", &Number::mantissa)
    .def("as_integer", &Number::as_integer)
    .def("__repr__", [](const Number& n) { return "<" + write_number(n) + ">"; });

  py::class_<Range>(m, "Range")
    .def(py::init<>())
    .def(py::init([](int64_t msb, int64_t lsb) { return make_range(msb, lsb); }), py::arg("msb"), py::arg("lsb"))
    .def_readwrite("start", &Range::start)
    .def_readwrite("end", &Range::end)
    .def("to_indices", &Range::to_indices)
    .def("__repr__", [](const Range& r) { return "<" + write_range(r) + ">"; });

  py::class_<Identifier>(m, "Identifier")
    .def(py::init<>())
    .def(py::init([](std::string name) { return Identifier{ name }; }), py::arg("name"))
    .def_readwrite("name", &Identifier::name)
    .def("__repr__", &repr_expr<Identifier>);
  py::class_<IdentifierIndexed>(m, "IdentifierIndexed")
    .def(py::init<>())
    .def(py::init([](std::string name, int64_t index) { return IdentifierIndexed{ name, make_number(index) }; }), py::arg("name"), py::arg("index"))
    .def_readwrite("name", &IdentifierIndexed::name)
    .def_readwrite("index", &IdentifierIndexed::index)
    .def("__repr__", &repr_expr<IdentifierIndexed>);
  py::class_<IdentifierSliced>(m, "IdentifierSliced")
    .def(py::init<>())
    .def(py::init([](std::string name, int64_t msb, int64_t lsb) { return IdentifierSliced{ name, make_range(msb, lsb) }; }),
         py::arg("name"), py::arg("msb"), py::arg("lsb"))
    .def_readwrite("name", &IdentifierSliced::name)
    .def_readwrite("range", &IdentifierSliced::range)
    .def("__repr__", &repr_expr<IdentifierSliced>);
  py::class_<ModelRef>(m, "ModelRef")
    .def(py::init<>())
    .def(py::init([](std::string instance, std::string name) { ModelRef r; r.instance = instance; r.name = name; return r; }),
         py::arg("instance"), py::arg("name"))
    .def_readwrite("instance", &ModelRef::instance)
    .def_readwrite("name", &ModelRef::name)
    .def_readwrite("index", &ModelRef::index)
    .def_readwrite("range", &ModelRef::range)
    .def("__repr__", &repr_expr<ModelRef>);
  py::class_<Concatenation, std::shared_ptr<Concatenation>>(m, "Concatenation")
    .def(py::init<>())
    .def(py::init([](std::vector<Expr> elements) { auto c = std::make_shared<Concatenation>(); c->elements = elements; return c; }),
         py::arg("elements"))
    .def_readwrite("elements", &Concatenation::elements)
    .def("__repr__", [](const std::shared_ptr<Concatenation>& x) { return "<" + write_expr(x) + ">"; });
  py::class_<Replication, std::shared_ptr<Replication>>(m, "Replication")
    .def(py::init<>())
    .def(py::init([](int64_t count, std::vector<Expr> elements) {
           return std::get<std::shared_ptr<Replication>>(make_replication(count, elements)); }),
         py::arg("count"), py::arg("elements"))
    .def_readwrite("count", &Replication::count)
    .def_readwrite("elements", &Replication::elements)
    .def("__repr__", [](const std::shared_ptr<Replication>& x) { return "<" + write_expr(x) + ">"; });
  py::class_<UnaryOp, std::shared_ptr<UnaryOp>>(m, "UnaryOp")
    .def(py::init<>())
    .def(py::init([](std::string op, Expr operand) { return std::get<std::shared_ptr<UnaryOp>>(make_unary(op, operand)); }),
         py::arg("op"), py::arg("operand"))
    .def_readwrite("op", &UnaryOp::op)
    .def_readwrite("operand", &UnaryOp::operand)
    .def("__repr__", [](const std::shared_ptr<UnaryOp>& x) { return "<" + write_expr(x) + ">"; });
  py::class_<BinaryOp, std::shared_ptr<BinaryOp>>(m, "BinaryOp")
    .def(py::init<>())
    .def(py::init([](std::string op, Expr lhs, Expr rhs) { return std::get<std::shared_ptr<BinaryOp>>(make_binary(op, lhs, rhs)); }),
         py::arg("op"), py::arg("lhs"), py::arg("rhs"))
    .def_readwrite("op", &BinaryOp::op)
    .def_readwrite("lhs", &BinaryOp::lhs)
    .def_readwrite("rhs", &BinaryOp::rhs)
    .def("__repr__", [](const std::shared_ptr<BinaryOp>& x) { return "<" + write_expr(x) + ">"; });
  py::class_<Choice, std::shared_ptr<Choice>>(m, "Choice")
    .def(py::init<>())
    .def(py::init([](Expr c, Expr t, Expr f) { return std::get<std::shared_ptr<Choice>>(make_choice(c, t, f)); }),
         py::arg("condition"), py::arg("if_true"), py::arg("if_false"))
    .def_readwrite("condition", &Choice::condition)
    .def_readwrite("if_true", &Choice::if_true)
    .def_readwrite("if_false", &Choice::if_false)
    .def("__repr__", [](const std::shared_ptr<Choice>& x) { return "<" + write_expr(x) + ">"; });

  // ---- statements
  py::class_<NetDeclaration>(m, "NetDeclaration")
    .def(py::init<>())
    .def_readwrite("net_name", &NetDeclaration::net_name)
    .def_readwrite("range", &NetDeclaration::range)
    .def_readwrite("location", &NetDeclaration::location);
  py::class_<InputDeclaration, NetDeclaration>(m, "InputDeclaration").def(py::init<>());
  py::class_<OutputDeclaration, NetDeclaration>(m, "OutputDeclaration").def(py::init<>());

  py::class_<ContinuousAssign>(m, "ContinuousAssign")
    .def(py::init<>())
    .def_readwrite("assignments", &ContinuousAssign::assignments,
                   "list of (left, right) pairs; assign a new list to change them")
    .def_readwrite("location", &ContinuousAssign::location);

  py::class_<Parameter>(m, "Parameter")
    .def(py::init<>())
    .def(py::init([](std::string name, int64_t value) { return Parameter{ name, value }; }), py::arg("name"), py::arg("value"))
    .def_readwrite("name", &Parameter::name)
    .def_readwrite("value", &Parameter::value)
    .def("__repr__", [](const Parameter& p) { return "<." + p.name + "(" + std::to_string(p.value) + ")>"; });

  py::class_<Connection>(m, "Connection")
    .def(py::init<>())
    .def(py::init([](std::string port, std::optional<Expr> expr) { return Connection{ port, expr }; }),
         py::arg("port"), py::arg("expr") = py::none())
    .def_readwrite("port", &Connection::port)
    .def_readwrite("expr", &Connection::expr)
    .def("__repr__", [](const Connection& c) { return "<." + c.port + "(" + (c.expr ? write_expr(*c.expr) : "") + ")>"; });

  py::class_<ModuleInstance>(m, "ModuleInstance")
    .def(py::init<>())
    .def(py::init([](std::string family, std::string name) { return make_instance(family, name); }),
         py::arg("module_name"), py::arg("instance_name"))
    .def_readwrite("module_name", &ModuleInstance::module_name)
    .def_readwrite("instance_name", &ModuleInstance::instance_name)
    .def_readwrite("parameters", &ModuleInstance::parameters)
    .def_readwrite("variant", &ModuleInstance::variant)
    .def_readwrite("connections", &ModuleInstance::connections)
    .def_readwrite("location", &ModuleInstance::location)
    .def("__repr__", [](const ModuleInstance& i) {
      return "<" + i.module_name + (i.variant ? " variant " + *i.variant : "") + " " + i.instance_name + ">"; });

  py::class_<Invariant>(m, "Invariant")
    .def(py::init<>())
    .def(py::init([](std::string name, Expr expr) { return Invariant{ name, expr, Location{} }; }), py::arg("name"), py::arg("expr"))
    .def_readwrite("name", &Invariant::name)
    .def_readwrite("expr", &Invariant::expr)
    .def_readwrite("location", &Invariant::location)
    .def("__repr__", [](const Invariant& i) { return "<invariant " + i.name + ": " + write_expr(i.expr) + ">"; });

  py::class_<Module>(m, "Module")
    .def(py::init<>())
    .def(py::init([](std::string name) { return make_module(name); }), py::arg("module_name"))
    .def_readwrite("module_name", &Module::module_name)
    .def_readwrite("port_list", &Module::port_list)
    .def_readwrite("input_declarations", &Module::input_declarations)
    .def_readwrite("output_declarations", &Module::output_declarations)
    .def_readwrite("net_declarations", &Module::net_declarations)
    .def_readwrite("assignments", &Module::assignments)
    .def_readwrite("module_instances", &Module::module_instances)
    .def_readwrite("invariants", &Module::invariants)
    .def_readwrite("location", &Module::location)
    .def("summary", &Module::summary)
    .def("__repr__", [](const Module& x) { return "<module " + x.module_name + ">"; });

  py::class_<Netlist>(m, "Netlist")
    .def(py::init<>())
    .def_readwrite("format_version", &Netlist::format_version)
    .def_readwrite("modules", &Netlist::modules)
    .def("__repr__", [](const Netlist& n) { return "<kernlist, " + std::to_string(n.modules.size()) + " module(s)>"; });

  py::bind_vector<std::vector<Module>>(m, "ModuleList");
  py::bind_vector<std::vector<ModuleInstance>>(m, "InstanceList");
  py::bind_vector<std::vector<Parameter>>(m, "ParameterList");
  py::bind_vector<std::vector<Connection>>(m, "ConnectionList");
  py::bind_vector<std::vector<NetDeclaration>>(m, "NetDeclarationList");
  py::bind_vector<std::vector<InputDeclaration>>(m, "InputDeclarationList");
  py::bind_vector<std::vector<OutputDeclaration>>(m, "OutputDeclarationList");
  py::bind_vector<std::vector<ContinuousAssign>>(m, "AssignList");
  py::bind_vector<std::vector<Invariant>>(m, "InvariantList");
  py::bind_vector<std::vector<std::string>>(m, "StringList");
  // so that a plain Python list can be assigned to these attributes: module.assignments = [...]
  py::implicitly_convertible<py::list, std::vector<Module>>();
  py::implicitly_convertible<py::list, std::vector<ModuleInstance>>();
  py::implicitly_convertible<py::list, std::vector<Parameter>>();
  py::implicitly_convertible<py::list, std::vector<Connection>>();
  py::implicitly_convertible<py::list, std::vector<NetDeclaration>>();
  py::implicitly_convertible<py::list, std::vector<InputDeclaration>>();
  py::implicitly_convertible<py::list, std::vector<OutputDeclaration>>();
  py::implicitly_convertible<py::list, std::vector<ContinuousAssign>>();
  py::implicitly_convertible<py::list, std::vector<Invariant>>();
  py::implicitly_convertible<py::list, std::vector<std::string>>();

  // ---- checking and groups
  py::class_<Problem>(m, "Problem")
    .def_readonly("line", &Problem::line)
    .def_readonly("column", &Problem::column)
    .def_readonly("module", &Problem::module)
    .def_readonly("message", &Problem::message)
    .def("__repr__", [](const Problem& p) { return "<line " + std::to_string(p.line) + ": " + p.message + ">"; });

  py::class_<LogicGroup>(m, "LogicGroup")
    .def_readonly("name", &LogicGroup::name)
    .def_readonly("outputs", &LogicGroup::outputs)
    .def_readonly("inputs", &LogicGroup::inputs)
    .def_readonly("operations", &LogicGroup::operations)
    .def_readonly("statements", &LogicGroup::statements)
    .def("__repr__", [](const LogicGroup& g) { return "<group " + g.name + ", " + std::to_string(g.operations) + " operation(s)>"; });

  // ---- functions
  m.def("read", &parse_file, py::arg("path"), "Read a .kl file; raises ParseError at the first syntax error");
  m.def("read_string", &parse_string, py::arg("text"), py::arg("source") = "string", "Read kernlist text");
  m.def("write", [](const Netlist& nl) { return write_netlist(nl); }, py::arg("netlist"), "The netlist as kernlist text");
  m.def("write_expr", &write_expr, py::arg("expr"), "An expression as kernlist text");
  m.def("check", &check, py::arg("netlist"), "Every problem found by checking one file, in line order");
  m.def("logic_groups", &logic_groups, py::arg("module"), "The logic groups of a module that passed checking");
}
