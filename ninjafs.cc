/*
  Ninja file system.

  Lists the targets as accessible file.
 */
#define FUSE_USE_VERSION 26

#include <assert.h>
#include <boost/algorithm/string.hpp>
#include <fuse.h>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdio.h>
#include <string.h>
#include <unordered_map>

#include "get_current_dir.h"
#include "strutil.h"
#include "directory_container.h"

using std::cout;
using std::endl;
using std::function;
using std::mutex;
using std::string;
using std::unique_lock;
using std::unique_ptr;
using std::unordered_map;
using std::vector;
using std::make_unique;
using std::unique_lock;
using std::mutex;

namespace ninjafs {

// Obtain the current directory when the binary started, we will
// become a daemon and chdir(/), so this is important.
static string original_cwd(GetCurrentDir());

// Concrete class.
class NinjaTarget : public directory_container::File {
public:
  NinjaTarget(const string& original_target_name) :
    original_target_name_(original_target_name){}
  virtual ~NinjaTarget() {}
  virtual int Getattr(struct stat *stbuf) {
    stbuf->st_mode = S_IFREG | 0555;
    stbuf->st_nlink = 1;
    unique_lock<mutex> l(mutex_);
    if (buf_.get()) {
      stbuf->st_size = buf_->size();
    } else {
      // Fool tools by giving a large enough estimate of the file size.
      stbuf->st_size = 4096 * 1024;
    }
    return 0;
  }

  const string& cwd() {
    // TODO;
    return original_cwd;
  }
  void Open() {
    unique_lock<mutex> l(mutex_);
    if (!buf_.get()) {
      // Fill in the content if it didn't exist before.
      string result = PopenAndReadOrDie(string("cd ") +
					cwd() + " && ninja " +
					original_target_name_);
      buf_.reset(new string);
      *buf_ = ReadFromFileOrDie(cwd() + "/" + original_target_name_);
    }
  }

  ssize_t Read(const char *path, char *target, size_t size, off_t offset) {
    // Fill in the response
    unique_lock<mutex> l(mutex_);
    if (offset < static_cast<off_t>(buf_->size())) {
      if (offset + size > buf_->size())
	size = buf_->size() - offset;
      memcpy(target, buf_->c_str() + offset, size);
    } else
      size = 0;
    return size;
  }
private:
  // If file was created already, this is not null.
  unique_ptr<string> buf_{};
  mutex mutex_{};

  // Target name to use with ninja.
  string original_target_name_{};
};

unique_ptr<directory_container::DirectoryContainer<NinjaTarget> > fs;

void LoadDirectory() {
  string ninja_targets = PopenAndReadOrDie("ninja -t targets all");
  vector<string> lines;
  boost::algorithm::split(lines, ninja_targets,
			  boost::is_any_of("\n"));
  for (const auto& line : lines)  {
    vector<string> elements;
    boost::algorithm::split(elements, line,
			    boost::is_any_of(":"),
			    boost::algorithm::token_compress_on);
    if (elements.size() == 2) {
      assert(elements[0][0] != '/'); // ninja targets do not start with /.
      const string& filename = elements[0];
      // For FUSE, path needs to start with /.
      fs->add(string("/") + filename, make_unique<NinjaTarget>(filename));
    }
  }
}

static int fs_getattr(const char *path, struct stat *stbuf) {
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

static int fs_open(const char *path, struct fuse_file_info *fi) {
  if (*path == 0)
    return -ENOENT;
  NinjaTarget* f = dynamic_cast<NinjaTarget*>(fs->mutable_get(path));
  if (!f)
    return -ENOENT;
  fi->fh = reinterpret_cast<uint64_t>(f);

  f->Open();
  return 0;
}

static int fs_read(const char *path, char *target, size_t size, off_t offset,
		   struct fuse_file_info *fi) {
  NinjaTarget* f = dynamic_cast<NinjaTarget*>(reinterpret_cast<directory_container::File*>(fi->fh));
  if (!f)
    return -ENOENT;

  return f->Read(path, target, size, offset);
}

} // anonymous namespace

using namespace ninjafs;

int main(int argc, char *argv[]) {
  fs.reset(new directory_container::DirectoryContainer<NinjaTarget>());
  LoadDirectory();
  fs->dump();

  struct fuse_operations o = {};
  o.getattr = &fs_getattr;
  o.readdir = &fs_readdir;
  o.open = &fs_open;
  o.read = &fs_read;
  return fuse_main(argc, argv, &o, NULL);
}
