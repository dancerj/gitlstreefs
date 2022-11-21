/*
  Excercise to handle additional flags in libfuse.
 */
#define FUSE_USE_VERSION 35

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include <fuse.h>

using std::function;
using std::string;
using std::unique_ptr;
using std::unordered_map;

namespace flag_fs {

class FlagFs {
 public:
  class File {
   public:
    File(string buf) : buf_(buf) {}
    // If file was created already, this is not null.
    string buf_;
  };

  FlagFs(const char* filename, const char* buffer) {
    files_[filename].reset(new File(buffer));
  }

  void for_each_filename(function<void(const string& s)> f) {
    for (const auto& it : files_) {
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

static int fs_getattr(const char* path, struct stat* stbuf, fuse_file_info*) {
  memset(stbuf, 0, sizeof(struct stat));
  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
    return 0;
  }
  if (*path == 0) return -ENOENT;

  FlagFs::File* f = fs->get(path + 1);
  if (!f) return -ENOENT;

  stbuf->st_mode = S_IFREG | 0555;
  stbuf->st_nlink = 1;
  stbuf->st_size = f->buf_.size();
  return 0;
}

static int fs_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info* fi,
                      fuse_readdir_flags) {
  if (strcmp(path, "/") != 0) return -ENOENT;

  filler(buf, ".", nullptr, 0, fuse_fill_dir_flags{});
  filler(buf, "..", nullptr, 0, fuse_fill_dir_flags{});
  fs->for_each_filename([&](const string& s) {
    filler(buf, s.c_str(), nullptr, 0, fuse_fill_dir_flags{});
  });

  return 0;
}

static int fs_open(const char* path, struct fuse_file_info* fi) {
  if (*path == 0) return -ENOENT;
  FlagFs::File* f = fs->get(path + 1);
  if (!f) return -ENOENT;
  fi->fh = reinterpret_cast<uint64_t>(f);
  return 0;
}

static int fs_read(const char* path, char* target, size_t size, off_t offset,
                   struct fuse_file_info* fi) {
  FlagFs::File* f = reinterpret_cast<FlagFs::File*>(fi->fh);
  if (!f) return -ENOENT;

  // Fill in the response
  if (offset < static_cast<off_t>(f->buf_.size())) {
    if (offset + size > f->buf_.size()) size = f->buf_.size() - offset;
    memcpy(target, f->buf_.c_str() + offset, size);
  } else
    size = 0;
  return size;
}

}  // namespace flag_fs

using namespace flag_fs;

struct hellofs_config {
  char* filename{nullptr};
  char* content{nullptr};
};

#define MYFS_OPT(t, p, v) \
  { t, offsetof(hellofs_config, p), v }

static struct fuse_opt hellofs_opts[] = {MYFS_OPT("--filename=%s", filename, 0),
                                         MYFS_OPT("--content=%s", content, 0),
                                         FUSE_OPT_END};

int main(int argc, char* argv[]) {
  fuse_operations o{};
  o.getattr = &fs_getattr;
  o.readdir = &fs_readdir;
  o.open = &fs_open;
  o.read = &fs_read;

  fuse_args args = FUSE_ARGS_INIT(argc, argv);
  hellofs_config conf{};
  fuse_opt_parse(&args, &conf, hellofs_opts, nullptr);
  printf("filename:%s and content:%s\n", conf.filename, conf.content);
  fs.reset(new FlagFs(conf.filename, conf.content));
  int ret = fuse_main(args.argc, args.argv, &o, nullptr);
  fuse_opt_free_args(&args);
  return ret;
}
