// A file system that adds a file 'unko' on all directories containing 'unkounko'
//
// Proof of concept that you can overlay mountpoint over existing
// directory and still read it.
//
// unkofs mountpoint -o nonempty
#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <string.h>

#include <string>
#include <memory>

#include "../relative_path.h"
#include "../update_rlimit.h"
#include "roptfs.h"

using std::string;
using std::unique_ptr;

class UnkoFileHandle : public roptfs::FileHandle {
public:
  UnkoFileHandle() :
    FileHandle(-1) {}
  virtual ~UnkoFileHandle() {}
private:
  DISALLOW_COPY_AND_ASSIGN(UnkoFileHandle);
};

class UnkoFsHandler : public roptfs::RoptfsHandler {
public:
  UnkoFsHandler() {}
  virtual ~UnkoFsHandler() {}

  ssize_t Read(const roptfs::FileHandle& fh, char* target,
	       size_t size, off_t offset) override {
    if (dynamic_cast<const UnkoFileHandle*>(&fh) != nullptr) {
      if (size <= 8) { return -EIO; }
      memcpy(target, "unkounko", 8);
      return 8;
    }
    return RoptfsHandler::Read(fh, target, size, offset);
  }

  int Open(const std::string& relative_path, unique_ptr<roptfs::FileHandle>* fh) override {
    if (relative_path == "unko") {
      fh->reset(new UnkoFileHandle);
      return 0;
    }
    return RoptfsHandler::Open(relative_path, fh);
  }

  int ReadDir(const std::string& relative_path,
	      void *buf, fuse_fill_dir_t filler,
	      off_t offset) override {
    filler(buf, "unko", nullptr, 0);
    return RoptfsHandler::ReadDir(relative_path, buf, filler, offset);
  }


  int GetAttr(const std::string& relative_path, struct stat* stbuf) override {
    if (relative_path == "unko") {
      stbuf->st_size = 8;
      stbuf->st_mode = S_IFREG | 0444;
      stbuf->st_uid = getuid();
      stbuf->st_gid = getgid();
      stbuf->st_nlink = 1;
      return 0;
    }
    return RoptfsHandler::GetAttr(relative_path, stbuf);
  }

private:
  DISALLOW_COPY_AND_ASSIGN(UnkoFsHandler);
};

int main(int argc, char** argv) {
  UpdateRlimit();

  const char* mountpoint = argv[1];
  assert(argc > 1);
  roptfs::RoptfsHandler::premount_dirfd_ = open(mountpoint, O_DIRECTORY);

  struct fuse_operations o = {};
  roptfs::FillFuseOperations<UnkoFsHandler>(&o);

  return fuse_main(argc, argv, &o, nullptr);
}

