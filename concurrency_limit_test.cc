#include "concurrency_limit.h"

#include <iostream>
#include <thread>
#include <unistd.h>
#include <vector>

using std::cout;
using std::endl;
using std::thread;
using std::vector;

int main(int argc, char** argv) {
  vector<thread> jobs;
  // 20 iterations should finish in about 3 beats
  for (int i = 0; i < 20; ++i) {
    jobs.emplace_back(thread([i]{
	  ScopedConcurrencyLimit l;
	  cout << "hello " << i << endl;
	  // sleep for a quaver
	  usleep(250000);
	}));
  }
  for (auto& job : jobs) { job.join(); }
}
