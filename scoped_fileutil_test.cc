#include <fcntl.h>

#include <future>
#include <vector>

#include "scoped_fileutil.h"

using std::async;
using std::future;
using std::vector;

void locker() {
  ScopedFileLockWithDelete l(AT_FDCWD, "out/scoped_fileutil_test_lock");
}

int main() {
  vector<future<void> > tasks;
  constexpr int kThreads = 10;

  for (int i = 0; i < kThreads; ++i) {
    tasks.emplace_back(async(locker));
  }
  return 0;
}
