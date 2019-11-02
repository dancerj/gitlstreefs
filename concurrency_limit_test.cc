#include "concurrency_limit.h"

#include <cassert>
#include <future>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

using std::async;
using std::cout;
using std::endl;
using std::future;
using std::lock_guard;
using std::mutex;
using std::string;
using std::thread;
using std::vector;

void TestThread() {
  vector<thread> jobs;
  int counter = 0;
  std::mutex m{};

  // 20 iterations should finish in about 3 beats
  for (int i = 0; i < 20; ++i) {
    jobs.emplace_back(thread([i, &m, &counter]{
	  char buffer[20];
	  sprintf(buffer, "task %i", i);
	  ScopedConcurrencyLimit l(buffer);
	  // sleep for a quaver
	  usleep(250000);
	  {
	    lock_guard<mutex> l(m);
	    counter++;
	  }
	}));
  }
  for (auto& job : jobs) { job.join(); }
  assert(counter == 20);
}

void TestAsync() {
int counter = 0;
  std::mutex m{};
  {
    vector<future<void> > jobs;

    // 20 iterations should finish in about 3 beats
    for (int i = 0; i < 20; ++i) {
      jobs.emplace_back(async([i, &m, &counter]{
	    char buffer[20];
	    sprintf(buffer, "task %i", i);
	    ScopedConcurrencyLimit l(buffer);
	    // sleep for a quaver
	    usleep(250000);
	    {
	      lock_guard<mutex> l(m);
	      counter++;
	    }
	  }));
    }
  }

  assert(counter == 20);
}


int main(int argc, char** argv) {
  TestThread();
  TestAsync();
}
