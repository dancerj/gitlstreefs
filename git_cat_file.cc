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

#define ABORT_ON_ERROR(A) if ((A) == -1) { \
    perror(#A);				   \
    syslog(LOG_ERR, #A);		   \
    abort();				   \
  }

namespace GitCatFile {
BidirectionalPopen::BidirectionalPopen(const std::vector<std::string>& command,
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

BidirectionalPopen::~BidirectionalPopen() {
    kill(pid_, SIGTERM);
    int status;
    assert(pid_ == waitpid(pid_, &status, 0));
    assert(WIFSIGNALED(status));
    assert(WTERMSIG(status) == SIGTERM);
    close(read_fd_);
    close(write_fd_);
    // int exit_code = WEXITSTATUS(status);
  }

void BidirectionalPopen::Write(const std::string& s) {
  write(write_fd_, s.data(), s.size());
}

std::string BidirectionalPopen::Read(int max_size) {
  std::string buf;
  buf.resize(max_size);
  ssize_t size = read(read_fd_, &buf[0], buf.size());
  if (size != -1) {
    buf.resize(size);
  } else
    return "";
  return buf;
}

GitCatFileMetadata::GitCatFileMetadata(const std::string& header) {
  auto newline = header.find('\n');
  assert(newline != std::string::npos);
  std::string first_line = header.substr(0, newline);
  first_line_size_ = first_line.size() + 1;
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

GitCatFileMetadata::~GitCatFileMetadata() {}

GitCatFileProcess::GitCatFileProcess(const std::string* cwd) :
  process_({"/usr/bin/git", "cat-file", "--batch"}, cwd) {}
GitCatFileProcess::~GitCatFileProcess() {}

std::string GitCatFileProcess::Request(const std::string& ref) {
  // TODO: probably getting the full response string is not what the
  // user wants, but more structured with metadata vs blob content.
  const int kMaxHeaderSize = 4096;
  std::lock_guard<std::mutex> l(m_);

  process_.Write(ref + "\n");
  std::string result = process_.Read(kMaxHeaderSize);
  GitCatFileMetadata metadata(result);
  if (metadata.size_ > kMaxHeaderSize) {
    result += process_.Read(metadata.size_ + metadata.first_line_size_ - kMaxHeaderSize);
  }
  return result;
}
}  // namespace GitCatFile
