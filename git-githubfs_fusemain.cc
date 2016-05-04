/*
 * 
 */
#define FUSE_USE_VERSION 26

#include <assert.h>
#include <fuse.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <memory>
#include <iostream>

#include "git-githubfs.h"

using std::string;
using std::unique_ptr;
using std::cerr;
using std::endl;

namespace githubfs {
// Global scope to make it accessible from callback.
unique_ptr<GitTree> fs;

static int fs_getattr(const char *path, struct stat *stbuf)
{
  return fs->Getattr(path, stbuf);
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
  (void) offset;
  (void) fi;

  if (path == 0 || *path != '/') {
    return -ENOENT;
  }
  // Skip the first "/"
  string fullpath(path + 1);
  FileElement* fe = fs->get(fullpath);
  if (!fe) {
    return -ENOENT;
  }

  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);
  fe->for_each_filename([&](const string& filename) {
      filler(buf, filename.c_str(), NULL, 0);
    });

  return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi)
{
  if (path == 0 || *path != '/') {
    return -ENOENT;
  }
  FileElement* fe = fs->get(path + 1);
  if (fe) {
    return 0;
  }
  return -ENOENT;
}

static int fs_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
  if (path == 0 || *path != '/') {
    return -ENOENT;
  }
  FileElement* fe = fs->get(path + 1);
  if (!fe) {
    return -ENOENT;
  }
  return fe->Read(buf, size, offset);
}
}  // namespace githubfs

struct githubfs_config {
  char* user{nullptr};
  char* project{nullptr};
  char* revision{nullptr};
};

#define MYFS_OPT(t, p, v) { t, offsetof(githubfs_config, p), v }

static struct fuse_opt githubfs_opts[] = {
  MYFS_OPT("--user=%s", user, 0),
  MYFS_OPT("--project=%s", project, 0),
  MYFS_OPT("--revision=%s", revision, 0),
  FUSE_OPT_END
};

int main(int argc, char *argv[]) {
  // Initialize fuse operations.
  struct fuse_operations o = {};
#define DEFINE_HANDLER(n) o.n = &githubfs::fs_##n
  DEFINE_HANDLER(getattr);
  DEFINE_HANDLER(readdir);
  DEFINE_HANDLER(open);
  DEFINE_HANDLER(read);
#undef DEFINE_HANDLER

  fuse_args args = FUSE_ARGS_INIT(argc, argv);
  githubfs_config conf{};
  fuse_opt_parse(&args, &conf, githubfs_opts, NULL);
  if (!(conf.user && conf.project)) {
    cerr << argv[0] << " --user=USER --project=PROJECT MOUNTPOINT --revision=HEAD" << endl
	 << " example: " << argv[0]
	 << " --user=dancerj --project=gitlstreefs mountpoint/" << endl;
    return 1;
  }

  string github_api_prefix = string("https://api.github.com/repos/") +
    conf.user + "/" + conf.project;
  githubfs::fs.reset(new githubfs::GitTree(conf.revision?conf.revision:"HEAD",
					   github_api_prefix.c_str()));
  int ret = fuse_main(args.argc, args.argv, &o, NULL);
  fuse_opt_free_args(&args);
  return ret;
}
