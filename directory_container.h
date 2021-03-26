#ifndef DIRECTORY_CONTAINER_H_
#define DIRECTORY_CONTAINER_H_

#include <sys/stat.h>

#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

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
  virtual int Getattr(struct stat* stbuf) = 0;

  /**
   * @return >= 0 on success, -errno on fail.
   */
  virtual ssize_t Read(char* buf, size_t size, off_t offset) = 0;
  virtual ssize_t Readlink(char* buf, size_t size) { return -ENOSYS; }

  virtual int Open() = 0;
  virtual int Release() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(File);
};

class Directory : public File {
 public:
  Directory();
  virtual ~Directory();

  virtual int Getattr(struct stat* stbuf) override;

  virtual ssize_t Read(char* buf, size_t size, off_t offset) override {
    // Can't read from a directory.
    return -EINVAL;
  }

  virtual int Open() override {
    // Can't open a directory?
    return -EINVAL;
  }

  virtual int Release() override {
    // Can't release a directory?
    return -EINVAL;
  }

  virtual ssize_t Readlink(char* buf, size_t size) { return -EINVAL; }

  void add(const std::string& path, std::unique_ptr<File> f) {
    std::lock_guard<std::mutex> l(mutex_);
    files_[path] = move(f);
  }

  File* get(const std::string& path) {
    std::lock_guard<std::mutex> l(mutex_);
    auto it = files_.find(path);
    if (it != files_.end()) {
      return it->second.get();
    }
    return nullptr;
  }

  void for_each(std::function<void(const std::string& filename, const File* f)>
                    callback) const {
    std::lock_guard<std::mutex> l(mutex_);
    for (const auto& file : files_) {
      callback(file.first, file.second.get());
    }
  }

  void dump(int indent = 0);

 private:
  typedef std::unordered_map<std::string, std::unique_ptr<File> >
      FileElementMap;
  mutable std::mutex mutex_{};

  FileElementMap files_{};
  DISALLOW_COPY_AND_ASSIGN(Directory);
};

class DirectoryContainer {
 public:
  DirectoryContainer();
  ~DirectoryContainer();

  void add(const std::string& path, std::unique_ptr<File> file);
  const File* get(const std::string& path) const;
  File* mutable_get(const std::string& path);
  bool is_directory(const std::string& path) const {
    std::lock_guard<std::mutex> l(path_mutex_);
    auto it = files_.find(path);
    if (it != files_.end())
      return dynamic_cast<Directory*>(it->second) != nullptr;
    return false;
  }

  int Getattr(const std::string& path, struct stat* stbuf);

  void dump();

  void for_each(const std::string& path,
                std::function<void(const std::string& name, const File* f)>
                    callback) const {
    const Directory* d = dynamic_cast<const Directory*>(get(path));
    if (d) {
      d->for_each([&callback](const std::string& name, const File* f) {
        callback(name, f);
      });
    }
  }

 private:
  // Maybe recursively create directories up to path, and return the Directory
  // object.
  Directory* MaybeCreateParentDir(const std::string& dirname);

  std::unordered_map<std::string /* fullpath */, File*> files_{};
  Directory root_{};
  mutable std::mutex path_mutex_{};

  struct timespec mount_time_ {};
  DISALLOW_COPY_AND_ASSIGN(DirectoryContainer);
};
}  // namespace directory_container

#endif
