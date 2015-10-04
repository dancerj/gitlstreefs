/*
  Uses github rest API v3 to mount filesystem.
 */

#include <assert.h>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <json_spirit.h>
#include <map>
#include <memory>
#include <stdio.h>
#include <sys/stat.h>
#include <unordered_map>
#include <vector>

#include "basename.h"
#include "git-githubfs.h"
#include "strutil.h"

using json_spirit::Value;
using std::cout;
using std::endl;
using std::map;
using std::mutex;
using std::string;
using std::unique_lock;
using std::unique_ptr;
using std::unordered_map;
using std::vector;

namespace githubfs {

const Value& GetObjectField(const json_spirit::Object& object,
			    const string& name) {
  auto it = std::find_if(object.begin(), object.end(),
			 [&name](const json_spirit::Pair& a) -> bool{
			   return a.name_ == name;
			 });
  assert(it != object.end());
  return it->value_;
}

void ParseCommits(const string& commits_string) {
  // Try parsing github api v3 trees output.
  Value commits;
  json_spirit::read(commits_string, commits);
  for (const auto& commit : commits.get_array()) {
    string hash = GetObjectField(GetObjectField(GetObjectField(commit.get_obj(),
							       "commit").get_obj(),
						"tree").get_obj(),
				 "sha").get_str();
    cout << "hash: " << hash << endl;
  }
}

void ParseTrees(const string& trees_string) {
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
	map<string, const Value*> file_property;
	for (const auto& property : file.get_obj()) {
	  file_property[property.name_] = &property.value_;
	}
	cout << file_property["path"]->get_str() << " "
	     << file_property["mode"]->get_str() << " "
	     << file_property["type"]->get_str() << " "
	     << file_property["sha"]->get_str() << endl;
	if (file_property["type"]->get_str() == "blob") {
	  cout << "size: " << file_property["size"]->get_int() << endl;
	}
      }
      break;
    }
  }
}

GitTree::GitTree(const char* hash, const string& gitdir)
  : gitdir_(gitdir), hash_(hash), fullpath_to_files_(), root_() {
  root_.reset(new FileElement(this, S_IFDIR, TYPE_tree,
			      "TODO", 0));
  LoadDirectory(&(root_->files_), "");
  fullpath_to_files_[""] = root_.get();
}

void GitTree::LoadDirectory(FileElement::FileElementMap* files,
			    const string& subdir) {
  string git_ls_tree = PopenAndReadOrDie(string("git ls-tree -l ") +
					 hash_ + " " + subdir) ;
  vector<string> lines;

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
      (*files)[basename].reset(fe);
      fullpath_to_files_[file_path] = fe;
      if (fstype == TYPE_tree) {
	LoadDirectory(&fe->files_, elements[4] + "/");
      }
    }
  }
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
      buf_.reset(new string(PopenAndReadOrDie(string("cd ") +
					      parent_->gitdir() +
					      " && git cat-file blob " +
					      sha1_)));
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
