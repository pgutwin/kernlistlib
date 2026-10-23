# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### kernlistlib
- Forked from veriloglib at `6f02bcc` (KL01). Added `CLAUDE.md`,
  `doc/KERNLIST_decisions.md`, a README preamble, and the build
  environment `containers/kernlistlib.def` (KL02).
- Baseline, the code as forked: builds and passes all 39 tests with
  GCC 13.3, CMake 3.28 and Ninja in Claude's workspace (2026-10-23).
  In the container on the production platform: not yet confirmed.

### veriloglib, before the fork

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




