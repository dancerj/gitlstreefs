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
#include "gitlstree.h"
#include "strutil.h"
#include "concurrency_limit.h"

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
  Configuration(const string& my_gitdir, const string& my_ssh) 
    : gitdir(my_gitdir), ssh(my_ssh) {
    clock_gettime(CLOCK_REALTIME, &mount_time);
  }
  // Directory for git directory. Needed because fuse chdir to / on
  // becoming a daemon.
  std::string gitdir;
  std::string ssh;
  struct timespec mount_time;
};
// Per-mountpoint configuration.
static unique_ptr<Configuration> configuration{};

int FileElement::Getattr(struct stat *stbuf) {
  stbuf->st_uid = getuid();
  stbuf->st_gid = getgid();
  stbuf->st_atim = stbuf->st_mtim = stbuf->st_ctim = configuration->mount_time;
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
string RunGitCommand(const string& command) {
  if (!configuration->ssh.empty()) {
    ScopedConcurrencyLimit l(command);
    return PopenAndReadOrDie(string("ssh ") + configuration->ssh + " 'cd " + configuration->gitdir + " && "
			     + command + "'");
  } else {
    return PopenAndReadOrDie("cd " + configuration->gitdir + " && "
			     + command);
  }
}

void LoadDirectory(const string& my_gitdir, const string& hash, const string& maybe_ssh, FileElement::DirectoryContainer* container) {
  configuration.reset(new Configuration(my_gitdir, maybe_ssh));

  string git_ls_tree = RunGitCommand(string("git ls-tree -l -r ") +
				     hash + " " );
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
      assert(file_path[0] != '/');  // git ls-tree do not start with /.
      string basename = BaseName(file_path);
      container->add(string("/") + file_path,
		     make_unique<FileElement>(container,
					      strtol(elements[0].c_str(), NULL, 8),
					      fstype,
					      elements[2],
					      atoi(elements[3].c_str())));
    }
  }
  for (auto& job : jobs) { job.join(); }
}

FileElement::FileElement(const FileElement::DirectoryContainer* parent, 
			 int attribute, GitFileType file_type,
			 const std::string& sha1, int size) :
  attribute_(attribute), file_type_(file_type),
  sha1_(sha1), size_(size), parent_(parent) {}

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

void FileElement::Open() {
  unique_lock<mutex> l(buf_mutex_);
  if (!buf_.get()) {
    buf_.reset(new string(RunGitCommand("git cat-file blob " + sha1_)));
  }
}

ssize_t FileElement::Read(char *target, size_t size, off_t offset) {
  if (offset < static_cast<off_t>(buf_->size())) {
    if (offset + size > buf_->size())
      size = buf_->size() - offset;
    memcpy(target, buf_->c_str() + offset, size);
  } else
    size = 0;
  return size;
}

}
