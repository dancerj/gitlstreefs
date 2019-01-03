/*
 * A prototype tool to mount git filesystem by parsing output of 'git
 * ls-tree -l -r REVISION'.

`git-ls-tree -l -r` output contains lines like:
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
#include <unordered_map>
#include <vector>

#include "concurrency_limit.h"
#include "gitlstree.h"
#include "strutil.h"
#include "scoped_timer.h"

using std::make_unique;
using std::mutex;
using std::string;
using std::unique_lock;
using std::unique_ptr;
using std::unordered_map;
using std::vector;

namespace gitlstree {

int FileElement::Getattr(struct stat *stbuf) {
  stbuf->st_uid = getuid();
  stbuf->st_gid = getgid();
  if (attribute_ == S_IFLNK) {
    // symbolic link.
    static_assert(S_IFLNK == 0120000, "symlink stat attribute wrong.");
    stbuf->st_mode = S_IFLNK | 0644;
  } else {
    stbuf->st_mode = S_IFREG | attribute_;
  }
  stbuf->st_size = size_;
  stbuf->st_nlink = 1;
  return 0;
}

// Maybe run remote command if ssh spec is available.
string GitTree::RunGitCommand(const vector<string>& commands, int* exit_code, 
			      const std::string& log_tag) {
  scoped_timer::ScopedTimer time(log_tag);
  if (!ssh_.empty()) {
    string command;
    for (const auto& s: commands) {
      command += s + " ";
    }
    ScopedConcurrencyLimit l(command);
    return PopenAndReadOrDie2({"ssh", ssh_,
	  string("cd ") + gitdir_ + " && "
	  + command}, nullptr, exit_code);
  } else {
    return PopenAndReadOrDie2(commands, &gitdir_, exit_code);
  }
}

bool GitTree::LoadDirectory(const string& hash, directory_container::DirectoryContainer* container) {
  int exit_code;
  string git_ls_tree(RunGitCommand({"git", "ls-tree", "-l", "-r", hash},
				   &exit_code, "lstree"));
  if (exit_code != 0) {
    // Failed to load directory.
    return false;
  }
  vector<string> lines;

  boost::algorithm::split(lines, git_ls_tree,
			  boost::is_any_of("\n"));
  for (const auto& line : lines)  {
    vector<string> elements;
    boost::algorithm::split(elements, line,
			    boost::is_any_of(" \t"),
			    boost::algorithm::token_compress_on);
    if (elements.size() == 5) {
      const string& file_path = elements[4];
      assert(file_path[0] != '/');  // git ls-tree do not start with /.
      container->add(string("/") + file_path,
		     make_unique<FileElement>(strtol(elements[0].c_str(), NULL, 8),
					      elements[2],
					      atoi(elements[3].c_str()),
					      this));
    }
  }
  container->add("/.status", make_unique<scoped_timer::StatusHandler>());
  return true;
}

/* static */
std::unique_ptr<GitTree> GitTree::NewGitTree(const string& my_gitdir,
					     const string& hash,
					     const string& maybe_ssh,
					     const string& cached_dir,
					     directory_container::DirectoryContainer* container) {
  unique_ptr<GitTree> g {new GitTree(my_gitdir, maybe_ssh, cached_dir)};
  if (g->LoadDirectory(hash, container)) {
    return g;
  } else {
    return nullptr;
  }
}


GitTree::GitTree(const string& my_gitdir,
		 const string& maybe_ssh, const string& cached_dir)
    : gitdir_(my_gitdir), ssh_(maybe_ssh), cache_(cached_dir) {
  cache_.Gc();
}

GitTree::~GitTree() {}

FileElement::FileElement(int attribute, const string& sha1, int size, GitTree* parent) :
  attribute_(attribute), sha1_(sha1), size_(size), parent_(parent) {}

int FileElement::maybe_cat_file_locked() {
  if (!memory_) {
    memory_ = parent_->cache().get(sha1_, [this](string* ret) -> bool {
	int exit_code;
	*ret = string(parent_->RunGitCommand({"git", "cat-file", "blob", sha1_},
					     &exit_code,
					     "cat-file"));
	return exit_code == 0;
      });
    if (!memory_) {
      // If still failed, something failed in the process.
      return -EIO;
    }
  }
  return 0;
}

int FileElement::Open() {
  unique_lock<mutex> l(buf_mutex_);
  return maybe_cat_file_locked();
}

ssize_t FileElement::Read(char *target, size_t size, off_t offset) {
  unique_lock<mutex> l(buf_mutex_);
  if (offset < static_cast<off_t>(memory_->size())) {
    if (offset + size > memory_->size())
      size = memory_->size() - offset;
    memcpy(target, static_cast<const char*>(memory_->memory()) + offset, size);
  } else
    size = 0;
  return size;
}

ssize_t FileElement::Readlink(char *target, size_t size) {
  unique_lock<mutex> l(buf_mutex_);
  int e = maybe_cat_file_locked();
  if (e != 0)
    return e;

  if (size > memory_->size()) {
    size = memory_->size();
    target[size] = 0;
  } else {
    // TODO: is this an error condition that we can't fit in the final 0 ?
  }

  memcpy(target, static_cast<const char*>(memory_->memory()), size);
  return 0;
}

void FileElement::GetHash(char* hash) const {
  memcpy(hash, sha1_.data(), 40);
}

int FileElement::Release() {
  unique_lock<mutex> l(buf_mutex_);
  parent_->cache().release(sha1_, memory_);
  memory_ = nullptr;
  return 0;
}

}
