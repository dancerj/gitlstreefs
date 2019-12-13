#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

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
};

int main() {
  BidirectionalPopen p({"/usr/bin/git", "cat-file", "--batch"}, nullptr);
  p.Write("5c7b5c80891eee3ae35687f3706567544a149e73\n");
  std::string result = p.Read(8192);
  std::cout << result << std::endl;
  // TODO implement something.
  return 0;
}
