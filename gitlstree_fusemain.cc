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
unique_ptr<directory_container::DirectoryContainer> fs;

static int fs_getattr(const char *path, struct stat *stbuf)
{
  return fs->Getattr(path, stbuf);
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi) {
  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);
  const directory_container::Directory* d = dynamic_cast<
    directory_container::Directory*>(fs->mutable_get(path));
  if (!d) return -ENOENT;
  d->for_each([&](const string& s, const directory_container::File* unused){
      filler(buf, s.c_str(), NULL, 0);
    });
  return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi)
{
  if (path == 0 || *path != '/') {
    return -ENOENT;
  }

  FileElement* f = dynamic_cast<FileElement*>(fs->mutable_get(path));
  if (!f)
    return -ENOENT;
  fi->fh = reinterpret_cast<uint64_t>(f);

  f->Open();
  return 0;
}

static int fs_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
  FileElement* fe = dynamic_cast<FileElement*>(reinterpret_cast<directory_container::File*>(fi->fh));
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
  string ssh(conf.ssh?conf.ssh:"");

  gitlstree::fs.reset(new directory_container::DirectoryContainer());
  gitlstree::LoadDirectory(path, revision, ssh, gitlstree::fs.get());

  int ret = fuse_main(args.argc, args.argv, &o, NULL);
  fuse_opt_free_args(&args);
  return ret;
}
