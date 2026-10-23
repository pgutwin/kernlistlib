// Reads every file of the test set (tests/kernlists, README there) and compares with its
// expectation: files in valid/ read; files in invalid/ whose first line says EXPECT reading
// fail to read with a syntax error; files that say EXPECT checking read (checking is not yet
// implemented, so only reading is tested for them here).
#include "veriloglib.hpp"
#include "verilog_writer.hpp"
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
    try { verilog::parse_file(f.string()); }
    catch (const verilog::parse_error& e) { read_ok = false; message = e.what(); }
    if (reading) EXPECT_FALSE(read_ok) << f.filename() << " was read, but must fail in reading";
    if (checking) EXPECT_TRUE(read_ok) << f.filename() << " must read (and fail in checking): " << message;
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
