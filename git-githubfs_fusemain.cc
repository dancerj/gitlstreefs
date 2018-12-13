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

#include "get_current_dir.h"
#include "git-githubfs.h"

using std::string;
using std::unique_ptr;
using std::cerr;
using std::endl;

namespace {
// Global scope to make it accessible from callback.
unique_ptr<directory_container::DirectoryContainer> fs;
}

namespace githubfs {

static int fs_getattr(const char *path, struct stat *stbuf)
{
  return fs->Getattr(path, stbuf);
}

static int fs_opendir(const char* path, struct fuse_file_info* fi) {
   if (path == 0 || *path != '/') {
     return -ENOENT;
   }
  const directory_container::Directory* d = dynamic_cast<
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

static int fs_release(const char *path, struct fuse_file_info *fi) {
  FileElement* fe = dynamic_cast<FileElement*>(reinterpret_cast<directory_container::File*>(fi->fh));
  if (!fe) {
    return -ENOENT;
  }
  return fe->Release();
}
}  // namespace githubfs

struct githubfs_config {
  char* user{nullptr};
  char* project{nullptr};
  char* revision{nullptr};
  char* cache_path{nullptr};
};

#define MYFS_OPT(t, p, v) { t, offsetof(githubfs_config, p), v }

static struct fuse_opt githubfs_opts[] = {
  MYFS_OPT("--user=%s", user, 0),
  MYFS_OPT("--project=%s", project, 0),
  MYFS_OPT("--revision=%s", revision, 0),
  MYFS_OPT("--cache_path=%s", cache_path, 0),
  FUSE_OPT_END
};

int main(int argc, char *argv[]) {
  // Initialize fuse operations.
  struct fuse_operations o = {};
#define DEFINE_HANDLER(n) o.n = &githubfs::fs_##n
  DEFINE_HANDLER(getattr);
  DEFINE_HANDLER(open);
  DEFINE_HANDLER(opendir);
  DEFINE_HANDLER(releasedir);
  DEFINE_HANDLER(read);
  DEFINE_HANDLER(readdir);
  DEFINE_HANDLER(release);
#undef DEFINE_HANDLER
  o.flag_nopath = true;

  fuse_args args = FUSE_ARGS_INIT(argc, argv);
  githubfs_config conf{};
  fuse_opt_parse(&args, &conf, githubfs_opts, NULL);
  if (!(conf.user && conf.project)) {
    cerr << argv[0] << " --user=USER --project=PROJECT MOUNTPOINT --revision=HEAD" << endl
	 << " example: " << argv[0]
	 << " --user=dancerj --project=gitlstreefs mountpoint/" << endl;
    return 1;
  }

  string cache_path(conf.cache_path?conf.cache_path:
		    GetCurrentDir() + "/.cache/");

  string github_api_prefix = string("https://api.github.com/repos/") +
    conf.user + "/" + conf.project;
  fs = std::make_unique<directory_container::DirectoryContainer>();
  auto git_tree =
    std::make_unique<githubfs::GitTree>(conf.revision?conf.revision:"HEAD",
					github_api_prefix.c_str(), fs.get(),
					cache_path);
  int ret = fuse_main(args.argc, args.argv, &o, NULL);
  fuse_opt_free_args(&args);
  return ret;
}
