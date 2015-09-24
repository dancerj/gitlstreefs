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

using std::cout;
using std::endl;
using std::function;
using std::mutex;
using std::string;
using std::unique_lock;
using std::unique_ptr;
using std::unordered_map;
using std::vector;

namespace ninjafs {

class NinjaFs {
public:
  NinjaFs(): cwd_(GetCurrentDir()) {
    LoadDirectory();
  }

  class File {
  public:
    File(const string& original_target_name) :
      original_target_name_(original_target_name) {}
    // If file was created already, this is not null.
    unique_ptr<string> buf_{};
    mutex mutex_{};
    string original_target_name_{};
  };

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
	const string& filename = elements[0];
	string escaped_filename(filename);
	for (size_t pos = 0; 
	     (pos = escaped_filename.find('/', pos)) != std::string::npos; ) {
	  escaped_filename[pos] = '_';
	}
	files_[escaped_filename].reset(new File(filename));
      }
    }
  }

  const string& cwd() const { return cwd_; }
private:
  const string cwd_;
  unordered_map<string, unique_ptr<File> > files_{};
};

unique_ptr<NinjaFs> fs;

static int fs_getattr(const char *path, struct stat *stbuf) {
  memset(stbuf, 0, sizeof(struct stat));
  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
    return 0;
  }
  if (*path == 0)
    return -ENOENT;

  NinjaFs::File* f = fs->get(path + 1);
  if (!f)
    return -ENOENT;

  stbuf->st_mode = S_IFREG | 0555;
  stbuf->st_nlink = 1;
  unique_lock<mutex> l(f->mutex_);
  if (f->buf_.get()) {
    stbuf->st_size = f->buf_->size();
  } else {
    // Hmm... fooling tools does not really work with the size.
    stbuf->st_size = 4096 * 1024;
  }
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
  NinjaFs::File* f = fs->get(path + 1);
  if (!f)
    return -ENOENT;
  fi->fh = reinterpret_cast<uint64_t>(f);

  unique_lock<mutex> l(f->mutex_);
  if (!f->buf_.get()) {
    // Fill in the content if it didn't exist before.
    string result = PopenAndReadOrDie(string("cd ") +
				      fs->cwd() + " && ninja " +
				      f->original_target_name_);
    f->buf_.reset(new string);
    *f->buf_ = ReadFromFileOrDie(fs->cwd() + "/" + f->original_target_name_);
  }
  return 0;
}

static int fs_read(const char *path, char *target, size_t size, off_t offset,
		   struct fuse_file_info *fi) {
  NinjaFs::File* f = reinterpret_cast<NinjaFs::File*>(fi->fh);
  if (!f)
    return -ENOENT;

  // Fill in the response
  unique_lock<mutex> l(f->mutex_);
  if (offset < static_cast<off_t>(f->buf_->size())) {
    if (offset + size > f->buf_->size())
      size = f->buf_->size() - offset;
    memcpy(target, f->buf_->c_str() + offset, size);
  } else
    size = 0;
  return size;
}

} // anonymous namespace

using namespace ninjafs;

int main(int argc, char *argv[]) {
  fs.reset(new NinjaFs());

  struct fuse_operations o = {};
  o.getattr = &fs_getattr;
  o.readdir = &fs_readdir;
  o.open = &fs_open;
  o.read = &fs_read;
  return fuse_main(argc, argv, &o, NULL);
}
