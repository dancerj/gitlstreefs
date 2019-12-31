#include "git_cat_file.h"

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

#include <cassert>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include "disallow.h"
#include "strutil.h"

#define ABORT_ON_ERROR(A) if ((A) == -1) {	\
    perror(#A);					\
    syslog(LOG_ERR, #A);			\
    abort();					\
  }

namespace GitCatFile {

BidirectionalPopen::BidirectionalPopen(const std::vector<std::string>& command,
				      const std::string* cwd) {
  auto read_pipe = ScopedPipe();
  auto write_pipe = ScopedPipe();

  switch (pid_ = fork()) {
  case -1:
    // Failed to fork.
    perror("Fork");
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
    ABORT_ON_ERROR(dup2(read_pipe.second.get(), 1));
    ABORT_ON_ERROR(dup2(read_pipe.second.get(), 2));
    ABORT_ON_ERROR(dup2(write_pipe.first.get(), 0));

    read_pipe.first.clear();
    read_pipe.second.clear();
    write_pipe.first.clear();
    write_pipe.second.clear();

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
    read_pipe.second.clear();
    write_pipe.first.clear();
    read_fd_.reset(read_pipe.first.release());
    write_fd_.reset(write_pipe.second.release());
  }  // end Parent process.
  }
}

BidirectionalPopen::~BidirectionalPopen() {
  kill(pid_, SIGTERM);
  int status;
  assert(pid_ == waitpid(pid_, &status, 0));
  assert(WIFSIGNALED(status));
  assert(WTERMSIG(status) == SIGTERM);
  // int exit_code = WEXITSTATUS(status);
}

void BidirectionalPopen::Write(const std::string& s) const {
  write(write_fd_.get(), s.data(), s.size());
}

std::string BidirectionalPopen::Read(int max_size) const {
  std::string buf;
  buf.resize(max_size);
  ssize_t size = read(read_fd_.get(), &buf[0], buf.size());
  if (size != -1) {
    buf.resize(size);
  } else
    return "";
  return buf;
}

#define ASSERT_NE(A, B, CONTEXT) {				\
    if ((A) == (B)) {						\
      std::cout << #A << "[" << A << "] != " <<			\
      #B << " [" << B << "] [" << CONTEXT << "]" << std::endl;	\
      assert(0);						\
    }}

GitCatFileMetadata::GitCatFileMetadata(const std::string& header) {
  auto newline = header.find('\n');
  ASSERT_NE(newline, std::string::npos, header);
  std::string first_line = header.substr(0, newline);
  first_line_size_ = first_line.size() + 1;
  auto space1 = first_line.find(' ');
  ASSERT_NE(space1, std::string::npos, header);
  sha1_ = first_line.substr(0, space1);

  auto space2 = first_line.find(' ', space1 + 1);
  if (space2 == std::string::npos) {
    // There's only two when the object could not be found.
    type_ = first_line.substr(space1 + 1,
			      newline - space1 - 1);
    return;
  }
  ASSERT_NE(space2, std::string::npos, header);
  ASSERT_NE(space1, space2, header);
  type_ = first_line.substr(space1 + 1,
			    space2 - space1 - 1);
  std::string size_str = first_line.substr(space2 + 1,
					   first_line.size() - space2 - 1);
  size_ = atoi(size_str.c_str());
}

GitCatFileMetadata::~GitCatFileMetadata() {}

GitCatFileProcess::GitCatFileProcess(const std::string* cwd) :
  process_({"/usr/bin/git", "cat-file", "--batch"}, cwd) {}

GitCatFileProcess::GitCatFileProcess(const std::string& cwd, const std::string& ssh) :
  process_({"/usr/bin/ssh", ssh, "cd", cwd, "&&", "/usr/bin/git", "cat-file", "--batch"},
	   nullptr /* local cwd should not matter */) {}

GitCatFileProcess::~GitCatFileProcess() {}

std::string GitCatFileProcess::Request(const std::string& ref) const {
  const int kMaxHeaderSize = 4096;
  std::lock_guard<std::mutex> l(m_);

  process_.Write(ref + "\n");
  std::string response = process_.Read(kMaxHeaderSize);

  const GitCatFileMetadata metadata{response};
  if (metadata.type_ == "missing") {
    throw ObjectNotFoundException();
  }

  int remaining;
  while ((remaining = (metadata.size_ +
		       metadata.first_line_size_ +
		       1 /* closing LF */)  /* The size of what we want to read */
	  - response.size() /* What we've already read */
	  ) > 0) {
    response += process_.Read(remaining);
  }
  assert(response[response.size()-1] == '\n');
  return response.substr(metadata.first_line_size_, metadata.size_);
}

}  // namespace GitCatFile
