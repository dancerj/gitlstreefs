#include <unordered_map>
#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <iostream>

#include "basename.h"

using std::function;
using std::map;
using std::string;
using std::unique_ptr;
using std::vector;
using std::pair;
using std::unordered_map;
using std::cout;
using std::endl;

// Some random implementation of an abstract file.
class File {
public:
  File() {}
  virtual ~File() {}

  virtual int Getattr(struct stat *stbuf) const = 0;
  virtual bool is_directory() const {
    return false;
  }
};

class Directory : public File {
public:
  Directory() {}
  virtual ~Directory() {}

  virtual int Getattr(struct stat *stbuf) const {
    return 0;
  };
  virtual bool is_directory() const {
    return true;
  }
  void add(const string& path, unique_ptr<File> f) {
    files_[path] = move(f);
  }
  File* get(const string& path) {
    auto it = files_.find(path);
    if (it != files_.end()) {
      return it->second.get();
    }
    return nullptr;
  }
  void dump() {
    for (const auto& file: files_) {
      cout << file.first << endl;
    }
  }
private:
  typedef std::unordered_map<std::string,
			     std::unique_ptr<File> > FileElementMap;
  FileElementMap files_{};
};

// Concrete class.
class GitFile : public File {
public:
  GitFile() {}
  virtual ~GitFile() {}
  virtual int Getattr(struct stat *stbuf) const {
    return 0;
  }
};

template<class _File>
class DirectoryContainer {
public:
  DirectoryContainer() {
    files_["/"] = &root_;
  };
  ~DirectoryContainer() {};

  // Maybe recursively create directories up to path, and return the Directory object.
  Directory* MaybeCreateParentDir(const string& dirname) {
    assert(dirname.size() >= 1);
    assert(dirname[dirname.size() - 1] == '/');
    if (dirname == "/") return &root_;
    Directory* directory = static_cast<Directory*>(files_[dirname]);
    if (directory) {
      return directory;
    }

    directory = new Directory();
    string parent(DirName(dirname.substr(0, dirname.size() - 1)));
    Directory* parent_directory = static_cast<Directory*>(files_[parent]);
    if (parent_directory == nullptr) {
      parent_directory = MaybeCreateParentDir(parent);
    }
    files_[dirname] = directory;
    parent_directory->add(dirname, unique_ptr<File>(directory));
    return directory;
  }

  void add(const string& path, unique_ptr<_File> file) {
    string dirname(DirName(path));
    Directory* dir = MaybeCreateParentDir(dirname);
    files_[path] = file.get();
    dir->add(path, move(file));
  }

  const _File* get(const string& path) {
    auto it = files_.find(path);
    if (it != files_.end())
      return it->second;
    else
      return nullptr;
  }

  bool is_directory(const string& path) {
    auto it = files_.find(path);
    if (it != files_.end())
      return it->second->is_directory();
    return false;
  }

  void dump() {
    cout << "Files map" << endl;
    for (const auto& file : files_) {
      cout << file.first << " " << file.second << endl;
      cout << "Is directory: " << file.second->is_directory() << endl;
    }
    cout << "Directory map" << endl;
    root_.dump();
  };

  vector<pair<const string&, _File*> > get_dir(const string& path);
  void for_each(const string& path, function<void(const string& full_path, _File* f)>);

private:
  map<string /* fullpath */ , File*> files_;
  Directory root_;
};

int main() {
  DirectoryContainer<GitFile> d;
  d.add("/this/dir", std::make_unique<GitFile>());
  d.add("/the", std::make_unique<GitFile>());
  d.add("/a", std::make_unique<GitFile>());
  d.add("/hoge/a", std::make_unique<GitFile>());

  d.dump();

  assert(d.is_directory("/this/"));
  assert(d.is_directory("/hoge/"));
  assert(!d.is_directory("/hog"));
  assert(!d.is_directory("/a"));
}
