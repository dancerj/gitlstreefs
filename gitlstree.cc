/*
 * A prototype tool to mount git filesystem by parsing output of 'git
 * ls-tree -r REVISION'.

git-ls-tree -l output output contains lines like:
100644 blob f313668af32ea3447a594ae1e7d8ac9841fbae7b	sound/README

and that should be mostly enough to obtain information to create a
mount-able filesystem.

 */

#include <assert.h>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <memory>
#include <stdio.h>
#include <sys/stat.h>
#include <thread>
#include <unordered_map>
#include <vector>

#include "basename.h"
#include "gitlstree.h"
#include "strutil.h"
#include "concurrency_limit.h"

using std::cout;
using std::endl;
using std::mutex;
using std::string;
using std::thread;
using std::unique_lock;
using std::unique_ptr;
using std::unordered_map;
using std::vector;

namespace gitlstree {

GitTree::GitTree(const char* hash, const char* ssh, const string& gitdir)
  : gitdir_(gitdir), hash_(hash), ssh_(ssh?ssh:""),
    fullpath_to_files_(), root_() {
  root_.reset(new FileElement(this, S_IFDIR, TYPE_tree,
			      "TODO", 0));
  LoadDirectory(&(root_->files_), "");
  fullpath_to_files_[""] = root_.get();
}

// Maybe run remote command if ssh spec is available.
string GitTree::RunGitCommand(const string& command) const {
  if (!ssh_.empty()) {
    ScopedConcurrencyLimit l(command);
    return PopenAndReadOrDie(string("ssh ") + ssh_ + " 'cd " + gitdir_ + " && "
			     + command + "'");
  } else {
    return PopenAndReadOrDie("cd " + gitdir_ + " && "
			     + command);
  }
}

void GitTree::LoadDirectory(FileElement::FileElementMap* files,
			    const string& subdir) {
  string git_ls_tree = RunGitCommand(string("git ls-tree -l ") +
				     hash_ + " " + subdir);
  cout << "Loaded directory " << subdir << endl;
  vector<string> lines;
  vector<thread> jobs;

  boost::algorithm::split(lines, git_ls_tree,
			  boost::is_any_of("\n"));
  for (const auto& line : lines)  {
    vector<string> elements;
    boost::algorithm::split(elements, line,
			    boost::is_any_of(" \t"),
			    boost::algorithm::token_compress_on);
    if (elements.size() == 5) {
      GitFileType fstype = FileTypeStringToFileType(elements[1]);
      const string& file_path = elements[4];
      string basename = BaseName(file_path);
      FileElement* fe = new FileElement(this, 
					strtol(elements[0].c_str(), NULL, 8),
					fstype,
					elements[2],
					atoi(elements[3].c_str()));
      {
	unique_lock<mutex> l(path_mutex_);
	(*files)[basename].reset(fe);
	fullpath_to_files_[file_path] = fe;
      }
      if (fstype == TYPE_tree) {
	string subdir = elements[4] + "/";
	jobs.emplace_back(thread([this, fe, subdir](){
	      LoadDirectory(&fe->files_, subdir);
	    }));
      }
    }
  }
  for (auto& job : jobs) { job.join(); }
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
      assert(f->file_type_ == TYPE_tree);
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

FileElement::FileElement(GitTree* parent, int attribute, GitFileType file_type,
			 const std::string& sha1, int size) :
  attribute_(attribute), file_type_(file_type),
  sha1_(sha1), size_(size), files_(), parent_(parent) {}

static void dumpFiles(const FileElement::FileElementMap* files) {
  for (const auto& it: *files) {
    const auto& f = it.second;
    cout << it.first << ":\t" << f->attribute_ << " " << f->file_type_ << " "
	 << f->sha1_ << " "
	 << f->size_ << " " << it.first << endl;
    if (f->file_type_ == TYPE_tree) {
      dumpFiles(&f->files_);
    }
  }
}

void GitTree::dump() const {
  dumpFiles(&root_->files_);
}

#define TYPE(a) {#a, TYPE_##a}
static unordered_map<string, GitFileType> file_type_map {
  TYPE(blob),
  TYPE(tree),
  TYPE(commit)
};
#undef TYPE

GitFileType FileTypeStringToFileType(const string& file_type_string) {
  return file_type_map.find(file_type_string)->second;
}

ssize_t FileElement::Read(char *target, size_t size, off_t offset) {
  {
    unique_lock<mutex> l(buf_mutex_);
    if (!buf_.get()) {
      buf_.reset(new string(parent_->RunGitCommand("git cat-file blob " + sha1_)));
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
