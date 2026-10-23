# kernlistlib decision log

Append-only. One entry per decision, numbered KL01, KL02, ... Reasons
here; the current state is in the specification documents.

## 2026-10-23: the fork

**KL01. kernlistlib is a fork of veriloglib.** Decided by the owner.
Forked at veriloglib commit `6f02bcc` and pushed to its own `origin`.
The kernlist is a Verilog-extending format (kern-design-examples DE22,
from the kernlist formats experiment there); veriloglib's PEGTL
grammar, data structures, builder, writer and validator are the
starting point. A fork rather than an extension of veriloglib, so that
the kernlist format has complete freedom and veriloglib keeps serving
its other purposes.

Not decided: whether the library's names (`veriloglib`, namespace
`verilog`, the tool `vparse`) are renamed.

**KL02. The build environment is an Apptainer container that gives
glibc 2.28.** Decided by the owner: the code must run beside commercial
EDA tools, which require glibc 2.28. `containers/kernlistlib.def`,
from the owner's example (Rocky Linux 8, GCC 13 from gcc-toolset-13,
CMake, Ninja). Claude builds and tests with the same tool versions in
its own workspace; the owner builds the container and tests on his
production platform at milestones (the baseline, after the grammar
changes, after the Python bindings).

Not decided: how PEGTL and GoogleTest are obtained inside the container
(today CMake fetches them from GitHub at configure time, which needs
network access where it is built); which Python goes into the container
for the bindings (Rocky Linux 8's default Python 3.6 is too old for
current pybind11).

## 2026-10-23: the format

**KL03. A kernlist file is Verilog modules after one required line,
`kernlist_format 1;`.** Decided by the owner, from three options: (A) a
Verilog `module ... endmodule` with KERN's statements inside; (B) its
own keyword, `kernlist ... endkernlist`; (C) a header naming the format
version and the motif library, then `kernlist` blocks. A was chosen
with C's format line made required: `module` is the foundation of
Verilog and stays; the required first statement makes a plain Verilog
file fail at once when read as a kernlist, and a kernlist fail at once
in a Verilog tool, which does not know the statement. The number is
checked: a parser refuses a version it does not know.

Not adopted: a required `motif_library` line naming the motif library
and its version. As in a usual VLSI flow, the libraries a Verilog file
needs are metadata the designer and the flow manage, not part of the
file. May be revisited.

Not decided: how motif instances and instances of other kernlist
modules are told apart (the `instance` keyword, or a rule that no
module is named like a family).

**KL04. A motif's outputs are named through declared wires.** Decided by
the owner, from three options: (A) every output connected to a declared
wire in the instance's port list, expressions using the wire, as in
Verilog; (B) outputs used directly as `instance.port`, the instance
listing only its inputs; (C) both allowed. A was chosen: it follows
Verilog's conventions, each instance statement shows all its ports in
one place, every net has a name, and veriloglib already reads it. The
cost is one wire declaration per motif output. B reads like Verilog's
hierarchical references while meaning something else; C gives two ways
to say one thing.

**KL05. Kernlist files end in `.kl`.** Decided by the owner, provided no
other use of the extension conflicts in the Linux environment. The one
established use found is Android's key layout files (`Generic.kl`),
which belong to Android devices, not to a Linux EDA environment.

**KL06. A motif instance is a plain Verilog instantiation, with
`variant` as a keyword after the parameters.** Decided by the owner,
from three options: (A) a keyword in front, `instance fifo ...`; (B)
plain Verilog instantiation, `fifo #(.DEPTH(2), .W(8)) variant
circular_buffer requests_0 (...)`, the variant left out while not yet
chosen; (C) the variant as one more parameter, `.VARIANT(...)`. B was
chosen: it reads as Verilog, and a motif family resolves like a
standard cell in a usual VLSI flow, so a name is a kernlist module if
it is defined in the files read and otherwise a motif family from the
library the flow supplies (this settles the question left open in
KL03). The variant stays separate from the parameters because the
designer sets the parameters, part of the family's contract, while KERN
chooses the variant. C would make a parameter's value a name and hide
that difference.

**KL07. The logic between motifs is written as plain `assign`
statements; its groups and their figures are not in the kernlist.**
Decided by the owner, from three options: (A) `assign` only, groups and
figures worked out by tools from the logic by a rule the specification
states; (B) a `logic` statement carrying its figures (area, levels);
(C) `assign` with figures appended. A was chosen: the kernlist
describes the circuit, as a netlist does, and figures are derived data,
as timing and parasitics are kept apart from a netlist in a usual VLSI
flow (SDF, SPEF). Storing figures beside the logic they are computed
from (B, C) lets the two drift apart. The cost: every tool must group
and cost the logic the same way, so the rule is part of the
specification. Characterized figures for logic, when they exist, can be
kept in a separate file.

**KL08. Logic between motifs uses bitwise logic and selection only.**
Decided by the owner, from three options: (A) `& | ^ ~`, the reductions
`& | ^`, `== !=`, `? :`, concatenation and replication, bit and part
selects, sized constants; no arithmetic, no ordered comparisons, no
shifts; (B) A plus `+ -`, ordered comparisons and shifts by constants;
(C) everything Verilog allows in a continuous assignment. A was chosen.
Rule: anything the motif library has a family for is written as a
motif instance, never as an operator, so that KERN sees it; the library
has an `adder` family, and `+` in logic would hide adders where KERN
would have to find them (the matching of naive Chisel, set aside in
kern-design-examples DE05). A design that needs something A forbids,
for which the library has no family, shows that a new family is needed.

