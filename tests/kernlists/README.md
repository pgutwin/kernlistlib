# Example kernlists: the test set

Kernlist files written by hand against `doc/KERNLIST_format.md` and
`doc/kernlist.bnf`, as the reader's first inputs. Each file in `invalid/`
has one fault and starts with a line saying how it must fail:

    // EXPECT reading: ...    the grammar rejects it
    // EXPECT checking: ...   it reads, and the one-file checks reject it

Forms allowed only in invariants (arithmetic, ordered comparisons,
unsized numbers, `model(...)`, `->`) are read everywhere and rejected by
checking outside invariants, so that the message can name the rule
(KL13).

Files in `valid/` must read and check without a problem.

None of these files are copies. The two designs are **derived** from
kern-design-examples (commit `bc7f1b3`) and rewritten here by hand; they may
be edited here freely.

## valid/

| File | What it covers |
|---|---|
| `minimal.kl` | the format line, one module, one assignment |
| `comments_before_format_line.kl` | comments and white space before the format line and between tokens |
| `empty_module.kl` | a module with no ports and no statements |
| `instances.kl` | instances with and without parameters and `variant`, an unconnected port, no connections |
| `operators.kl` | every expression form in KL08, precedence, concatenation on the left, `c & &a` |
| `invariants.kl` | plain logic, `->`, `model(...)`, `+ -`, comparisons, a statement over several lines |
| `two_modules.kl` | a module instantiating another module of the same file |
| `escaped_identifiers.kl` | escaped names, as firtool writes them (`\%c-2_i2 `) |
| `shared_request_port.kl` | the request side: 4 FIFOs, an arbiter, a selector, the master-number logic. Derived from `designs/shared_request_port/design.py` |
| `shared_memory_port.kl` | the full port: also credit counters, the tagged memory, response FIFOs, and the credit and tag invariants. Derived from `experiments/kernlist_formats/results/kernlist.kv` |

## invalid/

| File | Fails in | Why |
|---|---|---|
| `no_format_line.kl` | reading | no format line |
| `format_line_after_module.kl` | reading | format line after the module |
| `format_line_twice.kl` | reading | two format lines |
| `format_version_2.kl` | checking | version 2 is not known |
| `no_module.kl` | reading | no module |
| `plus_in_assign.kl` | checking | `+` in `assign` |
| `minus_in_connection.kl` | checking | `-` in a connection |
| `less_than_in_assign.kl` | checking | `<` in `assign` |
| `shift_in_assign.kl` | reading | `<<` in `assign` |
| `unsized_constant_in_assign.kl` | checking | `1` instead of `1'b1` |
| `logical_and_in_assign.kl` | reading | `&&` |
| `logical_or_in_connection.kl` | reading | `\|\|` |
| `implication_in_assign.kl` | checking | `->` outside an invariant |
| `model_in_assign.kl` | checking | `model(...)` in `assign` |
| `model_in_connection.kl` | checking | `model(...)` in a connection |
| `variant_without_name.kl` | reading | `variant` with no name |
| `variant_before_parameters.kl` | reading | `variant` before `#(...)` |
| `parameter_not_a_number.kl` | reading | `.W(WIDTH)` |
| `parameter_by_position.kl` | reading | `#(8)` |
| `connection_by_position.kl` | reading | `(a)` instead of `(.in(a))` |
| `inout_port.kl` | reading | `inout` |
| `invariant_without_name.kl` | reading | no name and colon |
| `implication_chained.kl` | reading | `a -> b -> c` |
| `implication_not_outermost.kl` | checking | `(a -> b) & c` |
| `undeclared_name.kl` | checking | a name never declared |
| `used_before_declared.kl` | checking | a name used before its declaration |
| `declared_twice.kl` | checking | a wire declared twice |
| `port_not_declared.kl` | checking | a port with no `input` or `output` |
| `instance_name_twice.kl` | checking | two instances with one name |
| `module_name_twice.kl` | checking | two modules with one name |
| `invariant_name_twice.kl` | checking | two invariants with one name |
| `variant_on_module_instance.kl` | checking | `variant` on an instance of a module in the same file |
| `width_assign.kl` | checking | 4 bits assigned to 2 (KL16) |
| `width_concatenation_assign.kl` | checking | 3 bits assigned to `{y, z}` (KL16) |
| `width_operands.kl` | checking | `a[3] & a` (KL16) |
| `width_equality.kl` | checking | 4 bits `==` 3 bits (KL16) |
| `width_choice_condition.kl` | checking | a 2-bit condition of `?:` (KL16) |
| `width_choice_sides.kl` | checking | sides of `?:` of 1 and 2 bits (KL16) |
| `width_select_outside_range.kl` | checking | `a[4]` of `[3:0]` (KL16) |
| `width_select_direction.kl` | checking | `a[0:3]` of `[3:0]` (KL16) |
| `width_select_on_scalar.kl` | checking | `a[0]` of a name without a range (KL16) |
| `width_constant_too_large.kl` | checking | `2'd5` (KL16) |
| `width_in_connection.kl` | checking | `a & b` of 1 and 2 bits in a connection (KL16) |
| `width_implication.kl` | checking | a 4-bit side of `->` (KL16) |
| `width_invariant_not_one_bit.kl` | checking | an invariant 4 bits wide (KL16) |
| `model_in_bitwise_logic.kl` | checking | a model value in `&` (KL16) |

Not covered, because they need the motif library (linking, later):
unknown families, wrong parameter or port names, outputs not connected to
declared wires (KL04), handshake pairs, model names in invariants, and
the widths of motif ports.

The C++ tests (`tests/test_kernlists.cpp`) read and check every file
here and compare with its expectation.

## reference/

`kernlist.lark` is `doc/kernlist.bnf` written for the Python parser Lark,
and `check.py` reads and checks every file here with it and compares the
result with the file's `EXPECT` line. It was used to check the test set, the grammar
and the width rules before the C++ code was written; the BNF and
`doc/KERNLIST_format.md` are the specification. Run it with `python3 tests/kernlists/reference/check.py`
(needs `pip install lark`).
