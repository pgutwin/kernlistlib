#include "veriloglib.hpp"
#include "verilog_grammar.hpp"
#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/trace.hpp>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: vparse [--trace] <file.v>\n";
    return 1;
  }

  bool do_trace = false;
  const char* filename = nullptr;

  // Very simple option handling:
  //   vparse file.v
  //   vparse --trace file.v
  if (std::string(argv[1]) == "--trace") {
    if (argc < 3) {
      std::cerr << "Usage: vparse [--trace] <file.v>\n";
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
      const bool ok = tao::pegtl::standard_trace<verilog::grammar::start>(in);
      return ok ? 0 : 2;
    }

    // Normal path: just parse and print a summary.
    verilog::Netlist nl = verilog::parse_file(filename);
    std::cout << "Parsed modules: " << nl.modules.size() << "\n";
    for (const auto& m : nl.modules) {
      std::cout << m.summary() << "\n";
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
