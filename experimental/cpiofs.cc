/*
  cpio file system.

  Allows you to mount cpio files.

Example:

 sudo ../out/experimental/cpiofs ~/mnt/ \
   --underlying_file=$(readlink -f ~/tmp/initrd.img.gunzip )

*/
#define FUSE_USE_VERSION 32

#include "directory_container.h"
#include "get_current_dir.h"
#include "strutil.h"

#include <assert.h>
#include <fuse.h>
#include <sys/mman.h>

#include <iostream>

using std::cout;
using std::endl;
using std::function;
using std::make_unique;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::vector;

namespace {

// Cpio header contains numbers in hex in 8 byte ASCII format.
struct Number {
 public:
  long get() const noexcept {
    std::string s(data_, 8);
    return strtol(s.c_str(), nullptr, 16);
  }

 private:
  // I only expect to be instantiated from mmap.
  Number() = delete;
  ~Number() = delete;
  char data_[8];
};

struct CpioHeader {
 private:
  // Why would you ever want to have a 4 byte aligned data structure with 2+4
  // magic header??!?!?
  char magic[6];

 public:
  Number ino;
  Number mode;
  Number uid;
  Number gid;
  Number nlink;
  Number mtime;
  Number filesize;
  Number major;
  Number minor;
  Number rmajor;
  Number rminor;
  Number namesize;
  Number chksum;

 private:
  char end[];

 public:
  bool CheckHeader() const {
    const char golden[] = "070701";

    for (int i = 0; i < 6; ++i) {
      if (magic[i] != golden[i]) return false;
    }
    return true;
  }

  std::string_view FileName() const {
    return std::string_view(end, namesize.get() - 1 /* terminating 0 */);
  }
  int ContentsOffset() const {
    int n = namesize.get();

    // Padding needs to be like:
    // 0 -> 0
    // 1 -> 3
    // 2 -> 2
    // 3 -> 1

    int padding = (4 - ((n + 2) % 4)) % 4;
    return n + padding;
  }

  std::string_view Contents() const {
    return std::string_view(end + ContentsOffset(), filesize.get());
  }

  int EndOffset() const {
    int n = ContentsOffset() + filesize.get();
    int padding = (4 - ((n + 2) % 4)) % 4;
    return n + padding;
  }

  const CpioHeader *Next() const {
    return reinterpret_cast<const CpioHeader *>(end + EndOffset());
  }

 private:
  CpioHeader() = delete;
  ~CpioHeader() = delete;
};

}  // namespace

namespace cpiofs {

class CpioFile : public directory_container::File {
 public:
  CpioFile(const CpioHeader *c) : c_(*c) {}
  virtual ~CpioFile() {}

  virtual int Getattr(struct stat *stbuf) override {
    stbuf->st_mode = c_.mode.get();
    stbuf->st_nlink = 1;
    stbuf->st_size = c_.filesize.get();
    return 0;
  }

  virtual int Open() override { return 0; }

  virtual int Release() override {
    // TODO: do I ever need to clean up?
    return 0;
  }

  virtual ssize_t Read(char *target, size_t size, off_t offset) override {
    const auto &contents = c_.Contents();
    if (offset < static_cast<off_t>(contents.size())) {
      if (offset + size > contents.size()) size = contents.size() - offset;
      contents.copy(target, size, offset);
    } else
      size = 0;
    return size;
  }

  virtual ssize_t Readlink(char *target, size_t size) override {
    const auto &contents = c_.Contents();

    if (size > contents.size()) {
      size = contents.size();
      target[size] = 0;
    } else {
      // TODO: is this an error condition that we can't fit in the final 0 ?
    }
    contents.copy(target, size);
    return 0;
  }

