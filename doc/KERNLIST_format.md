# The kernlist format

Status: **draft**, rewritten in place as decisions are made. Reasons
are in `KERNLIST_decisions.md`. Only what is listed under "Decided" is
decided.

## 1. What a kernlist is

KERN's description of a design: motif instances, the logic between
them, their connections, and the facts KERN needs about them (variants,
handshake pairs, invariants). The input to the floorplanner. The format
extends the structural Verilog subset that veriloglib reads
(`verilog_subset.bnf`).

## 2. Decided

### 2.1 File structure (KL03)

A kernlist file is:

1. the statement `kernlist_format 1;`, required, before anything else
   other than comments and white space;
2. one or more Verilog modules, `module ... endmodule`, with the
   statements of the format inside.

```
kernlist_format 1;

module SharedMemoryPort (io_request_0_valid, ...);
  ...
endmodule
```

A reader refuses a file without the statement, and a file whose
version it does not know (today, anything but 1).

The motif library a file's families refer to is not named in the file:
it is metadata the designer and the flow manage, as for a Verilog
file's libraries in a usual VLSI flow.

Kernlist files end in `.kl` (KL05).

### 2.2 Naming a motif's outputs (KL04)

Every output port of a motif instance is connected, in the instance's
port list, to a declared wire; expressions elsewhere use the wire. An
instance statement therefore lists all its ports, inputs and outputs.

```
wire [3:0] grant;
wire       any_granted;
... arbiter (.req({eligible_3, eligible_2, eligible_1, eligible_0}), .gnt(grant), .any(any_granted));
assign requests_0_take = grant[0];
```

### 2.3 Motif instances (KL06)

A motif instance is a Verilog instantiation of the motif family by
name, with the family's parameters, optionally followed by the keyword
`variant` and the variant's name, then the instance name and named port
connections:

```
fifo #(.DEPTH(2), .W(8)) variant circular_buffer requests_0 (.enqData(io_request_0_bits), ...);
fifo #(.DEPTH(2), .W(8)) requests_1 (...);        // variant not yet chosen
```

- The parameters are the family's (in the motif library), set by the
  designer; they are part of the contract.
- The variant is KERN's choice among the family's variants. Without
  `variant`, it is not yet chosen.
- A name is resolved as a kernlist module if a module of that name is
  defined in the files read, and otherwise as a motif family from the
  motif library the flow supplies, as a standard cell is resolved in a
  usual VLSI flow. An instance of a kernlist module has no `variant`.

### 2.4 Logic between motifs (KL07)

Logic between motifs is written as Verilog continuous assignments to
declared wires or to the module's outputs:

```
wire [1:0] master_number;
assign master_number = {|grant[3:2], grant[3] | grant[1]};
```

Expressions may use only (KL08):

| Kind | Forms |
|---|---|
| bitwise | `a & b`, `a \| b`, `a ^ b`, `~a` |
| reduction | `&a`, `\|a`, `^a` |
| equality | `a == b`, `a != b` |
| choice | `c ? a : b` |
| joining | `{a, b, ...}`, `{n{a}}` |
| selection | `a[i]`, `a[hi:lo]` (constant indices) |
| constants | sized: `2'd3`, `4'b1010`, `8'hff` |
| grouping | `( ... )` |

Not allowed: arithmetic (`+ - * / %`), ordered comparisons
(`< > <= >=`), shifts, unsized numbers, and anything else. Anything the
motif library has a family for (an adder, for example) is written as a
motif instance, never as an operator.

How these limits are enforced (KL13): the reader accepts one expression
language everywhere, the one of invariants (2.6). Checking then rejects
`+ -`, `< <= > >=`, unsized numbers, `model(...)` and `->` in `assign`
and connections, with a message that names the rule:

```
line 6: + and - are arithmetic, allowed only in invariant statements,
in assign; use a motif
```

Forms in no part of the format (`* / %`, shifts, `&&`, `||`) are
rejected by the reader.

The kernlist holds the logic exactly and nothing derived from it. Tools
that need to place or cost the logic work out its **groups** by one
rule, the same in every tool (from kern-design-examples DE11):

1. The operations are the operators: `~ & | ^` (also as reductions),
   `== !=` and `?:`. Names, selects, concatenations, replications and
   constants are **wiring**, wherever they appear (KL18).
2. Two operations are in the same group if one is an operand of the
   other, directly or through wiring: through selects and
   concatenations within a statement, and through wires between
   statements (`assign w = a & b; assign y = w[0] | c;` is one group).
   Wiring is followed bit by bit, so operations that only share an
   input, or sit side by side in one concatenation, are separate
   groups. Constants join no group.
3. So the groups depend only on the circuit, not on how it is written:
   `assign y = {a & b, c & d};` and two assignments to `y[1]` and
   `y[0]` give the same two groups.
4. A group's inputs are the signals entering it (motif outputs, module
   inputs); its outputs are the signals it drives (wires or module
   outputs, or motif inputs driven directly).
5. A group is named after the signal it drives, the first in file order
   when it drives several, with the bits as a select when it drives
   part of a signal (`eligible[3]`); a group that drives a motif input
   directly is named `instance.port`, with the bits counted from 0 when
   it drives part of the port (KL17).

