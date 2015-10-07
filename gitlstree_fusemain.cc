#define FUSE_USE_VERSION 26

#include <assert.h>
#include <fuse.h>
#include <memory>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "get_current_dir.h"
#include "gitlstree.h"

using std::string;
using std::unique_ptr;

namespace gitlstree {
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
}  // namespace gitlstree

struct gitlstree_config {
  char* ssh{nullptr};
  char* path{nullptr};
  char* revision{nullptr};
};

#define MYFS_OPT(t, p, v) { t, offsetof(gitlstree_config, p), v }

static struct fuse_opt gitlstree_opts[] = {
  MYFS_OPT("--ssh=%s", ssh, 0),
  MYFS_OPT("--path=%s", path, 0),
  MYFS_OPT("--revision=%s", revision, 0),
  FUSE_OPT_END
};

int main(int argc, char *argv[]) {

  struct fuse_operations o = {};
  o.getattr = &gitlstree::fs_getattr;
  o.readdir = &gitlstree::fs_readdir;
  o.open = &gitlstree::fs_open;
  o.read = &gitlstree::fs_read;

  fuse_args args = FUSE_ARGS_INIT(argc, argv);
  gitlstree_config conf{};
  fuse_opt_parse(&args, &conf, gitlstree_opts, NULL);

  string revision(conf.revision?conf.revision:"HEAD");
  string path(conf.path?conf.path:GetCurrentDir());

  gitlstree::fs.reset(new gitlstree::GitTree(revision.c_str(),
					     conf.ssh,
					     path));

  int ret = fuse_main(args.argc, args.argv, &o, NULL);
  fuse_opt_free_args(&args);
  return ret;
}
