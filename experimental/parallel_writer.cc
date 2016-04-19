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
const int kIteration = 100;
const int kFiles = 100;

void writer(int i) {
  for (int iteration = 0; iteration < kIteration; ++iteration) {
    string filename("test" + to_string(i));
    {
      ScopedFd fd(open(filename.c_str(), O_TRUNC|O_WRONLY|O_CREAT, 0777));
      assert(-1 != fd.get());
      assert(-1 != write(fd.get(), kData, sizeof kData));
    }
    {
      ScopedFd fd(open(filename.c_str(), O_WRONLY));
      assert(-1 != fd.get());
      assert(-1 != lseek(fd.get(), 0, SEEK_END));
      assert(-1 != write(fd.get(), kData, sizeof kData));
    }
  }
}

int main() {
  vector<thread> threads;
  for (int i = 0; i < kFiles; ++i) {
    threads.emplace_back(thread(bind(writer, i)));
  }
  for (auto &t : threads) {
    t.join();
  }
}
