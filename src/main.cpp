#include "veriloglib.hpp"
#include "verilog_checker.hpp"
#include "verilog_groups.hpp"
#include "verilog_grammar.hpp"
#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/trace.hpp>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: vparse [--trace] <file.kl>\n";
    return 1;
  }

  bool do_trace = false;
  const char* filename = nullptr;

  // Very simple option handling:
  //   vparse file.v
  //   vparse --trace file.v
  if (std::string(argv[1]) == "--trace") {
    if (argc < 3) {
      std::cerr << "Usage: vparse [--trace] <file.kl>\n";
      return 1;
    }
    do_trace = true;
    filename = argv[2];
  } else {
    filename = argv[1];
  }

  try {
    if (do_trace) {
      // Run PEGTL's standard trace on the full Verilog grammar.
      // This will dump the enter/exit events for each rule as it parses.
      tao::pegtl::file_input<> in(filename);
      const bool ok = tao::pegtl::standard_trace<verilog::grammar::file>(in);
      return ok ? 0 : 2;
    }

    // Normal path: just parse and print a summary.
    verilog::Netlist nl = verilog::parse_file(filename);
    std::cout << "Format version: " << nl.format_version << "\n";
    std::cout << "Parsed modules: " << nl.modules.size() << "\n";
    for (const auto& m : nl.modules) {
      std::cout << m.summary() << "\n";
    }
    // Checking one file (KL11, KL14): every problem, each with its line
    auto problems = verilog::check(nl);
    for (const auto& p : problems) std::cerr << verilog::format_problem(p, filename) << "\n";
    if (!problems.empty()) return 1;
    // The logic groups (KL17)
    for (const auto& m : nl.modules) {
      for (const auto& g : verilog::logic_groups(m)) {
        std::cout << "group " << g.name << " (line " << g.statements.front().line << "): "
                  << g.operations << (g.operations == 1 ? " operation" : " operations") << "; reads";
        for (const auto& i : g.inputs) std::cout << " " << i;
        std::cout << "; drives";
        for (const auto& o : g.outputs) std::cout << " " << o;
        std::cout << "\n";
      }
    }
  } catch (const verilog::parse_error& e) {
    std::cerr << "Parse error: " << e.what() << "\n";
    return 2;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 3;
  }

  return 0;
}
