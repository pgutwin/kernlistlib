#pragma once
// Checking one kernlist file on its own (doc/KERNLIST_format.md 2.7): everything that needs
// nothing outside the file. Every problem is reported, each with its line (KL14).
#include "veriloglib.hpp"
#include <string>
#include <vector>

namespace verilog {

  struct Problem {
    int line = 0;          // of the statement; 0 when the netlist was made by the builder
    int column = 0;
    std::string module;    // empty for problems of the file as a whole
    std::string message;
  };

  // All problems of a netlist read from one file, in the order of their lines.
  std::vector<Problem> check(const Netlist& nl);

  // "file:line:column: message", for printing
  std::string format_problem(const Problem& p, const std::string& source);

} // namespace verilog
