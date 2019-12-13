#include "strutil.h"

#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <vector>

#include "scoped_fd.h"
#include "ostream_vector.h"

#define ABORT_ON_ERROR(A) if ((A) == -1) { \
    perror(#A);				   \
    syslog(LOG_ERR, #A);		   \
    abort();				   \
  }

#define RETURN_FALSE_ON_ERROR(A) if ((A) == -1) { \
    perror(#A);					  \
    syslog(LOG_ERR, #A);			  \
    return false;				  \
  }

bool ReadFromFile(int dirfd, const std::string& filename, std::string* result) {
  ScopedFd fd(openat(dirfd, filename.c_str(), O_RDONLY | O_CLOEXEC));
  if (fd.get() == -1) {
    perror(("open ReadFile " + filename).c_str());
    abort();
  }
  struct stat st;
  RETURN_FALSE_ON_ERROR(fstat(fd.get(), &st));
  result->resize(st.st_size, '-');
  ssize_t read_length;
  RETURN_FALSE_ON_ERROR(read_length = read(fd.get(), &(*result)[0], st.st_size));
  if (st.st_size != read_length) { return false; }
  return true;
}

std::string ReadFromFileOrDie(int dirfd, const std::string& filename) {
  std::string s;
  assert(ReadFromFile(dirfd, filename, &s));
  return s;
}

// A popen implementation that does not require forking a shell. In
// gitlstreefs benchmarks, we're spending 5% of CPU time initializing
// shell startup.
std::string PopenAndReadOrDie2(const std::vector<std::string>& command,
			       const std::string* cwd,
			       int* maybe_exit_code) {
  std::string retval;  // only used on successful exit from parent.
  pid_t pid;
  int pipefd[2];
  assert(0 == pipe(pipefd));

  switch (pid = fork()) {
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
    ABORT_ON_ERROR(dup2(pipefd[1], 1));
    ABORT_ON_ERROR(dup2(pipefd[1], 2));
    ABORT_ON_ERROR(close(pipefd[0]));
    ABORT_ON_ERROR(close(pipefd[1]));
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
    ABORT_ON_ERROR(close(pipefd[1]));
    std::string readbuf;
    const int bufsize = 4096;
    readbuf.resize(bufsize);
    while(1) {
      ssize_t read_length;
      ABORT_ON_ERROR(read_length = read(pipefd[0], &readbuf[0], bufsize));
      if (read_length == -1) {
	perror("read from pipe");
	break;
      }
      if (read_length == 0) {
	break;
      }
      retval += readbuf.substr(0, read_length);
    }
    ABORT_ON_ERROR(close(pipefd[0]));
    int status;
    assert(pid == waitpid(pid, &status, 0));
    if (!WIFEXITED(status)) {
      std::cerr << "Did not complete " << command << std::endl;
      abort();
    }
    if (maybe_exit_code)
      *maybe_exit_code = WEXITSTATUS(status);
  }  // end Parent process.
  }
  return retval;
}

std::vector<std::string> SplitStringUsing(const std::string s, char c,
					  bool token_compress) {
  std::vector<std::string> result{};
  int prev = 0;
  for (std::string::size_type i = 0; i < s.size(); prev = i + 1) {
    i = s.find(c, prev);
    if (token_compress && (i - prev == 0)) {
      continue;
    }
    result.emplace_back(s.substr(prev, i - prev));
  }
  return result;
}
