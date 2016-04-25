#include <thread>
#include <vector>

#include "scoped_fileutil.h"

using std::thread;
using std::vector;

void locker() {
  ScopedFileLockWithDelete l(AT_FDCWD, "out/scoped_fileutil_test_lock");
}

int main() {
  vector<thread> threads;
  const int kThreads = 10;

  for (int i = 0; i < kThreads; ++i) {
    threads.emplace_back(thread(locker));
  }
  for (auto &t : threads) {
    t.join();
  }
  return 0;
}
