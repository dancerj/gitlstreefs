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

using namespace std;

std::string ReadFromFileOrDie(const std::string& filename) {
  string retval;
  int fd = open(filename.c_str(), O_RDONLY);
  assert(fd != -1);
  struct stat st;
  int stat_result = fstat(fd, &st);
  assert(stat_result != -1);
  retval.resize(st.st_size, '-');
  ssize_t read_length = read(fd, &retval[0], st.st_size);
  assert(-1 != read_length);
  return retval;
}

// A popen implementation that does not require forking a shell. In
// gitlstreefs benchmarks, we're spending 5% of CPU time initializing
// shell startup.
std::string PopenAndReadOrDie2(const std::vector<std::string>& command,
			       const string* cwd,
			       int* maybe_exit_code) {
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
    // Redirect stdout. TODO: what to do with stderr?
    dup2(pipefd[1], 1);
    close(pipefd[0]);
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
    string retval;
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

    return retval;
  }
  }
}
