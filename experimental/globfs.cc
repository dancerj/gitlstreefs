// A filesystem that filters filenames with a glob.
//
// globfs mountpoint --glob_pattern='hoge*' --underlying_path=./
#define FUSE_USE_VERSION 35

#include <dirent.h>
#include <fnmatch.h>
#include <fuse.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <string>

#include "../relative_path.h"
#include "../update_rlimit.h"
#include "roptfs.h"

using std::string;
using std::unique_ptr;

class GlobFsHandler : public roptfs::RoptfsHandler {
 public:
  GlobFsHandler() {}
  virtual ~GlobFsHandler() {}

  int ReadDir(const std::string& relative_path, void* buf,
              fuse_fill_dir_t filler, off_t offset) override {
    filler(buf, ".", nullptr, 0, fuse_fill_dir_flags{});
    filler(buf, "..", nullptr, 0, fuse_fill_dir_flags{});
    struct dirent** namelist{nullptr};
    int scandir_count = scandirat(premount_dirfd_, relative_path.c_str(),
                                  &namelist, nullptr, nullptr);
    if (scandir_count == -1) {
      return -ENOENT;
    }
    for (int i = 0; i < scandir_count; ++i) {
      if (!fnmatch(glob_pattern_.c_str(), namelist[i]->d_name, FNM_PATHNAME)) {
        filler(buf, namelist[i]->d_name, nullptr, 0, fuse_fill_dir_flags{});
      }
      free(namelist[i]);
    }
    free(namelist);
    return 0;
  }

  int Open(const std::string& relative_path,
           unique_ptr<roptfs::FileHandle>* fh) override {
    if (fnmatch(glob_pattern_.c_str(), relative_path.c_str(), FNM_PATHNAME)) {
      return -ENOENT;
    }
    return RoptfsHandler::Open(relative_path, fh);
  }

  int GetAttr(const std::string& relative_path, struct stat* stbuf) override {
    if (fnmatch(glob_pattern_.c_str(), relative_path.c_str(), FNM_PATHNAME) &&
        relative_path != "./") {
      return -ENOENT;
    }
    return RoptfsHandler::GetAttr(relative_path, stbuf);
  }

  inline static string glob_pattern_{};

 private:
  DISALLOW_COPY_AND_ASSIGN(GlobFsHandler);
};

struct globfs_config {
  char* glob_pattern{nullptr};
  char* underlying_path{nullptr};
};

#define MYFS_OPT(t, p, v) \
  { t, offsetof(globfs_config, p), v }

static struct fuse_opt globfs_opts[] = {
    MYFS_OPT("--glob_pattern=%s", glob_pattern, 0),
    MYFS_OPT("--underlying_path=%s", underlying_path, 0), FUSE_OPT_END};
#undef MYFS_OPT

int main(int argc, char** argv) {
  UpdateRlimit();

  fuse_args args = FUSE_ARGS_INIT(argc, argv);
  globfs_config conf{};
  fuse_opt_parse(&args, &conf, globfs_opts, nullptr);

  if (conf.glob_pattern == nullptr || conf.underlying_path == nullptr) {
    fprintf(stderr,
            "Usage: %s [mountpoint] --glob_pattern=[pattern] "
            "--underlying_path=path\n",
            argv[0]);
    printf("%p %p\n", conf.glob_pattern, conf.underlying_path);
    return 1;
  }
  GlobFsHandler::glob_pattern_ = conf.glob_pattern;
  roptfs::RoptfsHandler::premount_dirfd_ =
      open(conf.underlying_path, O_DIRECTORY);

  struct fuse_operations o = {};
  roptfs::FillFuseOperations<GlobFsHandler>(&o);

  int ret = fuse_main(args.argc, args.argv, &o, nullptr);
  fuse_opt_free_args(&args);
  return ret;
}
