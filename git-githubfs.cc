/*
  Uses github rest API v3 to mount filesystem.
 */

#include <assert.h>
#include <boost/algorithm/string.hpp>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <stdio.h>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unordered_map>
#include <vector>

#include "base64decode.h"
#include "concurrency_limit.h"
#include "git-githubfs.h"
#include "jsonparser.h"
#include "strutil.h"
#include "scoped_timer.h"

using std::cout;
using std::endl;
using std::function;
using std::map;
using std::mutex;
using std::string;
using std::unique_lock;
using std::unique_ptr;
using std::unordered_map;
using std::vector;
using std::thread;

namespace githubfs {

namespace {
string HttpFetch(const string& url) {
  ScopedConcurrencyLimit l(url);
  ScopedTimer timer(url);
  vector<string> request{"curl",
      "-s",
      "-A",
      "git-githubfs(https://github.com/dancerj/gitlstreefs)"};
  request.push_back(url);
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
		 strtol(file->get("mode").get_string().c_str(), NULL, 8),
		 fstype,
		 file->get("sha").get_string(),
		 file_size,
		 file->get("url").get_string());
  }
  return true;
}

void GitTree::LoadDirectoryInternal(const string& subdir, const string& tree_hash,
				    bool remote_recurse) {
  vector<thread> jobs;
  string fetch_url = github_api_prefix_ + "/git/trees/" + tree_hash;
  if (remote_recurse) {
    // Let the remote system recurse.
    fetch_url += "?recursive=true";
  }
  const string github_tree = HttpFetch(fetch_url);
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
		       jobs.emplace_back(thread([this, subdir, path, sha](){
			     LoadDirectoryInternal(subdir + path + "/", sha, false);
			   }));
		     }
		   }
		 })) {
    for (auto& job : jobs) { job.join(); }
  } else {
    cout << "Retry with remote recursion off." << endl;
    LoadDirectoryInternal(subdir, tree_hash, false);
  }
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
  unique_lock<mutex> l(buf_mutex_);
  if (!memory_) return -1;
  if (offset < static_cast<off_t>(memory_->size())) {
    if (offset + size > memory_->size())
      size = memory_->size() - offset;
    memcpy(target, static_cast<const char*>(memory_->memory()) + offset, size);
  } else
    size = 0;
  return size;
}

int FileElement::Open() {
  unique_lock<mutex> l(buf_mutex_);
  if (!memory_) {
    memory_ = parent_->cache().get(sha1_, [this](string* ret) -> bool {
	const string url = parent_->get_github_api_prefix() +
	  "/git/blobs/" + sha1_;
	string blob_string = HttpFetch(url);
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

int FileElement::Release() {
  unique_lock<mutex> l(buf_mutex_);
  parent_->cache().release(sha1_, memory_);
  memory_ = nullptr;
  return 0;
}

GitTree::GitTree(const char* hash, const char* github_api_prefix,
		 directory_container::DirectoryContainer* container,
		 const std::string& cache_dir)
    : github_api_prefix_(github_api_prefix), container_(container), cache_(cache_dir) {
  string commit = HttpFetch(github_api_prefix_ + "/commits/" + hash);
  const string tree_hash = ParseCommit(commit);

  LoadDirectoryInternal("", tree_hash, true /* remote recurse*/);
}

GitTree::~GitTree() {}

}  // githubfs
