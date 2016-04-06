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
#include <thread>
#include <unordered_map>
#include <vector>

#include "basename.h"
#include "cached_file.h"
#include "concurrency_limit.h"
#include "gitlstree.h"
#include "strutil.h"

using std::cout;
using std::endl;
using std::make_unique;
using std::mutex;
using std::string;
using std::thread;
using std::unique_lock;
using std::unique_ptr;
using std::unordered_map;
using std::vector;

namespace gitlstree {

struct Configuration {
public:
  Configuration(const string& my_gitdir, const string& my_ssh, 
		const string& cache_path)
    : gitdir(my_gitdir), ssh(my_ssh), cache(cache_path) {
  }
  // Directory for git directory. Needed because fuse chdir to / on
  // becoming a daemon.
  const std::string gitdir;
  const std::string ssh;
  Cache cache;
};
// Per-mountpoint configuration.
static unique_ptr<Configuration> configuration{};

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
string RunGitCommand(const vector<string>& commands, int* exit_code) {
  if (!configuration->ssh.empty()) {
    string command;
    for (const auto& s: commands) {
      command += s + " ";
    }
    ScopedConcurrencyLimit l(command);
    return PopenAndReadOrDie2({"ssh", configuration->ssh,
	  string("cd ") + configuration->gitdir + " && "
	  + command}, nullptr, exit_code);
  } else {
    return PopenAndReadOrDie2(commands, &configuration->gitdir, exit_code);
  }
}

bool LoadDirectory(const string& my_gitdir, const string& hash,
		   const string& maybe_ssh, const string& cached_dir,
		   directory_container::DirectoryContainer* container) {
  configuration.reset(nullptr);
  configuration.reset(new Configuration(my_gitdir, maybe_ssh, cached_dir));

  int exit_code;
  string git_ls_tree(RunGitCommand({"git", "ls-tree", "-l", "-r", hash}, &exit_code));
  if (exit_code != 0) {
    // Failed to load directory.
    return false;
  }

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
      const string& file_path = elements[4];
      assert(file_path[0] != '/');  // git ls-tree do not start with /.
      string basename = BaseName(file_path);
      container->add(string("/") + file_path,
		     make_unique<FileElement>(strtol(elements[0].c_str(), NULL, 8),
					      elements[2],
					      atoi(elements[3].c_str())));
    }
  }
  for (auto& job : jobs) { job.join(); }
  return true;
}

FileElement::FileElement(int attribute, const std::string& sha1, int size) :
  attribute_(attribute), sha1_(sha1), size_(size)  {}

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

int FileElement::Open() {
  unique_lock<mutex> l(buf_mutex_);
  if (!memory_) {
    memory_ = configuration->cache.get(sha1_, [this](string* ret) -> bool {
	int exit_code;
	*ret = string(RunGitCommand({"git", "cat-file", "blob", sha1_},
				    &exit_code));
	return exit_code == 0;
      });
    if (!memory_) {
      // If still failed, something failed in the process.
      return -EIO;
    }
  }
  return 0;
}

ssize_t FileElement::Read(char *target, size_t size, off_t offset) {
  if (offset < static_cast<off_t>(memory_->size())) {
    if (offset + size > memory_->size())
      size = memory_->size() - offset;
    memcpy(target, static_cast<const char*>(memory_->memory()) + offset, size);
  } else
    size = 0;
  return size;
}

void FileElement::GetHash(char* hash) const {
  memcpy(hash, sha1_.data(), 40);
}

int FileElement::Release() {
  unique_lock<mutex> l(buf_mutex_);
  configuration->cache.release(sha1_, memory_);
  memory_ = nullptr;
  return 0;
}

}
