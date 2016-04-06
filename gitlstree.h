#if !defined(GITLSTREE_H__)
#define GITLSTREE_H__

#include <sys/ioctl.h>

#include <mutex>
#include <unordered_map>

#include "cached_file.h"
#include "directory_container.h"
#include "disallow.h"

namespace gitlstree {

enum GitFileType {
  TYPE_blob = 1,
  TYPE_tree = 2,
  TYPE_commit = 3
};

class FileElement : public directory_container::File {
public:
  FileElement(int attribute, const std::string& sha1, int size);
  virtual int Open();
  virtual ssize_t Read(char *buf, size_t size, off_t offset);
  virtual int Getattr(struct stat *stbuf);
  int Release();
  void GetHash(char* hash) const;

private:
  // If file content is read, this should be populated.
  const Cache::Memory* memory_{};
  std::mutex buf_mutex_{};

  int attribute_;
  std::string sha1_;
  int size_;

  DISALLOW_COPY_AND_ASSIGN(FileElement);
};

GitFileType FileTypeStringToFileType(const std::string& file_type_string);

bool LoadDirectory(const std::string& gitdir,
		   const std::string& hash,
		   const std::string& maybe_ssh,
		   const std::string& cache_dir,
		   directory_container::DirectoryContainer* container);

struct GetHashIoctlArg {
public:
  GetHashIoctlArg() {}

  // Verify that transport worked.
  static constexpr size_t kSize = 40;
  void verify() { assert(size == kSize); }

  size_t size{kSize};
  char hash[kSize + 1]{};

  DISALLOW_COPY_AND_ASSIGN(GetHashIoctlArg);
};

constexpr int IOCTL_MAGIC_NUMBER = 0;
constexpr int IOCTL_GIT_HASH_COMMAND = 1;

constexpr int IOCTL_GIT_HASH = _IOR(IOCTL_MAGIC_NUMBER,
				    IOCTL_GIT_HASH_COMMAND, GetHashIoctlArg);

} // namespace gitlstree
#endif
