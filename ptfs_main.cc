#define FUSE_USE_VERSION 26

#include "ptfs.h"

#include <fuse.h>
#include <stddef.h>
#include <syslog.h>

#include <iostream>

#include "update_rlimit.h"

using std::cerr;
using std::cout;
using std::endl;

struct ptfs_config {
  char* underlying_path{nullptr};
};

#define MYFS_OPT(t, p, v) { t, offsetof(ptfs_config, p), v }

static struct fuse_opt ptfs_opts[] = {
  MYFS_OPT("--underlying_path=%s", underlying_path, 0),
  FUSE_OPT_END
};
#undef MYFS_OPT

int main(int argc, char** argv) {
  openlog("ptfs", LOG_PERROR | LOG_PID, LOG_USER);
  UpdateRlimit();  // We need more than 1024 files open.
  umask(0);

  struct fuse_operations o = {};
  ptfs::FillFuseOperations<ptfs::PtfsHandler>(&o);
  fuse_args args = FUSE_ARGS_INIT(argc, argv);
  ptfs_config conf{};
  fuse_opt_parse(&args, &conf, ptfs_opts, NULL);

  if (!conf.underlying_path) {
    cerr << argv[0]
	 << " [mountpoint] --underlying_path="
	 << endl;
    return 1;
  }
  ptfs::PtfsHandler::premount_dirfd_ = open(conf.underlying_path, O_PATH|O_DIRECTORY);

  int ret = fuse_main(args.argc, args.argv, &o, NULL);
  fuse_opt_free_args(&args);
  return ret;
}
