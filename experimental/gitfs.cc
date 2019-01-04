/*
 * A prototype tool to mount git filesystem by parsing output of 'git
 * ls-tree -r REVISION'.

git-ls-tree output output contains lines like:
100644 blob f313668af32ea3447a594ae1e7d8ac9841fbae7b	sound/README

git ls-tree -l -r  will also add file size

100644 blob f32d521227f2912fdc570418405c7360e42e9c93      49	statistics/README

and that should be mostly enough to obtain information to create a
mount-able filesystem.

 */

#include <assert.h>
#include <iostream>
#include <memory>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>

#include "gitfs.h"
#include "gitxx.h"
#include "strutil.h"

using std::cout;
using std::endl;
using std::move;
using std::mutex;
using std::string;
using std::lock_guard;
using std::unique_lock;
using std::unique_ptr;
using std::unordered_map;

namespace gitfs {

GitTree::GitTree(const char* revision_ref, const string& gitdir)
  : repo_(gitdir), fullpath_to_files_(), root_() {
  root_.reset(new FileElement(S_IFDIR, GIT_OBJ_TREE,
			      "TODO", 0, nullptr));
  unique_ptr<gitxx::Object> o(repo_.GetRevision(revision_ref));
  unique_ptr<gitxx::Tree> t(o->GetTreeFromCommit());
  LoadDirectory(&(root_->files_), "", t.get());
  fullpath_to_files_[""] = root_.get();
}

void GitTree::LoadDirectory(FileElement::FileElementMap* files,
			    const string& subdir,
			    gitxx::Tree* tree) {
  tree->for_each_file([&](const string& name, int attribute,
			  git_otype file_type, const string& sha1,
			  int size, unique_ptr<gitxx::Object> object){
      FileElement* fe = new FileElement(attribute,
					file_type,
					sha1,
					size,
					move(object));
      (*files)[name].reset(fe);
      fullpath_to_files_[subdir + name] = fe;
    }, [&](const string& name, gitxx::Tree* subtree){
      // Subdirectory
      LoadDirectory(&(*files)[name]->files_, subdir + name + "/", subtree);
    });
}

// Convert from Git attributes to filesystem attributes.
int GitTree::Getattr(const string& fullpath, struct stat *stbuf) const {
  memset(stbuf, 0, sizeof(struct stat));
  FileElement* f = get(fullpath.substr(1));
  stbuf->st_uid = getuid();
  stbuf->st_gid = getgid();
  // stbuf->st_atim = stbuf->st_mtim = stbuf->st_ctim;
  if (f) {
    stbuf->st_nlink = 1;
    if (f->attribute_ == S_IFDIR) {
      static_assert(S_IFDIR == 040000, "dir stat attribute wrong.");
      assert(f->file_type_ == GIT_OBJ_TREE);
      // This is a directory.
      stbuf->st_mode = S_IFDIR | 0755;
      stbuf->st_nlink = 2;
    } else if (f->attribute_ == S_IFLNK) {
      // symbolic link.
      static_assert(S_IFLNK == 0120000, "symlink stat attribute wrong.");
      stbuf->st_mode = S_IFLNK | 0644;
    } else {
      stbuf->st_mode = S_IFREG | f->attribute_;
    }
    stbuf->st_size = f->size_;
    return 0;
  } else {
    return -ENOENT;
  }
}

FileElement::FileElement(int attribute, git_otype file_type,
			 const std::string& sha1, int size,
			 unique_ptr<gitxx::Object> object) :
  attribute_(attribute), file_type_(file_type),
  sha1_(sha1), size_(size), files_(), object_(move(object)) {}

#define TYPE(a) {GIT_OBJ_##a, #a}
static unordered_map<int, string> file_type_to_string_map {
  TYPE(COMMIT),
  TYPE(TREE),
  TYPE(BLOB),
};
#undef TYPE

static void dumpFiles(const FileElement::FileElementMap* files) {
  for (const auto& it: *files) {
    const auto& f = it.second;
    cout << it.first << ":\t" << f->attribute_ << " "
	 << file_type_to_string_map[(int)f->file_type_] << " "
	 << f->sha1_ << " "
	 << f->size_ << " " << it.first << endl;
    if (f->file_type_ == GIT_OBJ_TREE) {
      dumpFiles(&f->files_);
    }
  }
}

void GitTree::dump() const {
  dumpFiles(&root_->files_);
}

ssize_t FileElement::Read(char *target, size_t size, off_t offset) {
  {
    lock_guard<mutex> l(buf_mutex_);
    if (!buf_.get()) {
      buf_.reset(new string(object_->GetBlobContent()));
    }
  }
  if (offset < static_cast<off_t>(buf_->size())) {
    if (offset + size > buf_->size())
      size = buf_->size() - offset;
    memcpy(target, buf_->c_str() + offset, size);
  } else
    size = 0;
  return size;
}

}
