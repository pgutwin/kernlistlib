// Reads and checks every file of the test set (tests/kernlists, README there) and compares with
// its expectation: files in valid/ read and check without a problem; files in invalid/ whose first
// line says EXPECT reading fail to read with a syntax error; files that say EXPECT checking read,
// and checking finds at least one problem.
#include "veriloglib.hpp"
#include "verilog_writer.hpp"
#include "verilog_checker.hpp"
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static std::vector<fs::path> files_in(const std::string& folder) {
  std::vector<fs::path> out;
  for (const auto& e : fs::directory_iterator(fs::path(KERNLISTS_DIR) / folder))
    if (e.path().extension() == ".kl") out.push_back(e.path());
  std::sort(out.begin(), out.end());
  return out;
}

static std::string first_line(const fs::path& p) {
  std::ifstream in(p);
  std::string line;
  std::getline(in, line);
  return line;
}

TEST(TestSet, ValidFilesRead) {
  auto files = files_in("valid");
  ASSERT_GE(files.size(), 10u);
  for (const auto& f : files) {
    try {
      auto nl = verilog::parse_file(f.string());
      EXPECT_GE(nl.modules.size(), 1u) << f;
      for (const auto& p : verilog::check(nl))
        ADD_FAILURE() << verilog::format_problem(p, f.filename().string());
    } catch (const verilog::parse_error& e) {
      ADD_FAILURE() << f.filename() << ": " << e.what();
    }
  }
}

TEST(TestSet, InvalidFilesFailWhereExpected) {
  auto files = files_in("invalid");
  ASSERT_GE(files.size(), 30u);
  for (const auto& f : files) {
    std::string expect = first_line(f);
    bool reading = expect.find("EXPECT reading") != std::string::npos;
    bool checking = expect.find("EXPECT checking") != std::string::npos;
    ASSERT_TRUE(reading || checking) << f << " has no EXPECT line";
    bool read_ok = true;
    std::string message;
    verilog::Netlist nl;
    try { nl = verilog::parse_file(f.string()); }
    catch (const verilog::parse_error& e) { read_ok = false; message = e.what(); }
    if (reading) EXPECT_FALSE(read_ok) << f.filename() << " was read, but must fail in reading";
    if (checking) {
      EXPECT_TRUE(read_ok) << f.filename() << " must read (and fail in checking): " << message;
      if (read_ok) EXPECT_FALSE(verilog::check(nl).empty()) << f.filename() << " passed checking, but must fail it";
    }
  }
}

TEST(TestSet, SharedMemoryPortContents) {
  auto nl = verilog::parse_file((fs::path(KERNLISTS_DIR) / "valid" / "shared_memory_port.kl").string());
  ASSERT_EQ(nl.modules.size(), 1u);
  const auto& m = nl.modules[0];
  EXPECT_EQ(m.module_name, "SharedMemoryPort");
  EXPECT_EQ(m.port_list.size(), 27u);
  EXPECT_EQ(m.module_instances.size(), 15u);   // 8 FIFOs, 4 credit counters, arbiter, selector, memory
  EXPECT_EQ(m.invariants.size(), 8u);
  for (const auto& inst : m.module_instances) EXPECT_TRUE(inst.variant.has_value()) << inst.instance_name;
}

TEST(TestSet, ValidFilesWriteAndReadBack) {
  // Writing a file that was read, then reading and writing again, gives the same text
  for (const auto& f : files_in("valid")) {
    auto nl = verilog::parse_file(f.string());
    std::string written = verilog::write_netlist(nl);
    std::string again;
    try { again = verilog::write_netlist(verilog::parse_string(written)); }
    catch (const verilog::parse_error& e) { ADD_FAILURE() << f.filename() << ": written text does not read: " << e.what(); continue; }
    EXPECT_EQ(written, again) << f.filename();
  }
}

TEST(Checking, ReportsEveryProblemWithItsLine) {
  // KL14: a file that reads is checked completely
  auto nl = verilog::parse_string(
    "kernlist_format 1;\n"                       // 1
    "module m (a, b, y);\n"                      // 2
    "  input [3:0] a;\n"                         // 3
    "  input b;\n"                               // 4
    "  output [1:0] y;\n"                        // 5
    "  assign y = a + 1;\n"                      // 6  arithmetic
    "  assign y = a;\n"                          // 7  width
    "  assign y = {b, other};\n"                 // 8  not declared
    "  invariant i: (b -> b) & b;\n"             // 9  -> not outermost
    "endmodule\n");
  auto problems = verilog::check(nl);
  ASSERT_EQ(problems.size(), 4u);
  EXPECT_EQ(problems[0].line, 6);
  EXPECT_NE(problems[0].message.find("arithmetic"), std::string::npos) << problems[0].message;
  EXPECT_EQ(problems[1].line, 7);
  EXPECT_EQ(problems[1].message, "assign of 4 bits to 2 bits");
  EXPECT_EQ(problems[2].line, 8);
  EXPECT_EQ(problems[2].message, "other is not declared");
  EXPECT_EQ(problems[3].line, 9);
}

TEST(Checking, MessagesNameTheRule) {
  auto first = [](const char* file) {
    auto nl = verilog::parse_file((fs::path(KERNLISTS_DIR) / "invalid" / file).string());
    auto problems = verilog::check(nl);
    return problems.empty() ? std::string() : problems[0].message;
  };
  EXPECT_EQ(first("plus_in_assign.kl"), "+ and - are arithmetic, allowed only in invariant statements, in assign; use a motif");
  EXPECT_EQ(first("format_version_2.kl"), "format version 2 is not known; this reader knows version 1");
  EXPECT_EQ(first("used_before_declared.kl"), "w is used before its declaration on line 7");
  EXPECT_EQ(first("width_select_outside_range.kl"), "a[4] is outside a's range [3:0], in assign");
  EXPECT_EQ(first("width_constant_too_large.kl"), "the constant 2'd5 does not fit in 2 bits, in assign");
  EXPECT_EQ(first("variant_on_module_instance.kl"), "instance i of module inner has a variant; only motif instances have one");
}
