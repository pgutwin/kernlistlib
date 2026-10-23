#pragma once
// The logic groups of a module (doc/KERNLIST_format.md 2.4, KL17): the logic between motifs,
// divided by one rule so that every tool places and costs the same groups.
#include "veriloglib.hpp"
#include <string>
#include <vector>

namespace verilog {

  // One bit a piece of logic drives: a bit of a declared signal, or bit `index` of a motif
  // input it drives directly (port = true, signal = "instance.port"; bits from 0, the least
  // significant, as the connection's expression has them).
  struct DrivenBit {
    std::string signal;
    int64_t index = 0;
    bool port = false;
  };

  // An operator expression with nothing but wiring above it (2.4): its expression and the bits
  // it drives, least significant first.
  struct GroupPiece {
    Expr expr;
    std::vector<DrivenBit> drives;
    Location location;
  };

  struct LogicGroup {
    std::string name;                  // the first signal it drives, as written (KL17)
    std::vector<std::string> outputs;  // what it drives: assigned signals as written, or instance.port
    std::vector<std::string> inputs;   // the signals it reads from outside: motif outputs, module inputs
    int operations = 0;                // operators: ~ & | ^ (also as reductions), == != ?:
    std::vector<Location> statements;  // the assign statements and instances it comes from
    std::vector<GroupPiece> pieces;    // its operator expressions, in file order
  };

  // The groups of a module that has passed checking, in the order of their first statement.
  std::vector<LogicGroup> logic_groups(const Module& m);

} // namespace verilog
