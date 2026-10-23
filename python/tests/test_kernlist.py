"""Tests of the Python bindings (KL19). Run by ctest, with the built module on PYTHONPATH and
KERNLISTS_DIR set to tests/kernlists."""
import os
import unittest

import kernlist

KERNLISTS = os.environ.get("KERNLISTS_DIR", os.path.join(os.path.dirname(__file__), "..", "..", "tests", "kernlists"))
FULL_PORT = os.path.join(KERNLISTS, "valid", "shared_memory_port.kl")


class Reading(unittest.TestCase):
    def test_full_port(self):
        nl = kernlist.read(FULL_PORT)
        self.assertEqual(nl.format_version, 1)
        m = nl.modules[0]
        self.assertEqual(m.module_name, "SharedMemoryPort")
        self.assertEqual(len(m.module_instances), 15)
        fifo = m.module_instances[0]
        self.assertEqual(fifo.module_name, "fifo")
        self.assertEqual([(p.name, p.value) for p in fifo.parameters], [("DEPTH", 2), ("W", 8)])
        self.assertEqual(fifo.variant, "circular_buffer")
        self.assertEqual(fifo.connections[0].port, "enqValid")
        self.assertIsInstance(fifo.connections[0].expr, kernlist.Identifier)
        self.assertEqual(len(m.invariants), 8)
        self.assertEqual(m.invariants[0].name, "credits_0_accounted")

    def test_expressions_are_objects(self):
        nl = kernlist.read_string("kernlist_format 1; module m(a, b, y); input a, b; output y;"
                                  " assign y = a & ~b; endmodule")
        left, right = nl.modules[0].assignments[0].assignments[0]
        self.assertEqual(left.name, "y")
        self.assertIsInstance(right, kernlist.BinaryOp)
        self.assertEqual(right.op, "&")
        self.assertEqual(right.lhs.name, "a")
        self.assertIsInstance(right.rhs, kernlist.UnaryOp)
        self.assertEqual(kernlist.write_expr(right), "a & ~b")

    def test_syntax_error(self):
        with self.assertRaises(kernlist.ParseError) as caught:
            kernlist.read_string("kernlist_format 1;\nmodule m (a, y);\n input a;\n output y;\n assign y = a << 1;\nendmodule\n")
        self.assertEqual(caught.exception.line, 5)
        self.assertEqual(caught.exception.column, 15)
        self.assertIn("expected ';'", str(caught.exception))


class CheckingAndGroups(unittest.TestCase):
    def test_valid_file_has_no_problems(self):
        self.assertEqual(kernlist.check(kernlist.read(FULL_PORT)), [])

    def test_problems(self):
        nl = kernlist.read(os.path.join(KERNLISTS, "invalid", "width_assign.kl"))
        problems = kernlist.check(nl)
        self.assertEqual(len(problems), 1)
        self.assertEqual(problems[0].line, 6)
        self.assertEqual(problems[0].message, "assign of 4 bits to 2 bits")

    def test_groups(self):
        m = kernlist.read(FULL_PORT).modules[0]
        groups = kernlist.logic_groups(m)
        self.assertEqual(len(groups), 14)
        self.assertEqual(groups[0].name, "eligible[3]")
        self.assertEqual(groups[0].inputs, ["requests_fifo_3_deqValid", "credits_3_available"])


class Changing(unittest.TestCase):
    def test_choose_a_variant_and_write(self):
        nl = kernlist.read(FULL_PORT)
        inst = nl.modules[0].module_instances[0]
        inst.variant = "shift_register"
        nl.modules[0].module_instances[1].variant = None
        text = kernlist.write(nl)
        self.assertIn("fifo #(.DEPTH(2), .W(8)) variant shift_register requests_fifo (", text)
        self.assertIn("fifo #(.DEPTH(2), .W(8)) requests_fifo_1 (", text)
        again = kernlist.read_string(text)
        self.assertEqual(again.modules[0].module_instances[0].variant, "shift_register")
        self.assertEqual(kernlist.check(again), [])

    def test_change_a_parameter_in_place(self):
        nl = kernlist.read(FULL_PORT)
        nl.modules[0].module_instances[0].parameters[0].value = 4
        self.assertEqual(nl.modules[0].module_instances[0].parameters[0].value, 4)

    def test_build_and_write(self):
        m = kernlist.Module("top")
        a = kernlist.InputDeclaration(); a.net_name = "a"
        m.input_declarations.append(a)
        m.port_list = ["a"]
        inst = kernlist.ModuleInstance("credit_counter", "credits")
        inst.parameters.append(kernlist.Parameter("LIMIT", 2))
        inst.variant = "binary"
        inst.connections.append(kernlist.Connection("take", kernlist.Identifier("a")))
        inst.connections.append(kernlist.Connection("available"))
        m.module_instances.append(inst)
        m.invariants.append(kernlist.Invariant("bounded",
            kernlist.BinaryOp("<=", kernlist.ModelRef("credits", "taken"), kernlist.Number(2))))
        nl = kernlist.Netlist()
        nl.modules.append(m)
        text = kernlist.write(nl)
        self.assertTrue(text.startswith("kernlist_format 1;"))
        self.assertIn("credit_counter #(.LIMIT(2)) variant binary credits (", text)
        self.assertIn(".available()", text)
        self.assertIn("invariant bounded: model(credits.taken) <= 2;", text)
        self.assertEqual(kernlist.check(kernlist.read_string(text)), [])


if __name__ == "__main__":
    unittest.main()
