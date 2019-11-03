/*
  Uses github rest API v3 to mount filesystem.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base64decode.h"
#include "concurrency_limit.h"
#include "git-githubfs.h"
#include "jsonparser.h"
#include "strutil.h"
#include "scoped_timer.h"

using std::async;
using std::cout;
using std::endl;
using std::function;
using std::future;
using std::lock_guard;
using std::map;
using std::mutex;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::vector;

namespace githubfs {

namespace {
string HttpFetch(const string& url, const string& key) {
  ScopedConcurrencyLimit l(url);
  scoped_timer::ScopedTimer timer(key);
  vector<string> request{"curl",
      "-s",
      "-A",
      "git-githubfs(https://github.com/dancerj/gitlstreefs)",
      url};
  return PopenAndReadOrDie2(request);
}

#define TYPE(a) {#a, TYPE_##a}
const static unordered_map<string, GitFileType> file_type_map {
  TYPE(blob),
  TYPE(tree),
  TYPE(commit)
};
#undef TYPE

GitFileType FileTypeStringToFileType(const string& file_type_string) {
  return file_type_map.find(file_type_string)->second;
}
} // anonymous

string ParseCommits(const string& commits_string) {
  // Try parsing github api v3 commits output.
  unique_ptr<jjson::Value> commits = jjson::Parse(commits_string);
  for (const auto& commit : commits->get_array()) {
    string hash = commit->get("commit")["tree"]["sha"].get_string();
    cout << "hash: " << hash << endl;
    return hash;
  }
  return "";
}

string ParseCommit(const string& commit_string) {
  // Try parsing github api v3 commit output.
  unique_ptr<jjson::Value> commit = jjson::Parse(commit_string);
  string hash = commit->get("commit")["tree"]["sha"].get_string();
  cout << "hash: " << hash << endl;
  return hash;
}

string ParseBlob(const string& blob_string) {
  // Try parsing github api v3 blob output.
  unique_ptr<jjson::Value> blob = jjson::Parse(blob_string);
  assert(blob->get("encoding").get_string() == "base64");
  string base64 = blob->get("content").get_string();
  return base64decode(base64);
}

// Parses tree object from json, returns false if it was truncated and
// needs retry.
bool ParseTrees(const string& trees_string, function<void(const string& path,
							  int mode,
							  GitFileType fstype,
							  const string& sha,
							  const int size,
							  const string& url)> file_handler) {
  // Try parsing github api v3 trees output.
  unique_ptr<jjson::Value> value = jjson::Parse(trees_string);

  bool truncated = value->get("truncated").is_true();
  if (truncated) return false;

  for (const auto& file : (*value)["tree"].get_array()) {
    // "path": ".gitignore",
    // "mode": "100644",
    // "type": "blob",
    // "sha": "0eca3e92941236b77ad23a02dc0c000cd0da7a18",
    // "size": 46,
    // "url": "https://api.github.com/repos/dancerj/gitlstreefs/git/blobs/0eca3e92941236b77ad23a02dc0c000cd0da7a18"

    // TODO: this is not the most efficient way to parse this
    // structure.
    size_t file_size = 0;
    if (file->get("type").get_string() == "blob") {
      file_size = file->get("size").get_number();
    }
    GitFileType fstype = FileTypeStringToFileType(file->get("type").get_string());
    file_handler(file->get("path").get_string(),
		 strtol(file->get("mode").get_string().c_str(), nullptr, 8),
		 fstype,
		 file->get("sha").get_string(),
		 file_size,
		 file->get("url").get_string());
  }
  return true;
}

// Convert from Git attributes to filesystem attributes.
int FileElement::Getattr(struct stat *stbuf) {
  stbuf->st_uid = getuid();
  stbuf->st_gid = getgid();
  // stbuf->st_atim = stbuf->st_mtim = stbuf->st_ctim;
  stbuf->st_nlink = 1;
  if (attribute_ == S_IFDIR) {
    // Would we get this?
    static_assert(S_IFDIR == 040000, "dir stat attribute wrong.");
    // This is a directory.
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
  } else if (attribute_ == S_IFLNK) {
    // symbolic link.
    static_assert(S_IFLNK == 0120000, "symlink stat attribute wrong.");
    stbuf->st_mode = S_IFLNK | 0644;
  } else {
    stbuf->st_mode = S_IFREG | attribute_;
  }
  stbuf->st_size = size_;
  return 0;
}

FileElement::FileElement(int attribute,const std::string& sha1, int size, GitTree* parent) :
  attribute_(attribute), sha1_(sha1), size_(size), parent_(parent) {}

ssize_t FileElement::Read(char *target, size_t size, off_t offset) {
  lock_guard<mutex> l(buf_mutex_);
  if (!memory_) return -1;
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
  if (e != 0) {
    return e;
  }
  if (size > memory_->size()) {
    size = memory_->size();
    target[size] = 0;
  } else {
    // TODO: is this an error condition that we can't fit in the final 0 ?
  }
  const char* source = memory_->memory_charp();

  memcpy(target, source, size);
  return 0;
}

ssize_t FileElement::maybe_cat_file_locked() {
  if (!memory_) {
    memory_ = parent_->cache().get(sha1_, [this](string* ret) -> bool {
	const string url = parent_->get_github_api_prefix() +
	  "/git/blobs/" + sha1_;
	string blob_string = HttpFetch(url, "blob");
	*ret = ParseBlob(blob_string);
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

int FileElement::Release() {
  lock_guard<mutex> l(buf_mutex_);
  parent_->cache().release(sha1_, memory_);
  memory_ = nullptr;
  return 0;
}

void GitTree::LoadDirectoryInternal(const string& subdir, const string& tree_hash,
				    bool remote_recurse) {
  vector<future<void> > jobs;
  string fetch_url = github_api_prefix_ + "/git/trees/" + tree_hash;
  if (remote_recurse) {
    // Let the remote system recurse.
    fetch_url += "?recursive=true";
  }
  const string github_tree = HttpFetch(fetch_url, "lstree");
  cout << "Loaded directory " << subdir << endl;
  if (ParseTrees(github_tree,
		  [&](const string& path,
		      int mode,
		      GitFileType fstype,
		      const string& sha,
		      const int size,
		      const string& url){
		   const std::string slash_path = string("/") + path;
		   if (fstype == TYPE_blob) {
		     container_->add(slash_path,
				     std::make_unique<FileElement>(mode, sha, size, this));
		   } else if (fstype == TYPE_tree) {
		     // Nonempty directories get auto-created, but maybe do it here?
		     container_->add(slash_path,
				     std::make_unique<directory_container::Directory>());
		     if (remote_recurse == false) {
		       // If remote side recursion didn't work, do recursion here.
		       jobs.emplace_back(async([this, subdir, path, sha](){
			     LoadDirectoryInternal(subdir + path + "/", sha, false);
			   }));
		     }
		   }
		 })) {
  } else {
    cout << "Retry with remote recursion off." << endl;
    LoadDirectoryInternal(subdir, tree_hash, false);
  }
}

GitTree::GitTree(const char* hash, const char* github_api_prefix,
		 directory_container::DirectoryContainer* container,
		 const std::string& cache_dir)
    : github_api_prefix_(github_api_prefix), container_(container), cache_(cache_dir) {
  cache_.Gc();
  string commit = HttpFetch(github_api_prefix_ + "/commits/" + hash, "commit");
  const string tree_hash = ParseCommit(commit);

  LoadDirectoryInternal("", tree_hash, true /* remote recurse*/);
  container->add("/.status", std::make_unique<scoped_timer::StatusHandler>());
}

GitTree::~GitTree() {}

}  // githubfs
