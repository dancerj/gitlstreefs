/*
 * A prototype tool to mount git filesystem by parsing output of 'git
 * ls-tree -l -r REVISION'.

`git-ls-tree -l -r` output contains lines like:
100644 blob f313668af32ea3447a594ae1e7d8ac9841fbae7b	sound/README

and that should be mostly enough to obtain information to create a
mount-able filesystem.

 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unordered_map>

#include <iostream>
#include <memory>
#include <vector>

#include "concurrency_limit.h"
#include "git_cat_file.h"
#include "gitlstree.h"
#include "ostream_vector.h"
#include "scoped_timer.h"
#include "strutil.h"

using std::lock_guard;
using std::make_unique;
using std::mutex;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::vector;

namespace gitlstree {
namespace {
class GitHeadHandler : public directory_container::File {
public:
  GitHeadHandler(const std::string& rev, GitTree* parent) :
    rev_(rev + "\n"),  // Has a terminating newline.
    parent_(parent) {}
  virtual ~GitHeadHandler() {}

  virtual int Getattr(struct stat *stbuf) override {
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_mode = S_IFREG | S_IRUSR | S_IWUSR;
    stbuf->st_size = rev_.size();
    stbuf->st_nlink = 1;
    return 0;
  }
  virtual ssize_t Read(char *target, size_t size, off_t offset) override {
    if (offset != 0) {
      return -EIO;
    }
    if (size <= rev_.size()) {
      return -EIO;
    }
    memcpy(target, rev_.c_str(), rev_.size() + 1);
    return rev_.size() + 1;
  }
  virtual int Open() override { return 0; }
  virtual int Release() override { return 0; }
private:
  std::string rev_;
  GitTree* parent_;
  DISALLOW_COPY_AND_ASSIGN(GitHeadHandler);
};

} // anynomous namespace

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
  constexpr bool verbose = false;
  if (verbose) {
    std::cout << commands << std::endl;
  }
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

bool GitTree::LoadDirectory(const string& ref, directory_container::DirectoryContainer* container) {
  int exit_code;
  int exit_code_revparse;
  string hash{RunGitCommand({"git", "rev-parse", ref}, &exit_code_revparse, "rev-parse")};
  // truncate the final newline.
  hash.resize(hash.size() - 1);
  if (exit_code_revparse != 0) {
    std::cerr << "Could not resolve " << ref << std::endl;
    return false;
  }

  string git_ls_tree(RunGitCommand({"git", "ls-tree", "-l", "-r", hash},
				   &exit_code, "lstree"));
  if (exit_code != 0) {
    // Failed to load directory.
    return false;
  }
  const vector<string> lines = SplitStringUsing(git_ls_tree, '\n', true);
  for (const auto& line : lines)  {
    const vector<string> elements = SplitStringUsing(line, ' ', true);
    // TODO split with tab too.
    // TODO
    if (elements.size() == 4) {
      const vector<string> elements3 = SplitStringUsing(elements[3], '\t', false);
      assert(elements3.size() == 2);
      const string& file_path = elements3[1];
      const string& sha1 = elements[2];
      mode_t attribute = strtol(elements[0].c_str(), nullptr, 8);
      size_t size = atoi(elements3[0].c_str());
      assert(file_path[0] != '/');  // git ls-tree do not start with /.
      container->add(string("/") + file_path,
		     make_unique<FileElement>(attribute,
					      sha1,
					      size,
					      this));
    }
  }
  container->add("/.status", make_unique<scoped_timer::StatusHandler>());
  container->add("/.git/HEAD", make_unique<GitHeadHandler>(hash, this));
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
    : gitdir_(my_gitdir), ssh_(maybe_ssh), cache_(cached_dir),
      git_cat_file_(maybe_ssh.empty() ?
		    std::make_unique<GitCatFile::GitCatFileProcess>(&my_gitdir) :
		    std::make_unique<GitCatFile::GitCatFileProcess>(my_gitdir, ssh_)) {
  cache_.Gc();
}

GitTree::~GitTree() {}

FileElement::FileElement(int attribute, const string& sha1, int size, GitTree* parent) :
  attribute_(attribute), sha1_(sha1), size_(size), parent_(parent) {}

int FileElement::maybe_cat_file_locked() {
  if (!memory_) {
    memory_ = parent_->cache().get(sha1_, [this](string* ret) -> bool {
	try {
	  *ret = parent_->git_cat_file()->Request(sha1_);
	} catch (GitCatFile::GitCatFileProcess::ObjectNotFoundException& e) {
	  // If the object was not found, caching the result is not useful.
	  return false;
	}
	return true;
      });
    if (!memory_) {
      // If still failed, something failed in the process.
      return -EIO;
    }
  }
  return 0;
}

int FileElement::Open() {
  lock_guard<mutex> l(buf_mutex_);
  return maybe_cat_file_locked();
}

ssize_t FileElement::Read(char *target, size_t size, off_t offset) {
  lock_guard<mutex> l(buf_mutex_);
  if (offset < static_cast<off_t>(memory_->size())) {
    if (offset + size > memory_->size())
      size = memory_->size() - offset;
    memcpy(target, memory_->memory_charp() + offset, size);
  } else
    size = 0;
  return size;
}

ssize_t FileElement::Readlink(char *target, size_t size) {
  lock_guard<mutex> l(buf_mutex_);
  int e = maybe_cat_file_locked();
  if (e != 0)
    return e;

  if (size > memory_->size()) {
    size = memory_->size();
    target[size] = 0;
  } else {
    // TODO: is this an error condition that we can't fit in the final 0 ?
  }

  memcpy(target, memory_->memory_charp(), size);
  return 0;
}

void FileElement::GetHash(char* hash) const {
  memcpy(hash, sha1_.data(), 40);
}

int FileElement::Release() {
  lock_guard<mutex> l(buf_mutex_);
  parent_->cache().release(sha1_, memory_);
  memory_ = nullptr;
  return 0;
}

}
