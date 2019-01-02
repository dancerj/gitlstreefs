#ifndef GITLSTREE_H_
#define GITLSTREE_H_

#include <sys/ioctl.h>

#include <mutex>

#include "cached_file.h"
#include "directory_container.h"
#include "disallow.h"

namespace gitlstree {

enum GitFileType {
  TYPE_blob = 1,
  TYPE_tree = 2,
  TYPE_commit = 3
};

class GitTree;

class FileElement : public directory_container::File {
public:
  FileElement(int attribute, const std::string& sha1, int size, GitTree* parent);
  virtual int Open() override;
  virtual ssize_t Read(char *buf, size_t size, off_t offset) override;
  virtual ssize_t Readlink(char *buf, size_t size) override;
  virtual int Getattr(struct stat *stbuf) override;
  int Release();
  void GetHash(char* hash) const;

private:
  int maybe_cat_file_locked();

  // If file content is read, this should be populated.
  const Cache::Memory* memory_{};
  std::mutex buf_mutex_{};

  int attribute_;
  std::string sha1_;
  int size_;

  GitTree* parent_;
  DISALLOW_COPY_AND_ASSIGN(FileElement);
};

class GitTree {
public:
  static std::unique_ptr<GitTree>
  NewGitTree(const std::string& gitdir,
	     const std::string& hash,
	     const std::string& maybe_ssh,
	     const std::string& cache_dir,
	     directory_container::DirectoryContainer* container);
  ~GitTree();

  std::string RunGitCommand(const std::vector<std::string>& commands,
			    int* exit_code, const std::string& log_tag);

  Cache& cache() { return cache_; }

private:
  GitTree(const std::string& gitdir,
	  const std::string& maybe_ssh,
	  const std::string& cache_dir);
  bool LoadDirectory(
	  const std::string& hash,
	  directory_container::DirectoryContainer* container);

  const std::string gitdir_;
  const std::string ssh_;
  Cache cache_;

  DISALLOW_COPY_AND_ASSIGN(GitTree);
};

struct GetHashIoctlArg {
public:
  GetHashIoctlArg() {}
  ~GetHashIoctlArg() {}

  // Verify that transport worked.
  static constexpr size_t kSize = 40;
  void verify() { assert(size == kSize); }

  size_t size = kSize;
  char hash[kSize + 1]{};

  DISALLOW_COPY_AND_ASSIGN(GetHashIoctlArg);
};

constexpr int IOCTL_MAGIC_NUMBER = 0;
constexpr int IOCTL_GIT_HASH_COMMAND = 1;

constexpr int IOCTL_GIT_HASH = _IOR(IOCTL_MAGIC_NUMBER,
				    IOCTL_GIT_HASH_COMMAND, GetHashIoctlArg);

} // namespace gitlstree
#endif
