#if !defined(DIRECTORY_CONTAINER_H__)
#define DIRECTORY_CONTAINER_H__

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cassert>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

#include "basename.h"

namespace directory_container {

// Some random implementation of an abstract file.
class File {
public:
  File() {}
  virtual ~File() {}

  virtual int Getattr(struct stat *stbuf) = 0;
  virtual bool is_directory() const {
    return false;
  }
};

class Directory : public File {
public:
  Directory() {}
  virtual ~Directory() {}

  virtual int Getattr(struct stat *stbuf) {
    stbuf->st_mode = S_IFDIR | 0777;
    stbuf->st_nlink = 2;
    return 0;
  };
  virtual bool is_directory() const {
    return true;
  }

  void add(const std::string& path, std::unique_ptr<File> f) {
    files_[path] = move(f);
  }

  File* get(const std::string& path) {
    auto it = files_.find(path);
    if (it != files_.end()) {
      return it->second.get();
    }
    return nullptr;
  }

  void for_each(std::function<void(const std::string& filename, const File* f)> callback) const {
    for (const auto& file: files_) {
      callback(file.first, file.second.get());
    }
  }

  void dump(int indent = 0) {
    for (const auto& file: files_) {
      std::cout << std::string(indent, ' ') << file.first << std::endl;
      if (file.second->is_directory()) {
	static_cast<Directory*>(file.second.get())->dump(indent + 1);
      }
    }
  }
private:
  typedef std::unordered_map<std::string,
			     std::unique_ptr<File> > FileElementMap;
  FileElementMap files_{};
};

template<class _File>
class DirectoryContainer {
public:
  DirectoryContainer() {
    files_["/"] = &root_;
  };
  ~DirectoryContainer() {};

  std::string StripTrailingSlash(std::string s) {
    assert(s.size() >= 1);
    assert(s[s.size() - 1] == '/');
    return s.substr(0, s.size() - 1);
  }

  // Maybe recursively create directories up to path, and return the Directory object.
  Directory* MaybeCreateParentDir(const std::string& dirname) {
    if (dirname == "") return &root_;
    auto it = files_.find(dirname);
    if (it != files_.end()) {
      return static_cast<Directory*>(it->second);
    }

    Directory* directory = new Directory();
    std::string parent(StripTrailingSlash(DirName(dirname)));
    auto parent_it = files_.find(parent);
    Directory* parent_directory;
    if (parent_it != files_.end()) {
      parent_directory = static_cast<Directory*>(parent_it->second);
    } else {
      parent_directory = MaybeCreateParentDir(parent);
    }
    files_[dirname] = directory;
    parent_directory->add(BaseName(dirname), std::unique_ptr<File>(directory));
    return directory;
  }

  void add(const std::string& path, std::unique_ptr<_File> file) {
    std::string dirname(StripTrailingSlash(DirName(path)));
    Directory* dir = MaybeCreateParentDir(dirname);
    files_[path] = file.get();
    dir->add(BaseName(path), move(file));
  }

  const File* get(const std::string& path) const {
    auto it = files_.find(path);
    if (it != files_.end())
      return it->second;
    else
      return nullptr;
  }

  File* mutable_get(const std::string& path) {
    auto it = files_.find(path);
    if (it != files_.end())
      return it->second;
    else
      return nullptr;
  }

  bool is_directory(const std::string& path) {
    auto it = files_.find(path);
    if (it != files_.end())
      return it->second->is_directory();
    return false;
  }

  int Getattr(const std::string& path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));
    File* f = mutable_get(path);
    if (!f) return -ENOENT;
    return f->Getattr(stbuf);
  }

  void dump() {
    std::cout << "Files map" << std::endl;
    for (const auto& file : files_) {
      std::cout << file.first << " " << file.second << std::endl;
      std::cout << "Is directory: " << file.second->is_directory() << std::endl;
    }
    std::cout << "Directory map" << std::endl;
    root_.dump();
  };

  void for_each(const std::string& path,
		std::function<void(const std::string& name, const File* f)> callback) const {
    const Directory* d = dynamic_cast<const Directory*>(get(path));
    if (d) {
      d->for_each([&callback](const std::string& name, const File* f){
	  callback(name, f);
	});
    }
  }

private:
  std::unordered_map<std::string /* fullpath */ , File*> files_;
  Directory root_;
};
} // namespace directory_container

#endif
