#include "git_cat_file.h"

#include <cassert>
#include <iostream>
#include <string>

#include "get_current_dir.h"
#include "scoped_timer.h"
#include "strutil.h"

// Benchmarking fork/exec and cat-file daemon
//
//
// $ ./out/git_cat_file_test 1000
// ForkExec 2329447
// Daemon 80830

using GitCatFile::GitCatFileMetadata;
using GitCatFile::GitCatFileProcess;

const char kConfigureJsHash[] = "5c7b5c80891eee3ae35687f3706567544a149e73";

#define ASSERT_EQ(a, b, message)                                            \
  if (a != b) {                                                             \
    std::cout << #a << " " << a << ", " << #b << " " << b << " " << message \
              << std::endl;                                                 \
    assert(a == b);                                                         \
  }

void GitCatFileWithProcess(int n, const std::string& git_dir) {
  GitCatFileProcess d(&git_dir);
  for (int i = 0; i < n; ++i) {
    std::string result = d.Request(kConfigureJsHash);
    ASSERT_EQ(result.find("#!/usr/bin/env nodejs"), 0u, result);
    ASSERT_EQ(result.rfind("Emit()\n"), 7170u, result);
    ASSERT_EQ(result.size(), 7177u, result);
  }
}

void GitCatFileWithoutProcess(int n, const std::string& git_dir) {
  for (int i = 0; i < n; ++i) {
    std::string result = PopenAndReadOrDie2(
        {"git", "cat-file", "blob", kConfigureJsHash}, &git_dir, nullptr);
    ASSERT_EQ(result.find("#!/usr/bin/env nodejs"), 0u, result);
    ASSERT_EQ(result.rfind("Emit()\n"), 7170u, result);
    ASSERT_EQ(result.size(), 7177u, result);
  }
}

void testParseFirstLine() {
  GitCatFileMetadata m("5c7b5c80891eee3ae35687f3706567544a149e73 blob 7177\n");

  ASSERT_EQ(m.sha1_, "5c7b5c80891eee3ae35687f3706567544a149e73", "metadata wrong");
  ASSERT_EQ(m.type_, "blob", "blob wrong");
  ASSERT_EQ(m.size_, 7177, "size wrong");
  ASSERT_EQ(m.first_line_size_, 51, "first line size wrong");
}

void testFailureCase(const std::string& git_dir) {
  GitCatFileProcess d(&git_dir);
  try {
    std::string result = d.Request("deadbeef");
  } catch (GitCatFile::GitCatFileProcess::ObjectNotFoundException& e) {
    return;
  }
  // Should not reach here.
  assert(0);
}

int main(int argc, char** argv) {
  int n = 2;
  if (argc == 2) {
    n = atoi(argv[1]);
  }
  const std::string git_dir =
      GetCurrentDir() + "/out/fetch_test_repo/gitlstreefs";
  {
    scoped_timer::ScopedTimer timer("ForkExec");
    GitCatFileWithoutProcess(n, git_dir);
  }
  {
    scoped_timer::ScopedTimer timer("Process");
    GitCatFileWithProcess(n, git_dir);
  }
  testParseFirstLine();
  testFailureCase(git_dir);
  return 0;
}
