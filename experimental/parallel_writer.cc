// Does writes to multiple files in multiple threads.

#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <future>
#include <string>
#include <vector>

#include "../scoped_fd.h"

using std::async;
using std::future;
using std::string;
using std::to_string;
using std::vector;

const char kData[] = "This is one kind of data";
int num_iteration;
string path_prefix;

#define ASSERT_ERRNO(A) \
  if ((A) == -1) {      \
    perror(#A);         \
    abort();            \
  }

void TruncateAndWrite(const string& filename) {
  // Truncate and write at beginning
  ScopedFd fd(open(filename.c_str(), O_TRUNC | O_WRONLY | O_CREAT, 0777));
  ASSERT_ERRNO(fd.get());
  ASSERT_ERRNO(write(fd.get(), kData, sizeof kData));
}
void AppendToFile(const string& filename) {
  // Append.
  ScopedFd fd(open(filename.c_str(), O_WRONLY));
  ASSERT_ERRNO(fd.get());
  ASSERT_ERRNO(lseek(fd.get(), 0, SEEK_END));
  ASSERT_ERRNO(write(fd.get(), kData, sizeof kData));
}

void writer(int i) {
  for (int iteration = 0; iteration < num_iteration; ++iteration) {
    string filename(path_prefix + "/test" + to_string(i));
    TruncateAndWrite(filename);
    AppendToFile(filename);
  }
}

int main(int argc, char** argv) {
  // $0 [path prefix] [number of files] [iteration]
  assert(argc == 4);
  path_prefix = argv[1];
  const int kFiles = atoi(argv[2]);
  num_iteration = atoi(argv[3]);
  vector<future<void> > tasks;
  for (int i = 0; i < kFiles; ++i) {
    tasks.emplace_back(async([i]() { writer(i); }));
  }
}
