#define FUSE_USE_VERSION 26

#include <assert.h>
#include <fuse.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <memory>

#include "get_current_dir.h"
#include "gitlstree.h"

using std::string;
using std::unique_ptr;

namespace{
// make it accessible from callback.
unique_ptr<directory_container::DirectoryContainer> fs{};
}

namespace gitlstree {

static int fs_getattr(const char *path, struct stat *stbuf)
{
  return fs->Getattr(path, stbuf);
}

static int fs_opendir(const char* path, struct fuse_file_info* fi) {
   if (path == 0 || *path != '/') {
     return -ENOENT;
   }
  const auto d = dynamic_cast<
    directory_container::Directory*>(fs->mutable_get(path));
  if (!d) return -ENOENT;
  fi->fh = reinterpret_cast<uint64_t>(d);
  return 0;
}

static int fs_releasedir(const char*, struct fuse_file_info* fi) {
  if (fi->fh == 0)
    return -EBADF;
  return 0;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi) {
  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);
  auto* d = reinterpret_cast<const directory_container::Directory*>(fi->fh);
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

  const auto f = reinterpret_cast<directory_container::File*>(fs->mutable_get(path));
  if (!f)
    return -ENOENT;
  fi->fh = reinterpret_cast<uint64_t>(f);

  f->Open();
  return 0;
}

static int fs_read(const char *path, char *buf, size_t size, off_t offset,
		   struct fuse_file_info *fi)
{
  const auto fe = reinterpret_cast<directory_container::File*>(fi->fh);
  if (!fe) {
    return -ENOENT;
  }
  return fe->Read(buf, size, offset);
}

static int fs_release(const char *path, struct fuse_file_info *fi) {
  const auto fe = reinterpret_cast<directory_container::File*>(fi->fh);
  if (!fe) {
    return -ENOENT;
  }
  return fe->Release();
}

static int fs_ioctl(const char *path, int cmd, void *arg,
		    struct fuse_file_info *fi, unsigned int flags, void *data) {
  if (flags & FUSE_IOCTL_COMPAT)
    return -ENOSYS;

  const auto fe = dynamic_cast<FileElement*>(reinterpret_cast<directory_container::File*>(fi->fh));
  if (!fe) {
    return -ENOENT;
  }
  switch (cmd) {
    case IOCTL_GIT_HASH: {
      gitlstree::GetHashIoctlArg *ioctl_arg = new (data) GetHashIoctlArg();
      fe->GetHash(ioctl_arg->hash);
      return 0;
    }
  }
  return -EINVAL;
}

}  // namespace gitlstree

struct gitlstree_config {
  char* ssh{nullptr};
  char* path{nullptr};
  char* revision{nullptr};
  char* cache_path{nullptr};
};

#define MYFS_OPT(t, p, v) { t, offsetof(gitlstree_config, p), v }

static struct fuse_opt gitlstree_opts[] = {
  MYFS_OPT("--ssh=%s", ssh, 0),
  MYFS_OPT("--path=%s", path, 0),
  MYFS_OPT("--revision=%s", revision, 0),
  MYFS_OPT("--cache_path=%s", cache_path, 0),
  FUSE_OPT_END
};

int main(int argc, char *argv[]) {

  struct fuse_operations o = {};

#define DEFINE_HANDLER(n) o.n = &gitlstree::fs_##n
  DEFINE_HANDLER(getattr);
  DEFINE_HANDLER(ioctl);
  DEFINE_HANDLER(open);
  DEFINE_HANDLER(opendir);
  DEFINE_HANDLER(read);
  DEFINE_HANDLER(readdir);
  DEFINE_HANDLER(release);
  DEFINE_HANDLER(releasedir);
#undef DEFINE_HANDLER
  o.flag_nopath = true;

  fuse_args args = FUSE_ARGS_INIT(argc, argv);
  gitlstree_config conf{};
  fuse_opt_parse(&args, &conf, gitlstree_opts, NULL);

  string revision(conf.revision?conf.revision:"HEAD");
  string path(conf.path?conf.path:GetCurrentDir());
  string ssh(conf.ssh?conf.ssh:"");
  string cache_path(conf.cache_path?conf.cache_path:
		    GetCurrentDir() + "/.cache/");

  fs = std::make_unique<directory_container::DirectoryContainer>();
  auto git = gitlstree::GitTree::NewGitTree(path, revision, ssh, cache_path,
				   fs.get());
  if (!git.get()) {
    fprintf(stderr, "Loading directory %s failed\n", path.c_str());
    return 1;
  }

  int ret = fuse_main(args.argc, args.argv, &o, NULL);
  fuse_opt_free_args(&args);
  return ret;
}
