/*
  Excercise to handle additional flags in libfuse.
 */
#define FUSE_USE_VERSION 26

#include <assert.h>
#include <fuse.h>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdio.h>
#include <string.h>
#include <unordered_map>
#include <stddef.h>

#include "get_current_dir.h"
#include "strutil.h"

using std::cout;
using std::endl;
using std::function;
using std::mutex;
using std::string;
using std::unique_lock;
using std::unique_ptr;
using std::unordered_map;

namespace flag_fs {

class FlagFs {
public:
  class File {
  public:
    File(string buf) :
      buf_(buf) {}
    // If file was created already, this is not null.
    string buf_;
  };

  FlagFs(const char* filename, const char* buffer) {
    files_[filename].reset(new File(buffer));
  }

  void for_each_filename(function<void(const string& s)> f) {
    for (const auto& it: files_) {
      f(it.first);
    }
  }

  File* get(const string& name) {
    const auto& it = files_.find(name);
    if (it != files_.end()) {
      return it->second.get();
    }
    return nullptr;
  }

private:
  unordered_map<string, unique_ptr<File> > files_{};
};

unique_ptr<FlagFs> fs;

static int fs_getattr(const char *path, struct stat *stbuf) {
  memset(stbuf, 0, sizeof(struct stat));
  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
    return 0;
  }
  if (*path == 0)
    return -ENOENT;

  FlagFs::File* f = fs->get(path + 1);
  if (!f)
    return -ENOENT;

  stbuf->st_mode = S_IFREG | 0555;
  stbuf->st_nlink = 1;
  stbuf->st_size = f->buf_.size();
  return 0;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi) {
  if (strcmp(path, "/") != 0)
    return -ENOENT;

  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);
  fs->for_each_filename([&](const string& s){
      filler(buf, s.c_str(), NULL, 0);
    });

  return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi) {
  if (*path == 0)
    return -ENOENT;
  FlagFs::File* f = fs->get(path + 1);
  if (!f)
    return -ENOENT;
  fi->fh = reinterpret_cast<uint64_t>(f);
  return 0;
}

static int fs_read(const char *path, char *target, size_t size, off_t offset,
		   struct fuse_file_info *fi) {
  FlagFs::File* f = reinterpret_cast<FlagFs::File*>(fi->fh);
  if (!f)
    return -ENOENT;

  // Fill in the response
  if (offset < static_cast<off_t>(f->buf_.size())) {
    if (offset + size > f->buf_.size())
      size = f->buf_.size() - offset;
    memcpy(target, f->buf_.c_str() + offset, size);
  } else
    size = 0;
  return size;
}

} // anonymous namespace

using namespace flag_fs;

struct hellofs_config {
  char* filename{nullptr};
  char* content{nullptr};
};

#define MYFS_OPT(t, p, v) { t, offsetof(hellofs_config, p), v }

static struct fuse_opt hellofs_opts[] = {
  MYFS_OPT("--filename=%s", filename, 0),
  MYFS_OPT("--content=%s", content, 0),
  FUSE_OPT_END
};

int main(int argc, char *argv[]) {

  fuse_operations o{};
  o.getattr = &fs_getattr;
  o.readdir = &fs_readdir;
  o.open = &fs_open;
  o.read = &fs_read;

  fuse_args args = FUSE_ARGS_INIT(argc, argv);
  hellofs_config conf{};
  fuse_opt_parse(&args, &conf, hellofs_opts, NULL);
  printf("filename:%s and content:%s\n", conf.filename, conf.content);
  fs.reset(new FlagFs(conf.filename, conf.content));
  int ret = fuse_main(args.argc, args.argv, &o, NULL);
  fuse_opt_free_args(&args);
  return ret;
}
