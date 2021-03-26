#include "directory_container.h"

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

namespace directory_container {

Directory::Directory() {}
Directory::~Directory() {}

int Directory::Getattr(struct stat* stbuf) {
  stbuf->st_uid = getuid();
  stbuf->st_gid = getgid();
  stbuf->st_mode = S_IFDIR | 0755;
  stbuf->st_nlink = 2;
  return 0;
};

void Directory::dump(int indent) {
  std::lock_guard<std::mutex> l(mutex_);
  for (const auto& file : files_) {
    std::cout << std::string(indent, ' ') << file.first << std::endl;
    Directory* d = dynamic_cast<Directory*>(file.second.get());
    if (d) {
      d->dump(indent + 1);
    }
  }
}

DirectoryContainer::DirectoryContainer() {
  files_["/"] = &root_;
  clock_gettime(CLOCK_REALTIME, &mount_time_);
};

DirectoryContainer::~DirectoryContainer() {}

int DirectoryContainer::Getattr(const std::string& path, struct stat* stbuf) {
  memset(stbuf, 0, sizeof(struct stat));
  stbuf->st_atim = stbuf->st_mtim = stbuf->st_ctim = mount_time_;
  File* f = mutable_get(path);
  if (!f) return -ENOENT;
  return f->Getattr(stbuf);
}

void DirectoryContainer::dump() {
  std::lock_guard<std::mutex> l(path_mutex_);
  std::cout << "Files map" << std::endl;
  for (const auto& file : files_) {
    std::cout << file.first << " " << file.second << std::endl;
    std::cout << "Is directory: "
              << (dynamic_cast<Directory*>(file.second) != nullptr)
              << std::endl;
  }
  std::cout << "Directory map" << std::endl;
  root_.dump();
};

void DirectoryContainer::add(const std::string& path,
                             std::unique_ptr<File> file) {
  std::lock_guard<std::mutex> l(path_mutex_);
  std::string dirname(DirName(path));
  Directory* dir = MaybeCreateParentDir(dirname);
  files_[path] = file.get();
  dir->add(BaseName(path), move(file));
}

const File* DirectoryContainer::get(const std::string& path) const {
  std::lock_guard<std::mutex> l(path_mutex_);
  auto it = files_.find(path);
  if (it != files_.end())
    return it->second;
  else
    return nullptr;
}

File* DirectoryContainer::mutable_get(const std::string& path) {
  std::lock_guard<std::mutex> l(path_mutex_);
  auto it = files_.find(path);
  if (it != files_.end())
    return it->second;
  else
    return nullptr;
}

Directory* DirectoryContainer::MaybeCreateParentDir(
    const std::string& dirname) {
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
}  // namespace directory_container
