// A filesystem that filters filenames with a glob.
//
// globfs mountpoint --glob_pattern='hoge*' --underlying_path=./
#define FUSE_USE_VERSION 26

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <fuse.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>

#include "../relative_path.h"

using std::string;

// Directory before mount.
int premount_dirfd = -1;
string glob_pattern;

static int fs_getattr(const char *path, struct stat *stbuf) {
  memset(stbuf, 0, sizeof(struct stat));
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  if (fnmatch(glob_pattern.c_str(), relative_path.c_str(), FNM_PATHNAME) &&
      relative_path != "./") {
    return -ENOENT;
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

  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);
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
    if (!fnmatch(glob_pattern.c_str(), namelist[i]->d_name, FNM_PATHNAME)) {
      filler(buf, namelist[i]->d_name, nullptr, 0);
    }
    free(namelist[i]);
  }
  free(namelist);
  return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi) {
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  if (fnmatch(glob_pattern.c_str(), relative_path.c_str(), FNM_PATHNAME)) {
    return -ENOENT;
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
  ssize_t n = pread(fd, target, size, offset);
  if (n == -1) return -errno;
  return n;
}

struct globfs_config {
  char* glob_pattern{nullptr};
  char* underlying_path{nullptr};
};

#define MYFS_OPT(t, p, v) { t, offsetof(globfs_config, p), v }

static struct fuse_opt globfs_opts[] = {
  MYFS_OPT("--glob_pattern=%s", glob_pattern, 0),
  MYFS_OPT("--underlying_path=%s", underlying_path, 0),
  FUSE_OPT_END
};
#undef MYFS_OPT

int main(int argc, char** argv) {

  fuse_args args = FUSE_ARGS_INIT(argc, argv);
  globfs_config conf{};
  fuse_opt_parse(&args, &conf, globfs_opts, NULL);

  if (conf.glob_pattern == nullptr ||
      conf.underlying_path == nullptr) {
    fprintf(stderr,
	    "Usage: %s [mountpoint] --glob_pattern=[pattern] --underlying_path=path\n",
	    argv[0]);
    printf("%p %p\n", conf.glob_pattern, conf.underlying_path);
    return 1;
  }
  glob_pattern = conf.glob_pattern;
  premount_dirfd = open(conf.underlying_path, O_DIRECTORY);
  assert(premount_dirfd != -1);

  struct fuse_operations o = {};
#define DEFINE_HANDLER(n) o.n = &fs_##n
  DEFINE_HANDLER(getattr);
  DEFINE_HANDLER(readdir);
  DEFINE_HANDLER(open);
  DEFINE_HANDLER(read);
#undef DEFINE_HANDLER

  int ret = fuse_main(args.argc, args.argv, &o, NULL);
  fuse_opt_free_args(&args);
  return ret;
}

