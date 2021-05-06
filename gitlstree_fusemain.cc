#define FUSE_USE_VERSION 35

#include <fuse.h>
#include <stddef.h>

#include <memory>

#include "get_current_dir.h"
#include "git_adapter.h"
#include "gitlstree.h"

using std::string;

namespace gitlstree {

static int fs_ioctl(const char *path, unsigned int cmd, void *arg,
                    struct fuse_file_info *fi, unsigned int flags, void *data) {
  if (flags & FUSE_IOCTL_COMPAT) return -ENOSYS;

  const auto fe = dynamic_cast<FileElement *>(
      reinterpret_cast<directory_container::File *>(fi->fh));
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
  char *ssh{nullptr};
  char *path{nullptr};
  char *revision{nullptr};
  char *cache_path{nullptr};
};

#define MYFS_OPT(t, p, v) \
  { t, offsetof(gitlstree_config, p), v }

static struct fuse_opt gitlstree_opts[] = {
    MYFS_OPT("--ssh=%s", ssh, 0), MYFS_OPT("--path=%s", path, 0),
    MYFS_OPT("--revision=%s", revision, 0),
    MYFS_OPT("--cache_path=%s", cache_path, 0), FUSE_OPT_END};

int main(int argc, char *argv[]) {
  struct fuse_operations o = git_adapter::GetFuseOperations();

#define DEFINE_HANDLER(n) o.n = &gitlstree::fs_##n
  DEFINE_HANDLER(ioctl);
#undef DEFINE_HANDLER

  fuse_args args = FUSE_ARGS_INIT(argc, argv);
  gitlstree_config conf{};
  fuse_opt_parse(&args, &conf, gitlstree_opts, nullptr);

  string revision(conf.revision ? conf.revision : "HEAD");
  string path(conf.path ? conf.path : GetCurrentDir());
  string ssh(conf.ssh ? conf.ssh : "");
  string cache_path(conf.cache_path ? conf.cache_path
                                    : GetCurrentDir() + "/.cache/");

  auto git = gitlstree::GitTree::NewGitTree(
      path, revision, ssh, cache_path, git_adapter::GetDirectoryContainer());
  if (!git.get()) {
    fprintf(stderr, "Loading directory %s failed\n", path.c_str());
    return 1;
  }

  int ret = fuse_main(args.argc, args.argv, &o, nullptr);
  fuse_opt_free_args(&args);
  return ret;
}
