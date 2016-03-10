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

// Directory before mount.
int premount_dirfd = -1;

// File descriptor value for our special pseudo-file. Arbitrary value
// that probably doesn't get used by open().
const int kFdUnko = 0;

using std::string;

string GetRelativePath(const char* path) {
  // Input is /absolute/path/below
  // convert to a relative path.

  if (*path == 0) {
    // Probably not a good idea.
    return "";
  }
  if(path[1] == 0) {
    // special-case / ?
    return "./";
  }
  return path + 1;
}

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

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi) {
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));

  // Directory would contain . and ..
  // filler(buf, ".", NULL, 0);
  // filler(buf, "..", NULL, 0);
  struct dirent **namelist{nullptr};
  int scandir_count = scandirat(premount_dirfd,
				relative_path.c_str(),
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
  o.getattr = &fs_getattr;
  o.readdir = &fs_readdir;
  o.open = &fs_open;
  o.read = &fs_read;
  return fuse_main(argc, argv, &o, NULL);
}

