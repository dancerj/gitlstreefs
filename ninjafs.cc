/*
  Ninja file system.

  Lists the targets as accessible file.
 */
#define FUSE_USE_VERSION 26

#include "directory_container.h"
#include "get_current_dir.h"
#include "strutil.h"

#include <assert.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>

#include <boost/algorithm/string.hpp>
#include <iostream>
#include <memory>
#include <mutex>
#include <unordered_map>

using std::cout;
using std::endl;
using std::function;
using std::make_unique;
using std::mutex;
using std::string;
using std::unique_lock;
using std::unique_ptr;
using std::unordered_map;
using std::vector;

namespace ninjafs {

// Obtain the current directory when the binary started, we will
// become a daemon and chdir(/), so this is important.
static string original_cwd(GetCurrentDir());

// A mutex to guard multiple ninja executions.
mutex global_ninja_mutex;

// We will hold the build log in memory and provide the contents per
// request from user, via a /ninja.log node.
class NinjaLog : public directory_container::File {
public:
  NinjaLog() : log_(), mutex_() {}
  virtual ~NinjaLog() {}

  virtual int Getattr(struct stat *stbuf) override {
    unique_lock<mutex> l(mutex_);
    stbuf->st_mode = S_IFREG | 0555;
    stbuf->st_nlink = 1;
    stbuf->st_size = log_.size();
    return 0;
  }

  virtual int Open() override {
    return 0;
  }

  virtual int Release() override {
    return 0;
  }

  virtual ssize_t Read(char *target, size_t size, off_t offset) override {
    // Fill in the response
    unique_lock<mutex> l(mutex_);
    if (offset < static_cast<off_t>(log_.size())) {
      if (offset + size > log_.size())
	size = log_.size() - offset;
      memcpy(target, log_.c_str() + offset, size);
    } else
      size = 0;
    return size;
  }

  void UpdateLog(const string&& log) {
    unique_lock<mutex> l(mutex_);
    log_ = move(log);
  }

private:
  string log_;
  mutex mutex_{};
};

// TODO: this is a weak global pointer referencing to a unique_ptr.
NinjaLog* ninja_log = nullptr;

// Concrete class.
class NinjaTarget : public directory_container::File {
public:
  NinjaTarget(const string& original_target_name) :
    original_target_name_(original_target_name) {}
  virtual ~NinjaTarget() {}
  virtual int Getattr(struct stat *stbuf) override {
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

  virtual int Open() override {
    unique_lock<mutex> l(mutex_);
    if (!buf_.get()) {
      int exit_code;
      // Fill in the content if it didn't exist before.
      assert(ninja_log);  // Initialization should have set this value.
      unique_lock<mutex> l(global_ninja_mutex);
      ninja_log->UpdateLog(PopenAndReadOrDie2({"ninja", original_target_name_},
					      &cwd(), &exit_code));

      if (exit_code == 0) {
	// success.
	buf_.reset(new string);
	*buf_ = ReadFromFileOrDie(AT_FDCWD, cwd() + "/" + original_target_name_);
      } else {
	return -EIO;  // Return IO error on build failure.
      }
    } else {
      // TODO, do I need / want to ever invalidate the cache?
    }
    return 0;
  }

  virtual int Release() override {
    // TODO: do I ever need to clean up?
    return 0;
  }

  virtual ssize_t Read(char *target, size_t size, off_t offset) override {
    // Fill in the response
    if (!buf_.get()) {
      // If buffer is empty, give back error.
      return -EIO;
    }
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

unique_ptr<directory_container::DirectoryContainer> fs;

bool LoadDirectory() {
  // NinjaLog is globally referenced but make it owned by filesystem.
  ninja_log = new NinjaLog();
  fs->add("/ninja.log", unique_ptr<NinjaLog>(ninja_log));

  int status;
  string ninja_targets = PopenAndReadOrDie2({"ninja", "-t", "targets", "all"},
					    nullptr, 
					    &status);
  if (status) {
    return false;
  }
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
  return true;
}

static int fs_getattr(const char *path, struct stat *stbuf) {
  return fs->Getattr(path, stbuf);
}

static int fs_opendir(const char* path, struct fuse_file_info* fi) {
  if (*path == 0)
    return -ENOENT;
  const auto d = dynamic_cast<
    directory_container::Directory*>(fs->mutable_get(path));
  fi->fh = reinterpret_cast<uint64_t>(d);
  return 0;
}

static int fs_releasedir(const char*, struct fuse_file_info* fi) {
  if (fi->fh == 0)
    return -EBADF;
  return 0;
}

static int fs_readdir(const char *unused, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi) {
  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);
  const directory_container::Directory* d = reinterpret_cast<
    directory_container::Directory*>(fi->fh);
  if (!d) return -ENOENT;
  d->for_each([&](const string& s, const directory_container::File* unused){
      filler(buf, s.c_str(), NULL, 0);
    });
  return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi) {
  if (*path == 0)
    return -ENOENT;
  directory_container::File* f = fs->mutable_get(path);
  if (!f)
    return -ENOENT;
  fi->fh = reinterpret_cast<uint64_t>(f);

  return f->Open();
}

static int fs_read(const char *path, char *target, size_t size, off_t offset,
		   struct fuse_file_info *fi) {
  auto f = reinterpret_cast<directory_container::File*>(fi->fh);
  if (!f)
    return -ENOENT;

  return f->Read(target, size, offset);
}

} // anonymous namespace

using namespace ninjafs;

int main(int argc, char *argv[]) {
  fs.reset(new directory_container::DirectoryContainer());
  if (!LoadDirectory()) {
    cout << "Failed to run ninja to obtain target." << endl;
    return 1;
  }

  struct fuse_operations o = {};
#define DEFINE_HANDLER(n) o.n = &fs_##n
  DEFINE_HANDLER(getattr);
  DEFINE_HANDLER(open);
  DEFINE_HANDLER(opendir);
  DEFINE_HANDLER(read);
  DEFINE_HANDLER(readdir);
  DEFINE_HANDLER(releasedir);
#undef DEFINE_HANDLER
  o.flag_nopath = true;

  return fuse_main(argc, argv, &o, NULL);
}
