#include "git_cat_file.h"

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <mutex>

#include "disallow.h"
#include "get_current_dir.h"
#include "scoped_timer.h"
#include "strutil.h"

// Benchmarking fork/exec and cat-file daemon
//
//
// $ ./out/git_cat_file_test 1000
// ForkExec 2329447
// Daemon 80830

using GitCatFile::GitCatFileProcess;
using GitCatFile::GitCatFileMetadata;

const char* kConfigureJsHash = "5c7b5c80891eee3ae35687f3706567544a149e73";

void GitCatFileWithProcess(int n, const std::string& git_dir) {
  GitCatFileProcess d(&git_dir);
  // BidirectionalPopen p({"/usr/bin/git", "cat-file", "--batch"}, nullptr);
  for (int i = 0; i < n; ++i) {
    std::string result = d.Request(kConfigureJsHash);
    // std::cout << result << std::endl;
    assert(result.find("#!/usr/bin/env nodejs") != std::string::npos);
  }
}

void GitCatFileWithoutProcess(int n, const std::string& git_dir) {
  for (int i = 0; i < n; ++i) {
    std::string result = PopenAndReadOrDie2({"git", "cat-file", "blob",
	  kConfigureJsHash},
      &git_dir, nullptr);
    assert(result.find("#!/usr/bin/env nodejs") != std::string::npos);
  }
  // std::cout << result << std::endl;
}

void testParseFirstLine() {
  GitCatFileMetadata m("5c7b5c80891eee3ae35687f3706567544a149e73 blob 7177\n");

  assert(m.sha1_ == "5c7b5c80891eee3ae35687f3706567544a149e73");
  assert(m.type_ == "blob");
  assert(m.size_ == 7177);
  assert(m.first_line_size_ == 51);
}

int main(int argc, char** argv) {
  int n = 1;
  if (argc == 2) {
    n = atoi(argv[1]);
  }
  const std::string git_dir = GetCurrentDir() + "/out/fetch_test_repo/gitlstreefs";
  {
    scoped_timer::ScopedTimer timer("ForkExec");
    GitCatFileWithoutProcess(n, git_dir);
  }
  {
    scoped_timer::ScopedTimer timer("Process");
    GitCatFileWithProcess(n, git_dir);
  }
  testParseFirstLine();
  return 0;
}
