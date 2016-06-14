#include "strutil.h"

#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

#include <vector>
#include <string>

#include "scoped_fd.h"

using namespace std;

#define ABORT_ON_ERROR(A) if ((A) == -1) { \
    perror(#A);				   \
    syslog(LOG_ERR, #A);		   \
    abort();				   \
  }

#define FALSE_ON_ERROR(A) if ((A) == -1) { \
    perror(#A);				   \
    syslog(LOG_ERR, #A);		   \
    return false;			   \
  }

bool ReadFromFile(int dirfd, const std::string& filename, string* result) {
  ScopedFd fd(openat(dirfd, filename.c_str(), O_RDONLY));
  if (fd.get() == -1) {
    perror(("open ReadFile " + filename).c_str());
    abort();
  }
  struct stat st;
  FALSE_ON_ERROR(fstat(fd.get(), &st));
  result->resize(st.st_size, '-');
  ssize_t read_length;
  FALSE_ON_ERROR(read_length = read(fd.get(), &(*result)[0], st.st_size));
  if (st.st_size != read_length) { return false; }
  return true;
}

std::string ReadFromFileOrDie(int dirfd, const std::string& filename) {
  string s;
  assert(ReadFromFile(dirfd, filename, &s));
  return s;
}

// A popen implementation that does not require forking a shell. In
// gitlstreefs benchmarks, we're spending 5% of CPU time initializing
// shell startup.
std::string PopenAndReadOrDie2(const std::vector<std::string>& command,
			       const string* cwd,
			       int* maybe_exit_code) {
  string retval;  // only used on successful exit from parent.
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
    // Redirect stdout and stderr, and merge them. Do I care if I have stderr?
    ABORT_ON_ERROR(dup2(pipefd[1], 1));
    ABORT_ON_ERROR(dup2(pipefd[1], 2));
    ABORT_ON_ERROR(close(pipefd[0]));
    ABORT_ON_ERROR(close(pipefd[1]));
    vector<char*> argv;
    for (auto& s: command) {
      // Const cast is necessary because the interface requires
      // mutable char* even though it probably doesn't. Love posix.
      argv.push_back(const_cast<char*>(s.c_str()));
    }
    argv.push_back(nullptr);
    if (cwd) {
      ABORT_ON_ERROR(chdir(cwd->c_str()));
    }
    ABORT_ON_ERROR(execvp(argv[0], &argv[0]));
    // Should not come here.
    exit(1);
  }
  default: {
    // Parent process.
    ABORT_ON_ERROR(close(pipefd[1]));
    string readbuf;
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
    assert(WIFEXITED(status));
    if (maybe_exit_code)
      *maybe_exit_code = WEXITSTATUS(status);
  }  // end Parent process.
  }
  return retval;
}
