// A file system that adds a file 'unko' on all directories containing 'unkounko'
//
// Proof of concept that you can overlay mountpoint over existing
// directory and still read it.
//
// unkofs mountpoint -o nonempty
#define FUSE_USE_VERSION 26

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>
#include <memory>

#include "../relative_path.h"

// Directory before mount.
int premount_dirfd = -1;

// File descriptor value for our special pseudo-file. Arbitrary value
// that probably doesn't get used by open().
const int kFdUnko = 0;

using std::string;
using std::unique_ptr;

static int fs_getattr(const char *path, struct stat *stbuf) {
  memset(stbuf, 0, sizeof(struct stat));
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  if (relative_path == "unko") {
    stbuf->st_size = 8;
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_nlink = 1;
    return 0;
  }

  if (-1 == fstatat(premount_dirfd, relative_path.c_str(), stbuf, AT_SYMLINK_NOFOLLOW)) {
    return -errno;
  } else {
    return 0;
  }
}

static int fs_opendir(const char* path, struct fuse_file_info* fi) {
  if (*path == 0)
    return -ENOENT;
  unique_ptr<string> relative_path(new string(GetRelativePath(path)));
  fi->fh = reinterpret_cast<uint64_t>(relative_path.release());
  return 0;
}

static int fs_releasedir(const char*, struct fuse_file_info* fi) {
  if (fi->fh == 0)
    return -EBADF;
  unique_ptr<string> auto_delete(reinterpret_cast<string*>(fi->fh));
  return 0;
}

static int fs_readdir(const char *unused, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi) {
  if (fi->fh == 0)
    return -ENOENT;
  string* relative_path(reinterpret_cast<string*>(fi->fh));

  // Directory would contain . and ..
  // filler(buf, ".", NULL, 0);
  // filler(buf, "..", NULL, 0);
  struct dirent **namelist{nullptr};
  int scandir_count = scandirat(premount_dirfd,
				relative_path->c_str(),
				&namelist,
				nullptr,
				nullptr);
  if (scandir_count == -1) {
    return -ENOENT;
  }
  for(int i = 0; i < scandir_count; ++i) {
    filler(buf, namelist[i]->d_name, nullptr, 0);
    free(namelist[i]);
  }
  filler(buf, "unko", nullptr, 0);
  free(namelist);
  return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi) {
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  if (relative_path == "unko") {
    fi->fh = kFdUnko;
    return 0;
  }
  int fd = openat(premount_dirfd, relative_path.c_str(), O_RDONLY);
  if (fd == -1)
    return -ENOENT;
  fi->fh = static_cast<uint64_t>(fd);

  return 0;
}

static int fs_read(const char *path, char *target, size_t size, off_t offset,
		   struct fuse_file_info *fi) {
  int fd = fi->fh;
  if (fd == -1)
    return -ENOENT;
  if (fd == kFdUnko) {
    if (size <= 8) { return -EIO; }
    memcpy(target, "unkounko", 8);
    return 8;
  }
  ssize_t n = pread(fd, target, size, offset);
  if (n == -1) return -errno;
  return n;
}

int main(int argc, char** argv) {
  const char* mountpoint = argv[1];
  assert(argc > 1);
  premount_dirfd = open(mountpoint, O_DIRECTORY);
  assert(premount_dirfd != -1);

  struct fuse_operations o = {};
#define DEFINE_HANDLER(n) o.n = &fs_##n
  DEFINE_HANDLER(getattr);
  DEFINE_HANDLER(open);
  DEFINE_HANDLER(opendir);
  DEFINE_HANDLER(read);
  DEFINE_HANDLER(readdir);
  DEFINE_HANDLER(releasedir);
#undef DEFINE_HANDLER
  o.flag_nopath = true;

  return fuse_main(argc, argv, &o, NULL);
}