Example, from the full port: `assign eligible = {requests_3_deqValid &
credits_3_available, ...};` gives four groups, `eligible[3]` to
`eligible[0]`, of one operation each, and `master_number = {grant[3:2]
!= 2'd0, grant[3] | grant[1]}` gives two, `master_number[1]` and
`master_number[0]`.

A group's figures (area in two-input gates, logic levels) are not part
of the format; how they are computed is the flow's concern.

### 2.5 Handshake pairs (KL09)

A kernlist does not state handshake pairs; tools find them by one rule.
Which ports of a motif form a handshake end, and whether the motif
sends or receives there, comes from its family in the motif library
(the end receives if its valid port is an input).

A receiving end R (bit position k, where an end is one bit position of
a wider valid port) and a sending end S form a pair if

1. R's valid is computed, through logic between motifs, from S's
   valid; or
2. S's ready is computed, through logic between motifs, from R's ready.

A sending end may form pairs with several receiving ends (a fork). A
receiving end that forms no pair is fed from the module's boundary or
from logic only.

### 2.6 Invariants (KL10)

Facts the designer states about the design, written inside the module
as `invariant` statements, each with a name:

```
invariant credits_0_accounted:
    model(credits_0.taken) == model(bank.inside[0]) + model(responses_0.held);
invariant tag_is_master_0:
    grant[0] -> master_number == 2'd0;
```

An invariant's expression may use everything allowed in logic (2.4),
and in addition:

| Form | Meaning |
|---|---|
| `a -> b` | implication: whenever `a` holds, `b` holds |
| `model(instance.name)` | a value of the instance's model, as its family's contract names it (an integer, for counts) |
| `+ -` and `< <= > >= == !=` | arithmetic and comparison on model values |

`->` is allowed only as the outermost operator of an invariant, at most
once. Each invariant must hold in every clock cycle after reset. These
forms appear only in `invariant` statements, never in `assign` or a
connection; checking enforces that (KL13). Tools that
want only the circuit skip `invariant` statements.

### 2.7 What reading, checking and linking each do (KL11)

| Layer | Needs | Does | In kernlistlib |
|---|---|---|---|
| reading | the file | syntax: the format line, modules, declarations, instances with `variant`, `assign`, `invariant` | yes |
| checking one file | the file | the format version; names declared before use, and declared once (ports, wires, instances, invariants, modules); KL08's limits in `assign` and connections, and the forms of 2.6 only in `invariant` (KL13); no `variant` on an instance of a module in the same file; widths (2.8, KL16); the logic groups of 2.4 | yes |
| linking | the file, other modules, the motif library | resolving each instantiated name to a module or a family (unresolved names are an error here); parameters and port names against the family; handshake ends and pairs (2.5); model names in invariants | later; until then, the flow's tools |

Reporting (KL14): a syntax error stops reading, and only it is
reported. A file that reads is checked completely, and every problem is
reported, each with its line.

### 2.8 Widths (KL16)

Checking works out the width of every expression from the declarations,
and a mismatch is an error, not a warning:

| Form | Width | Error when |
|---|---|---|
| a declared name | its range's width; 1 without a range | |
| `a[i]`, `a[hi:lo]` | 1; `hi - lo + 1` | an index is outside `a`'s range; the select runs against the range's direction (`a[0:3]` of `[3:0]`); `a` has no range |
| sized constant `n'b...` | `n` | its value does not fit in `n` bits |
| `{a, b}`, `{n{a, b}}` | the sum; `n` times the sum | |
| `~a` | `a`'s | |
| `&a`, `\|a`, `^a` | 1 | |
| `a & b`, `a \| b`, `a ^ b` | `a`'s | `a` and `b` differ |
| `a == b`, `a != b` | 1 | `a` and `b` differ |
| `c ? a : b` | `a`'s | `c` is not 1 bit; `a` and `b` differ |
| `assign x = e;` | | `x` and `e` differ (a concatenation on the left has the sum of its parts) |

In invariants, model values and unsized numbers are numbers, not bits:
`+ -` give a number, `< <= > >=` give 1 bit, and `== !=` compare widths
only when both sides are bits. A number cannot be an operand of `& | ^ ~`,
a reduction, a concatenation or `?:`. Each side of `->`, and each
invariant as a whole, must be 1 bit.

Connections are not checked here: the widths of a motif's ports come
from the motif library, at linking. Problems found earlier in an
expression (an undeclared name, a form allowed only in invariants) are
not reported again as width problems.

### 2.9 Forms (KL12)

The `.kl` text is the only form; Python tools read it through
kernlistlib's bindings. A JSON form may be added later if a tool needs
one.

### 2.10 Grammar

`kernlist.bnf` gives the grammar of everything above.

## 3. Not decided

- Protocol invariants (the Bluespec-derived kinds of KERN's Regime 2):
  the keyword `protocol` is reserved for their statement, and a
  `protocol` statement is rejected until it is specified (KL20).
- Invariants across clock cycles (bounded, as "within N").
- How invariants written in Chisel reach the kernlist (the front end, in
  kern-design-examples).
