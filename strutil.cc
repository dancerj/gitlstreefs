/*BINFMTCXX:
*/
#include "strutil.h"

#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <vector>
#include <string>

#include "scoped_fd.h"

using namespace std;

std::string ReadFromFileOrDie(int dirfd, const std::string& filename) {
  string retval;
  ScopedFd fd(openat(dirfd, filename.c_str(), O_RDONLY));
  if (fd.get() == -1) {
    perror(("open ReadFile " + filename).c_str());
    abort();
  }
  struct stat st;
  int stat_result = fstat(fd.get(), &st);
  assert(stat_result != -1);
  retval.resize(st.st_size, '-');
  ssize_t read_length = read(fd.get(), &retval[0], st.st_size);
  assert(st.st_size == read_length);
  return retval;
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
    dup2(pipefd[1], 1);
    dup2(pipefd[1], 2);
    close(pipefd[0]);
    close(pipefd[1]);
    vector<char*> argv;
    for (auto& s: command) {
      // Const cast is necessary because the interface requires
      // mutable char* even though it probably doesn't. Love posix.
      argv.push_back(const_cast<char*>(s.c_str()));
    }
    argv.push_back(nullptr);
    if (cwd) {
      if (-1 == chdir(cwd->c_str())) {
	perror("chdir");
	exit(1);
      }
    }
    execvp(argv[0], &argv[0]);
    perror("execvp");
    // Should not come here.
    exit(1);
  }
  default: {
    // Parent process.
    close(pipefd[1]);
    string readbuf;
    const int bufsize = 4096;
    readbuf.resize(bufsize);
    while(1) {
      ssize_t read_length = read(pipefd[0], &readbuf[0], bufsize);
      if (read_length == -1) {
	perror("read from pipe");
	break;
      }
      if (read_length == 0) {
	break;
      }
      retval += readbuf.substr(0, read_length);
    }
    close(pipefd[0]);
    int status;
    assert(pid == waitpid(pid, &status, 0));
    assert(WIFEXITED(status));
    if (maybe_exit_code)
      *maybe_exit_code = WEXITSTATUS(status);
  }
  }
  return retval;
}
