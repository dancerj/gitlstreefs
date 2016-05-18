#include <sys/resource.h>
#include <stdio.h>

#include <iostream>

#include "update_rlimit.h"

using std::cout;
using std::endl;

void UpdateRlimit() {
  struct rlimit r;
  if (-1 == getrlimit(RLIMIT_NOFILE, &r)) {
    perror("getrlimit");
    return;
  }
  cout << "Updating file open limit: "
       << r.rlim_cur << " to " << r.rlim_max << endl;
  r.rlim_cur = r.rlim_max;
  if (-1 == setrlimit(RLIMIT_NOFILE, &r)) {
    perror("setrlimit");
    return;
  }
}