 private:
  const CpioHeader &c_;
};

unique_ptr<directory_container::DirectoryContainer> fs;

bool LoadDirectory(const char *cpio_file) {
  assert(cpio_file != nullptr);
  int fd = open(cpio_file, O_RDONLY);
  assert(fd != -1);
  struct stat st;
  assert(-1 != fstat(fd, &st));

  void *m = mmap(nullptr, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if (m == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }
  assert(m != MAP_FAILED);

  const CpioHeader *c = reinterpret_cast<CpioHeader *>(m);
  while (c->CheckHeader()) {
    if (c->FileName() == "TRAILER!!!") {
      // TODO handle concatenated files
      std::cout << "Found trailer" << std::endl;
      return true;
    }
    std::cout << "filename: " << c->FileName()
              << " size: " << c->Contents().size()
              << " mode: " << ((c->mode.get() & S_IFREG) ? true : false)
              << std::endl;
    switch (c->mode.get() & S_IFMT) {
      case S_IFREG:
      case S_IFLNK:
        // TODO support more file types.
        std::string filename(std::string("/") + std::string(c->FileName()));
        fs->add(filename, make_unique<CpioFile>(c));
    }
    c = c->Next();
  }

  return false;
}

static int fs_getattr(const char *path, struct stat *stbuf,
                      fuse_file_info *fi) {
  // First request for .Trash after mount from GNOME gets called with
  // path and fi == null.
  if (fi) {
    auto f = reinterpret_cast<directory_container::File *>(fi->fh);
    assert(f != nullptr);
    return f->Getattr(stbuf);
  } else {
    return fs->Getattr(path, stbuf);
  }
}

static int fs_opendir(const char *path, struct fuse_file_info *fi) {
  if (*path == 0) return -ENOENT;
  const auto d =
      dynamic_cast<directory_container::Directory *>(fs->mutable_get(path));
  fi->fh = reinterpret_cast<uint64_t>(d);
  return 0;
}

static int fs_releasedir(const char *, struct fuse_file_info *fi) {
  if (fi->fh == 0) return -EBADF;
  return 0;
}

static int fs_readdir(const char *, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      fuse_readdir_flags) {
  filler(buf, ".", nullptr, 0, fuse_fill_dir_flags{});
  filler(buf, "..", nullptr, 0, fuse_fill_dir_flags{});
  const directory_container::Directory *d =
      reinterpret_cast<directory_container::Directory *>(fi->fh);
  if (!d) return -ENOENT;
  d->for_each([&](const string &s, const directory_container::File *unused) {
    filler(buf, s.c_str(), nullptr, 0, fuse_fill_dir_flags{});
  });
  return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi) {
  if (*path == 0) return -ENOENT;
  directory_container::File *f = fs->mutable_get(path);
  if (!f) return -ENOENT;
  fi->fh = reinterpret_cast<uint64_t>(f);

  return f->Open();
}

static int fs_read(const char *, char *target, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
  auto f = reinterpret_cast<directory_container::File *>(fi->fh);
  if (!f) return -ENOENT;

  return f->Read(target, size, offset);
}

static int fs_readlink(const char *path, char *buf, size_t size) {
  if (*path == 0) return -ENOENT;
  directory_container::File *f = fs->mutable_get(path);
  if (!f) return -ENOENT;

  return f->Readlink(buf, size);
}

void *fs_init(fuse_conn_info *, fuse_config *config) {
  config->nullpath_ok = 1;
  return nullptr;
}

}  // namespace cpiofs

using namespace cpiofs;

struct cpiofs_config {
  char *underlying_file{nullptr};
};

#define MYFS_OPT(t, p, v) \
  { t, offsetof(cpiofs_config, p), v }

static struct fuse_opt cpiofs_opts[] = {
    MYFS_OPT("--underlying_file=%s", underlying_file, 0), FUSE_OPT_END};
#undef MYFS_OPT

int main(int argc, char *argv[]) {
  struct fuse_operations o = {};
#define DEFINE_HANDLER(n) o.n = &fs_##n
  DEFINE_HANDLER(getattr);
  DEFINE_HANDLER(init);
  DEFINE_HANDLER(open);
  DEFINE_HANDLER(opendir);
  DEFINE_HANDLER(read);
  // NOTE: read_buf shouldn't be implemented because using FD isn't useful.
  DEFINE_HANDLER(readdir);
  DEFINE_HANDLER(readlink);
  DEFINE_HANDLER(releasedir);
#undef DEFINE_HANDLER

  fuse_args args = FUSE_ARGS_INIT(argc, argv);
  cpiofs_config conf{};
  fuse_opt_parse(&args, &conf, cpiofs_opts, nullptr);

  if (conf.underlying_file == nullptr) {
    fprintf(stderr,
            "Usage: %s [mountpoint] "
            "--underlying_file=file\n",
            argv[0]);
    return EXIT_FAILURE;
  }

  fs.reset(new directory_container::DirectoryContainer());
  if (!LoadDirectory(conf.underlying_file)) {
    cout << "Failed to load cpio file." << endl;
    return EXIT_FAILURE;
  }

  int ret = fuse_main(args.argc, args.argv, &o, nullptr);
  fuse_opt_free_args(&args);
  return ret;
}
