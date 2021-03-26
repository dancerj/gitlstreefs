/*
   An IOCTL client, that takes a file as parameter and runs ioctl over it.
 */
#include <assert.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <iostream>
#include <string>

#include "gitlstree.h"

using std::cout;
using std::endl;
using std::string;

int main(int argc, char** argv) {
  assert(argc == 2);
  int fd = open(argv[1], O_RDONLY);
  assert(fd != -1);
  gitlstree::GetHashIoctlArg ioctl_arg{};
  cout << "ioctl size: " << _IOC_SIZE(gitlstree::IOCTL_GIT_HASH) << " sizeof "
       << sizeof ioctl_arg << endl;
  if (ioctl(fd, gitlstree::IOCTL_GIT_HASH, &ioctl_arg) == -1) {
    perror("ioctl");
  } else {
    ioctl_arg.verify();
    cout << string(ioctl_arg.hash, 40) << endl;
  }
  close(fd);
}
