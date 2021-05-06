#define FUSE_USE_VERSION 35

#include <assert.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <memory>

#include "get_current_dir.h"
#include "gitfs.h"

using std::string;
using std::unique_ptr;

namespace gitfs {
// Global scope to make it accessible from callback.
unique_ptr<GitTree> fs;

static int fs_getattr(const char *path, struct stat *stbuf, fuse_file_info *) {
  return fs->Getattr(path, stbuf);
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      fuse_readdir_flags) {
  (void)offset;
  (void)fi;

  if (path == 0 || *path != '/') {
    return -ENOENT;
  }
  // Skip the first "/"
  string fullpath(path + 1);
  FileElement *fe = fs->get(fullpath);
  if (!fe) {
    return -ENOENT;
  }

  filler(buf, ".", nullptr, 0, fuse_fill_dir_flags{});
  filler(buf, "..", nullptr, 0, fuse_fill_dir_flags{});
  fe->for_each_filename([&](const string &filename) {
    filler(buf, filename.c_str(), nullptr, 0, fuse_fill_dir_flags{});
  });

  return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi) {
  if (path == 0 || *path != '/') {
    return -ENOENT;
  }
  FileElement *fe = fs->get(path + 1);
  if (fe) {
    return 0;
  }
  return -ENOENT;
}

static int fs_read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
  if (path == 0 || *path != '/') {
    return -ENOENT;
  }
  FileElement *fe = fs->get(path + 1);
  if (!fe) {
    return -ENOENT;
  }
  return fe->Read(buf, size, offset);
}
}  // namespace gitfs

int main(int argc, char *argv[]) {
  gitfs::fs.reset(new gitfs::GitTree("HEAD", GetCurrentDir()));

  struct fuse_operations o = {};
#define DEFINE_HANDLER(n) o.n = &gitfs::fs_##n
  DEFINE_HANDLER(getattr);
  DEFINE_HANDLER(readdir);
  DEFINE_HANDLER(open);
  DEFINE_HANDLER(read);
#undef DEFINE_HANDLER
  return fuse_main(argc, argv, &o, nullptr);
}
