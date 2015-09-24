/*BINFMTCXX:
*/
#include "strutil.h"

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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

std::string PopenAndReadOrDie(const std::string& command,
			      int* maybe_exit_code) {
  string retval;
  string readbuf;
  const int bufsize = 4096;
  readbuf.resize(bufsize);
  FILE* f = popen(command.c_str(), "r");
  assert(f != NULL);
  while(1) {
    size_t read_length = fread(&readbuf[0], 1, bufsize, f);
    if (read_length == 0) {
      // end of file or error, stop reading.
      assert(feof(f));
      break;
    }
    retval += readbuf.substr(0, read_length);
  }
  int exit_code = pclose(f);
  if (maybe_exit_code)
    *maybe_exit_code = exit_code;

  return retval;
}
