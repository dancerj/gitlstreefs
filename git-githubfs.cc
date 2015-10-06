/*
  Uses github rest API v3 to mount filesystem.
 */

#include <assert.h>
#include <boost/algorithm/string.hpp>
#include <functional>
#include <iostream>
#include <json_spirit.h>
#include <map>
#include <memory>
#include <stdio.h>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unordered_map>
#include <vector>

#include "git-githubfs.h"
#include "strutil.h"

using json_spirit::Value;
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
  static const string request_prefix = "curl -A 'git-githubfs(https://github.com/dancerj/gitlstreefs)' ";

  return PopenAndReadOrDie(request_prefix + url);
}
} // anonymous

const Value& GetObjectField(const string& name,
			    const json_spirit::Object& object) {
  auto it = std::find_if(object.begin(), object.end(),
			 [&name](const json_spirit::Pair& a) -> bool{
			   return a.name_ == name;
			 });
  assert(it != object.end());
  return it->value_;
}

string ParseCommits(const string& commits_string) {
  // Try parsing github api v3 commits output.
  Value commits;
  json_spirit::read(commits_string, commits);
  for (const auto& commit : commits.get_array()) {
    string hash = GetObjectField("sha",
				 GetObjectField("tree",
						GetObjectField("commit",
							       commit.get_obj())
						.get_obj())
				 .get_obj()).get_str();
    cout << "hash: " << hash << endl;
    return hash;
  }
  return "";
}

string ParseCommit(const string& commit_string) {
  // Try parsing github api v3 commit output.
  Value commit;
  json_spirit::read(commit_string, commit);
  string hash = GetObjectField("sha",
			       GetObjectField("tree",
					      GetObjectField("commit",
							     commit.get_obj())
					      .get_obj())
			       .get_obj()).get_str();
  cout << "hash: " << hash << endl;
  return hash;
}

string base64_decode(string base64) {
  // TODO there must be a better way to do this.
  return PopenAndReadOrDie(string("echo '") + base64 + "' | base64 -d ");
}

string ParseBlob(const string& blob_string) {
  // Try parsing github api v3 blob output.
  Value blob;
  json_spirit::read(blob_string, blob);
  assert(GetObjectField("encoding", blob.get_obj()).get_str() == "base64");
  json_spirit::Object& o = blob.get_obj();
  string base64 = GetObjectField("content", blob.get_obj()).get_str();
  return base64_decode(base64);
}

void ParseTrees(const string& trees_string, function<void(const string& path,
							  int mode,
							  const GitFileType fstype,
							  const string& sha,
							  const int size,
							  const string& url)> file_handler) {
  // Try parsing github api v3 trees output.
  Value value;
  json_spirit::read(trees_string, value);
  for (const auto& tree : value.get_obj()) {
    // object is a vector of pair of key-value pairs.
    if (tree.name_== "tree") {
      for (const auto& file : tree.value_.get_array()) {
	// "path": ".gitignore",
	// "mode": "100644",
	// "type": "blob",
	// "sha": "0eca3e92941236b77ad23a02dc0c000cd0da7a18",
	// "size": 46,
	// "url": "https://api.github.com/repos/dancerj/gitlstreefs/git/blobs/0eca3e92941236b77ad23a02dc0c000cd0da7a18"

	// TODO: this is not the most efficient way to parse this
	// structure.
	map<string, const Value*> file_property;
	for (const auto& property : file.get_obj()) {
	  file_property[property.name_] = &property.value_;
	}
	size_t file_size = 0;
	if (file_property["type"]->get_str() == "blob") {
	  file_size = file_property["size"]->get_int();
	}
	GitFileType fstype = FileTypeStringToFileType(file_property["type"]->get_str());
	file_handler(file_property["path"]->get_str(),
		     strtol(file_property["mode"]->get_str().c_str(), NULL, 8),
		     fstype,
		     file_property["sha"]->get_str(),
		     file_size, 
		     file_property["url"]->get_str());
      }
      break;
    }
  }
}

// github_api_prefix such as "https://api.github.com/repos/dancerj/gitlstreefs"
GitTree::GitTree(const char* hash, const char* github_api_prefix)
  : hash_(hash), github_api_prefix_(github_api_prefix),
    fullpath_to_files_(), root_() {
  root_.reset(new FileElement(this, S_IFDIR, TYPE_tree,
			      "TODO", 0));
  string commit = HttpFetch(github_api_prefix_ + "/commits/" + hash);
  const string tree_hash = ParseCommit(commit);

  LoadDirectory(&(root_->files_), "", tree_hash);
  fullpath_to_files_[""] = root_.get();
}

void GitTree::LoadDirectory(FileElement::FileElementMap* files,
			    const string& subdir, const string& tree_hash) {
  cout << "Loading directory " << subdir << endl;
  vector<thread> jobs;
  string github_tree = HttpFetch(github_api_prefix_ + "/git/trees/" + tree_hash);
  ParseTrees(github_tree,
	     [&](const string& name,
		 int mode,
		 const GitFileType fstype,
		 const string& sha,
		 const int size,
		 const string& url){
	       FileElement* fe = new FileElement(this, mode, fstype, sha, size);
	       {
		 unique_lock<mutex> l(path_mutex_);
		 (*files)[name].reset(fe);
		 fullpath_to_files_[subdir + name] = fe;
	       }
	       if (fstype == TYPE_tree) {
		 jobs.emplace_back(thread([this, fe, subdir, name, sha](){
		       LoadDirectory(&fe->files_, subdir + name + "/", sha);
		     }));
	       }
	     });
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
      const string& url = parent_->get_github_api_prefix() +
	"/git/blobs/" + sha1_;
      string blob_string = HttpFetch(url);
      buf_.reset(new string(ParseBlob(blob_string)));
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
