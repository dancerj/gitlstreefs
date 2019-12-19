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
#include "scoped_timer.h"
#include "strutil.h"

// Benchmarking fork/exec and cat-file daemon
//
//
// $ ./out/git_cat_file_test 1000
// ForkExec 2329447
// Daemon 80830

#define ABORT_ON_ERROR(A) if ((A) == -1) { \
    perror(#A);				   \
    syslog(LOG_ERR, #A);		   \
    abort();				   \
  }

// For git-cat-file mode I want basic code that opens socket.
class BidirectionalPopen {
public:
  BidirectionalPopen(const std::vector<std::string>& command,
		     const std::string* cwd) {
    int write_pipefd[2];
    int read_pipefd[2];
    assert(0 == pipe(write_pipefd));
    assert(0 == pipe(read_pipefd));

    switch (pid_ = fork()) {
    case -1:
      // Failed to fork.
      perror("Fork");
      close(write_pipefd[0]);
      close(write_pipefd[1]);
      close(read_pipefd[0]);
      close(read_pipefd[1]);
      exit(1);
    case 0: {
      // Child process.
      //
      // Change directory before redirecting things so that error
      // message has a chance of being viewed.
      if (cwd) {
	ABORT_ON_ERROR(chdir(cwd->c_str()));
      }
      // Redirect stdout and stderr, and merge them. Do I care if I have stderr?
      ABORT_ON_ERROR(dup2(read_pipefd[1], 1));
      ABORT_ON_ERROR(dup2(read_pipefd[1], 2));
      ABORT_ON_ERROR(dup2(write_pipefd[0], 0));

      ABORT_ON_ERROR(close(read_pipefd[0]));
      ABORT_ON_ERROR(close(read_pipefd[1]));
      ABORT_ON_ERROR(close(write_pipefd[0]));
      ABORT_ON_ERROR(close(write_pipefd[1]));
      std::vector<char*> argv;
      for (auto& s: command) {
	// Const cast is necessary because the interface requires
	// mutable char* even though it probably doesn't. Love posix.
	argv.emplace_back(const_cast<char*>(s.c_str()));
      }
      argv.emplace_back(nullptr);
      ABORT_ON_ERROR(execvp(argv[0], &argv[0]));
      // Should not come here.
      exit(1);
    }
    default: {
      // Parent process.
      ABORT_ON_ERROR(close(read_pipefd[1]));
      ABORT_ON_ERROR(close(write_pipefd[0]));
      read_fd_ = read_pipefd[0];
      write_fd_ = write_pipefd[1];
    }  // end Parent process.
    }
  }

  ~BidirectionalPopen() {
    kill(pid_, SIGTERM);
    int status;
    assert(pid_ == waitpid(pid_, &status, 0));
    assert(WIFSIGNALED(status));
    assert(WTERMSIG(status) == SIGTERM);
    close(read_fd_);
    close(write_fd_);
    // int exit_code = WEXITSTATUS(status);
  }

  void Write(const std::string& s) {
    write(write_fd_, s.data(), s.size());
  }

  std::string Read(int max_size) {
    std::string buf;
    buf.resize(max_size);
    ssize_t size = read(read_fd_, &buf[0], buf.size());
    if (size != -1) {
      buf.resize(size);
    } else
      return "";
    return buf;
  }

private:
  int read_fd_{-1};
  int write_fd_{-1};
  pid_t pid_{-1};

  DISALLOW_COPY_AND_ASSIGN(BidirectionalPopen);
};

class GitCatFileProcess {
public:
  GitCatFileProcess() : process_({"/usr/bin/git", "cat-file", "--batch"}, nullptr) {}
  ~GitCatFileProcess() {}

  std::string Request(const std::string& ref, ssize_t max_response_size) {
    std::lock_guard<std::mutex> l(m_);

    process_.Write(ref + "\n");
    // TODO remove the need for max response size.
    return process_.Read(max_response_size);
  }

private:
  BidirectionalPopen process_;
  std::mutex m_;
};

const char* kConfigureJsHash = "5c7b5c80891eee3ae35687f3706567544a149e73";

void GitCatFileWithProcess(int n) {
  GitCatFileProcess d;
  // BidirectionalPopen p({"/usr/bin/git", "cat-file", "--batch"}, nullptr);
  for (int i = 0; i < n; ++i) {
    std::string result = d.Request(kConfigureJsHash, 8192);
    // std::cout << result << std::endl;
    // TODO verify the output.
  }
}

void GitCatFileWithoutProcess(int n) {
  for (int i = 0; i < n; ++i) {
    std::string result = PopenAndReadOrDie2({"git", "cat-file", "blob",
	  kConfigureJsHash},
    nullptr, nullptr);
  }
  // std::cout << result << std::endl;
}

class GitCatFileMetadata {
public:
  GitCatFileMetadata(const std::string& header) {
    auto newline = header.find('\n');
    assert(newline != std::string::npos);
    std::string first_line = header.substr(0, newline);
    auto space1 = first_line.find(' ');
    auto space2 = first_line.find(' ', space1 + 1);
    assert(space1 != std::string::npos);
    assert(space2 != std::string::npos);
    assert(space1 != space2);
    sha1_ = first_line.substr(0, space1);
    type_ = first_line.substr(space1 + 1,
			      space2 - space1 - 1);
    std::string size_str = first_line.substr(space2 + 1,
					 first_line.size() - space2 - 1);
    size_ = atoi(size_str.c_str());
  }
  ~GitCatFileMetadata() {}

  int size_{-1};
  std::string sha1_;
  std::string type_;
};

void testParseFirstLine() {
  GitCatFileMetadata m("5c7b5c80891eee3ae35687f3706567544a149e73 blob 7177\n");

  assert(m.sha1_ == "5c7b5c80891eee3ae35687f3706567544a149e73");
  assert(m.type_ == "blob");
  assert(m.size_ == 7177);
}

int main(int argc, char** argv) {
  int n = 1;
  if (argc == 2) {
    n = atoi(argv[1]);
  }
  {
    scoped_timer::ScopedTimer timer("ForkExec");
    GitCatFileWithoutProcess(n);
  }
  {
    scoped_timer::ScopedTimer timer("Process");
    GitCatFileWithProcess(n);
  }
  testParseFirstLine();
  return 0;
}
