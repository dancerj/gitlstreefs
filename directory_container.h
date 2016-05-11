#ifndef DIRECTORY_CONTAINER_H_
#define DIRECTORY_CONTAINER_H_

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cassert>
#include <iostream>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "basename.h"
#include "disallow.h"

namespace directory_container {

// Abstract file class, Directory is implemented here, concrete File
// should be implemented by the user.
class File {
public:
  File() {}
  virtual ~File() {}

  /**
   * @return 0 on success, -errno on fail.
   */
  virtual int Getattr(struct stat *stbuf) = 0;

  /**
   * @return >= 0 on success, -errno on fail.
   */
  virtual ssize_t Read(char *buf, size_t size, off_t offset) = 0;

  virtual int Open() = 0;

  DISALLOW_COPY_AND_ASSIGN(File);
};

class Directory : public File {
public:
  Directory() {}
  virtual ~Directory() {}

  virtual int Getattr(struct stat *stbuf) override {
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
    return 0;
  };

  virtual ssize_t Read(char *buf, size_t size, off_t offset) override {
    // Can't read from a directory.
    return -EINVAL;
  }

  virtual int Open() override {
    // Can't open a directory?
    return -EINVAL;
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
      Directory* d = dynamic_cast<Directory*>(file.second.get());
      if (d) {
	d->dump(indent + 1);
      }
    }
  }

private:
  typedef std::unordered_map<std::string,
			     std::unique_ptr<File> > FileElementMap;
  FileElementMap files_{};
  DISALLOW_COPY_AND_ASSIGN(Directory);
};

class DirectoryContainer {
public:
  DirectoryContainer() {
    files_["/"] = &root_;
    clock_gettime(CLOCK_REALTIME, &mount_time_);
  };
  ~DirectoryContainer() {};

  void add(const std::string& path, std::unique_ptr<File> file) {
    std::unique_lock<std::mutex> l(path_mutex_);
    std::string dirname(DirName(path));
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
      return dynamic_cast<Directory*>(it->second) != nullptr;
    return false;
  }

  int Getattr(const std::string& path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));
    stbuf->st_atim = stbuf->st_mtim = stbuf->st_ctim =
      mount_time_;
    File* f = mutable_get(path);
    if (!f) return -ENOENT;
    return f->Getattr(stbuf);
  }

  void dump() {
    std::cout << "Files map" << std::endl;
    for (const auto& file : files_) {
      std::cout << file.first << " " << file.second << std::endl;
      std::cout << "Is directory: "
		<< (dynamic_cast<Directory*>(file.second) != nullptr) << std::endl;
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
  // Maybe recursively create directories up to path, and return the Directory object.
  Directory* MaybeCreateParentDir(const std::string& dirname) {
    if (dirname == "") return &root_;
    auto it = files_.find(dirname);
    if (it != files_.end()) {
      return static_cast<Directory*>(it->second);
    }

    Directory* directory = new Directory();
    std::string parent(DirName(dirname));
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

  std::unordered_map<std::string /* fullpath */ , File*> files_;
  Directory root_;
  std::mutex path_mutex_{};

  struct timespec mount_time_;
  DISALLOW_COPY_AND_ASSIGN(DirectoryContainer);
};
} // namespace directory_container

#endif
