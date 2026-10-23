# kernlistlib

The parser, writer and validator for the **kernlist**: KERN's
description of a design as motif instances, the logic between them and
their connections, the input to the floorplanner (kern-design-examples
DE08). The kernlist is a format that extends structural Verilog with
KERN's concepts as keywords (kern-design-examples DE22). This
repository is a fork of the owner's veriloglib at commit `6f02bcc`,
made so that the kernlist format has complete freedom; veriloglib
keeps serving its other purposes and is not changed from here.

Nothing is decided except what the decision log records.

## How we work

- One design question at a time, with concrete options and examples. If
  the owner says a question is unclear, re-ask it with concrete choices.
- Test an idea with a small experiment before writing it into a
  specification.
- Plain, verbose names. Avoid jargon; define a term the first time it is
  used. Always say what is decided and what is not.
- Specification documents are rewritten in place and describe only the
  current state. Reasons go in `doc/KERNLIST_decisions.md`, append-only,
  one short entry per decision, numbered KL01, KL02, ... Other
  repositories' decisions keep their numbers: "kern-design-examples
  DE22", "motif library D43".
- Record every change in `CHANGELOG.md` under `## [n/a] <D Month YYYY>`
  (reuse today's heading), with `### Added/Changed/Removed/Fixed`,
  bullets prefixed with the session name.
- Git: stage changes freely. Never commit, amend, rebase or push; the
  owner reviews and commits. Commit messages proposed to the owner
  contain no double quotes or backticks.

## Building

- The code must build for Linux with glibc 2.28, which commercial EDA
  tools require. The build environment is the Apptainer definition
  `containers/kernlistlib.def` (Rocky Linux 8, GCC 13 from
  gcc-toolset-13). Every dependency added to the code is added to the
  definition in the same change.
- Claude builds and tests in its own workspace with the same tool
  versions; that checks the code, not glibc compatibility. The owner
  builds the container and runs the tests on his production Linux
  platform at milestones; the CHANGELOG records which milestones have
  been confirmed there. Baseline confirmed 2026-10-23.

## Copies from other repositories

- Files copied from another repository are marked at the top with the
  source repository and commit, listed in a README in their directory,
  and never edited here. To change one, change the source and copy
  again. (The fork itself is not a copy: everything inherited from
  veriloglib is this repository's own and may be changed.)

## Layout

```
CLAUDE.md
README.md                    what kernlistlib is; the fork; how to build
CHANGELOG.md
CMakeLists.txt
containers/kernlistlib.def   the build environment (glibc 2.28)
doc/
  KERNLIST_decisions.md      append-only log, KL01, KL02, ...
  KERNLIST_format.md         the format specification
  kernlist.bnf               the grammar
  verilog_subset.bnf, ...    veriloglib's documents, kept as they were at the fork
include/  src/               the library and the command-line tool (vparse); the grammar is
                             include/verilog_grammar.hpp, the reader src/veriloglib.cpp,
                             checking one file src/verilog_checker.cpp,
                             logic groups src/verilog_groups.cpp
tests/                       unit tests; tests/kernlists/ the kernlist test set (README there)
python/                      Python bindings (to come)
```

Directories are created when they first get content.
