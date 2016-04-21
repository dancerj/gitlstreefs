// Does writes to multiple files in multiple threads.

#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <thread>
#include <vector>

#include "../scoped_fd.h"

using std::vector;
using std::bind;
using std::thread;
using std::string;
using std::to_string;

const char kData[] = "This is one kind of data";
int num_iteration;

#define ASSERT_ERRNO(A) if ((A) == -1) {   \
    perror(#A);				   \
    abort();				   \
  }

void TruncateAndWrite(const string& filename) {
  // Truncate and write at beginning
  ScopedFd fd(open(filename.c_str(), O_TRUNC|O_WRONLY|O_CREAT, 0777));
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
    string filename("test" + to_string(i));
    TruncateAndWrite(filename);
    AppendToFile(filename);
  }
}

int main(int argc, char** argv) {
  // $0 [number of files] [iteration]
  assert(argc == 3);
  const int kFiles = atoi(argv[1]);
  num_iteration = atoi(argv[2]);
  vector<thread> threads;
  for (int i = 0; i < kFiles; ++i) {
    threads.emplace_back(thread(bind(writer, i)));
  }
  for (auto &t : threads) {
    t.join();
  }
}
