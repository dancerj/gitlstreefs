#define FUSE_USE_VERSION 35

#include <assert.h>
#include <fuse.h>
#include <stddef.h>

#include <iostream>
#include <memory>

#include "get_current_dir.h"
#include "git-githubfs.h"
#include "git_adapter.h"

using std::cerr;
using std::endl;
using std::string;
using std::unique_ptr;

struct githubfs_config {
  char* user{nullptr};
  char* project{nullptr};
  char* revision{nullptr};
  char* cache_path{nullptr};
};

#define MYFS_OPT(t, p, v) \
  { t, offsetof(githubfs_config, p), v }

static struct fuse_opt githubfs_opts[] = {
    MYFS_OPT("--user=%s", user, 0), MYFS_OPT("--project=%s", project, 0),
    MYFS_OPT("--revision=%s", revision, 0),
    MYFS_OPT("--cache_path=%s", cache_path, 0), FUSE_OPT_END};

int main(int argc, char* argv[]) {
  // Initialize fuse operations.
  struct fuse_operations o = git_adapter::GetFuseOperations();

  fuse_args args = FUSE_ARGS_INIT(argc, argv);
  githubfs_config conf{};
  fuse_opt_parse(&args, &conf, githubfs_opts, nullptr);
  if (!(conf.user && conf.project)) {
    cerr << argv[0]
         << " --user=USER --project=PROJECT MOUNTPOINT --revision=HEAD" << endl
         << " example: " << argv[0]
         << " --user=dancerj --project=gitlstreefs mountpoint/" << endl;
    return EXIT_FAILURE;
  }

  const string cache_path(conf.cache_path ? conf.cache_path
                                          : GetCurrentDir() + "/.cache/");

  const string github_api_prefix =
      string("https://api.github.com/repos/") + conf.user + "/" + conf.project;
  auto git_tree = std::make_unique<githubfs::GitTree>(
      conf.revision ? conf.revision : "HEAD", github_api_prefix.c_str(),
      git_adapter::GetDirectoryContainer(), cache_path);
  int ret = fuse_main(args.argc, args.argv, &o, nullptr);
  fuse_opt_free_args(&args);
  return ret;
}