**KL09. Handshake pairs are worked out from the kernlist, not written in
it.** Decided by the owner, from three options: (A) found by a rule the
specification states; (B) written as `pair` statements; (C) found by
the rule, with optional `pair` statements checked against it. A was
chosen, consistent with KL07: the file describes the circuit and
everything else is derived. The rule is the one the connection checks
in kern-design-examples used; it found exactly the right pairs in every
design so far, including a fork (one sending end, four receiving ends).
C can be added if a design ever makes the rule ambiguous (two senders'
valids merged by an OR into one receiver, for example).

**KL10. Invariants are written inline, in the module, as `invariant`
statements with their own wider expression language.** Decided by the
owner, from two options: (A) inline, as a designer writes SystemVerilog
assertions inside RTL; (B) in a separate file bound to the module, as
SystemVerilog `bind` files are. A was chosen: invariants come from the
designer's Chisel and state the designer's intent, so they are kept as
a SystemVerilog designer keeps assertions, beside the design. As with
SystemVerilog assertions, the statement has its own sublanguage
(implication, references to motif models, arithmetic on counts) that
never appears in the circuit's logic: KL08's limits on `assign` stay
intact, and tools that want only the circuit skip `invariant`
statements, as synthesis skips assertions.

Not decided: invariants across clock cycles (bounded, as "within N");
how invariants written in Chisel reach the kernlist (a question for the
front end, in kern-design-examples).

**KL11. kernlistlib reads and checks one file now; linking with the
motif library comes later.** Decided by the owner, from three options:
(A) kernlistlib reads and checks files, and linking belongs to the
flow's tools; (B) kernlistlib also reads the motif library and links;
(C) reading and checking now, linking added to kernlistlib once the
motif library's contract format settles. C was chosen. The owner's
point: resolving names is not the reader's job, as veriloglib's parser
does not resolve references either. Three layers: reading (syntax);
checking one file on its own (declarations, KL08's limits, the forms
allowed only in invariants, the version, and the logic groups of KL07,
which need nothing outside the file); linking with the motif library
(resolving names, checking parameters and ports against families,
handshake ends and so pairs per KL09, model names). Unresolved names
are an error of linking. Until linking is added, the flow's tools link,
as the prototypes in kern-design-examples do; implemented once in
kernlistlib later, so that every tool applies KL09 the same way.

**KL12. No JSON form for now.** Decided by the owner, from three
options: (A) the `.kl` text is the only form, read by Python through
the bindings; (B) a JSON form as well, converted both ways by
kernlistlib; (C) the text form and bindings only for now, a JSON
conversion added later if a tool needs one. C was chosen: the bindings
cover the Python tools, and the content is small and fixed, so a JSON
form is easy to add when a need appears.

**KL13. The reader accepts one expression language everywhere;
checking enforces KL08's limits.** Decided by the owner, from two
options: (A) the grammar has two expression languages, so the reader
rejects arithmetic, ordered comparisons, unsized numbers, `model(...)`
and `->` outside invariants, with a syntax error ("unexpected `+` at
line 6 col 16"); (B) the reader accepts the invariant language in
`assign`, connections and invariants alike, and checking rejects those
forms outside invariants with a message naming the rule ("`+` is
arithmetic, allowed only in invariant statements; use a motif"). B was
chosen: a designer who writes `+` is better served by being told why it
is not allowed and what to use instead. The grammar becomes simpler
(one expression language, with Verilog's precedence), and checking gets
one more rule. Found while writing the test set (`tests/kernlists`),
whose reference reader showed the specification (checking) and the
grammar (reading) disagreeing; with B the seven affected test files
fail in checking with such messages, and `->` below an invariant's
outermost operator is a checking error too. Forms that are in no part
of the format (`* / %`, shifts, `&&`, `||`) are still rejected by the
reader.

**KL14. A syntax error stops reading; checking reports every problem
it finds.** Decided by the owner, from three options: (A) report the
first mistake only and stop, as veriloglib does; (B) a syntax error
stops reading, and checking, on a file that reads, reports all its
problems in one run; (C) report everything, the reader skipping to the
next `;` after a syntax error and carrying on. B was chosen: since KL13
moved the common mistakes (forbidden operators) into checking, B lists
nearly all of a designer's errors in one run while the reader stays
simple; after a syntax error, what follows cannot be trusted, so later
messages under C would sometimes be wrong.

**KL15. The reader is rewritten against the grammar; the data model is
extended in place.** A technical decision, made in the session for the
owner's review. veriloglib's reader re-scanned the text of each
expression to rebuild it, which cannot carry operators and their
precedence; its data model had no source lines (needed for KL14's
messages), parameters, `variant` or invariants. The reader is now a
rule-by-rule transcription of `kernlist.bnf` in PEGTL that builds a
parse tree and converts it; veriloglib's data model, builder, writer and
validator are kept and extended. Changes to the data model: the format
version; a line and column on modules and statements; parameters,
`variant` and connections in the order written (a port may be left
unconnected) on instances; operators, replication, choice and model
values in expressions; invariants. Removed, as not in the format:
`inout`, positional connections, `(* *)` read as a comment, unsized
`'h` and signed constants. Names are held without the backslash of an
escaped identifier, and the writer escapes them where needed (the
builder used to store them escaped). Names in code (`verilog`
namespace, `vparse`) are unchanged, as KL01 left the renaming open.

