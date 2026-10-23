# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed
- Reformatted all the files with proper indentation.
- **BREAKING**: `Expr` variant now includes `Number` as first alternative (#3)
  - External code using `std::visit` on `Expr` must add `Number` case
  - All internal code updated (writer, validator, tests)

### Added
- Number literal support in expressions (#3)
  - Numbers can now appear on RHS of assignments: `assign a = 2'hx;`
  - Numbers in concatenations: `{8'h00, data, 2'hx}`
  - Support for x/z don't-care values in based numbers
- Also added new stand-alone verilog test cases in ```tests```

### Removed

### Fixed
- Fixed issue #1: input and output statements cannot handel comma seperated lists
- Parser failures on escaped identifiers (backslash-prefixed) (#3)

## [n/a] 23 October 2026

### Added
- kern-design-examples session: forked from veriloglib at `6f02bcc` as kernlistlib (KL01).
- kern-design-examples session: `CLAUDE.md`, `doc/KERNLIST_decisions.md` and a README preamble.
- kern-design-examples session: build environment `containers/kernlistlib.def`, from the owner's
  Apptainer example (KL02).
- kern-design-examples session: `doc/KERNLIST_format.md`, the format specification (draft); file
  structure decided: Verilog modules after a required `kernlist_format 1;` (KL03).
- kern-design-examples session: format decisions KL04 (a motif's outputs named through declared
  wires) and KL05 (files end in `.kl`), in `doc/KERNLIST_format.md`.
- kern-design-examples session: format decision KL06 (a motif instance is a plain Verilog
  instantiation with `variant` after the parameters), in `doc/KERNLIST_format.md`.
- kern-design-examples session: format decision KL07 (logic between motifs as plain `assign`;
  groups by a stated rule; figures not in the file), in `doc/KERNLIST_format.md`.
- kern-design-examples session: format decision KL08 (operators: bitwise logic and selection only),
  in `doc/KERNLIST_format.md`.
- kern-design-examples session: format decision KL09 (handshake pairs worked out by a stated rule,
  not written), in `doc/KERNLIST_format.md`.
- kern-design-examples session: format decision KL10 (invariants inline, as `invariant` statements
  with their own expression language), in `doc/KERNLIST_format.md`.
- kern-design-examples session: format decision KL11 (reading and checking one file in
  kernlistlib now, linking with the motif library later), in `doc/KERNLIST_format.md`.
- kern-design-examples session: format decision KL12 (no JSON form for now); `doc/kernlist.bnf`,
  the grammar (draft).
- kern-design-examples session: baseline confirmed. The code as forked builds and passes all 39
  tests in Claude's workspace (GCC 13.3, CMake 3.28, Ninja) and in the container on the owner's
  Linux platform.
- kern-design-examples session: `tests/kernlists/`, the test set: 10 files that must be accepted
  (including the request side and the full port, derived from kern-design-examples) and 32 that
  must be rejected, each with its expected failure; `tests/kernlists/reference/`, the grammar in
  Lark with a script that checks the test set against it.

### Changed
- kern-design-examples session: format decision KL13 (the reader accepts one expression language
  everywhere; checking rejects arithmetic, ordered comparisons, unsized numbers, `model(...)` and
  `->` outside invariants, with messages naming the rule), in `doc/KERNLIST_format.md` and
  `doc/kernlist.bnf`; the test set and its reference reader follow it.
- kern-design-examples session: `doc/kernlist.bnf` gains lexical notes found with the test set:
  keywords are not identifiers; `&&`, `||` and `->` are single tokens; a sized number is one
  constant.

- kern-design-examples session: format decision KL14 (a syntax error stops reading; checking
  reports every problem), in `doc/KERNLIST_format.md`.
- kern-design-examples session: the reader reads the kernlist format (KL15): a rewritten grammar
  (`include/verilog_grammar.hpp`, following `doc/kernlist.bnf`) and reader (`src/veriloglib.cpp`),
  syntax errors with line, column and a message. The data model gains the format version, source
  locations, instance parameters, `variant`, ordered connections (a port may be unconnected),
  operators, replication, choice, model values and invariants. The writer writes all of these,
  with the format line and only the parentheses needed. The builder gains `make_replication`,
  `make_unary`, `make_binary`, `make_choice`, `make_model_ref` and `add_invariant`;
  `make_instance` takes connections. Names are held unescaped; the writer escapes them.
- kern-design-examples session: `tests/test_kernlists.cpp` reads the test set and compares with
  each file's expectation, and writes and reads back every valid file; the parse tests in
  `tests/test_verilog.cpp` are rewritten in the kernlist form, with tests for parameters,
  `variant`, operators and their precedence, invariants and error positions (53 tests).

- kern-design-examples session: format decision KL16 (width mismatches between declared signals
  are errors), with the width rules in `doc/KERNLIST_format.md` 2.8, tried first in the reference
  checker; 14 new test files that must fail on widths.
- kern-design-examples session: checking one file, `include/verilog_checker.hpp` and
  `src/verilog_checker.cpp`: `check(netlist)` returns every problem with its line (KL14): format
  version; modules, declarations, instances, invariants, ports and parameters defined once; ports
  declared; names declared before use; forms allowed only in invariants (KL13) and `->` only
  outermost; no `variant` on an instance of a module of the same file; widths (KL16). `vparse`
  prints the problems and exits with 1 if there are any. The C++ tests check every file of the
  test set against its expectation and test the messages (55 tests).

- kern-design-examples session: logic groups, `include/verilog_groups.hpp` and
  `src/verilog_groups.cpp`: `logic_groups(module)` divides the logic between motifs by the rule
  of `doc/KERNLIST_format.md` 2.4, with each group's name (KL17), inputs, outputs and number of
  operations; `vparse` lists them. Format decisions KL17 (a group is named after the signal it
  drives) and KL18 (selects and concatenations are wiring everywhere, so groups follow the
  circuit), with 2.4 rewritten; 7 tests (62 tests).

### Removed
- kern-design-examples session: `tests/test_verilog.cpp.bak`, a leftover.
- kern-design-examples session: `include/verilog_actions.hpp`, veriloglib's reader actions,
  replaced by the parse tree conversion in `src/veriloglib.cpp` (KL15).
- kern-design-examples session: from the reader and data model, as not in the format: `inout`,
  positional connections, `(* *)` read as a comment, unsized `'h` and signed constants
  (`add_inout`, `remove_inout`, `ports_pos`, `ports_named`).

## [n/a] 20 November 2025

### Changed
- Extensive update to add
  - Builder function: API access for modifying existing parsed Verilog or building "from scratch"
  - Validation function: API checks for completeness of internal representation of netlist
  - Writer function: API to write out Verilog

## [n/a] 8 October 2025

### Fixed
- Net name slicing was broken - at least now it's fixed in the testing but need to continue to investigate

## [n/a] 22 September 2025

### Fixed
- Extended testing showed that the port naming in instances was buggered up.
  - Took quite a while to sort out the actions problem.

## [n/a] 22 September 2025

### Added
- Initial version




