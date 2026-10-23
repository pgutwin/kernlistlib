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
