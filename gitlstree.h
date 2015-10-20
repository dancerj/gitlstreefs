#if !defined(GITLSTREE_H__)
#define GITLSTREE_H__

#include <sys/ioctl.h>

#include <mutex>
#include <unordered_map>

#include "directory_container.h"

namespace gitlstree {

enum GitFileType {
  TYPE_blob = 1,
  TYPE_tree = 2,
  TYPE_commit = 3
};

class FileElement : public directory_container::File {
public:
  FileElement(int attribute, GitFileType file_type,
	      const std::string& sha1, int size);
  void Open();
  ssize_t Read(char *buf, size_t size, off_t offset);
  virtual int Getattr(struct stat *stbuf);
  void GetHash(char* hash) const;

private:
  // If file content is read, this should be populated.
  std::unique_ptr<std::string> buf_{};
  std::mutex buf_mutex_{};

  int attribute_;
  GitFileType file_type_;
  std::string sha1_;
  int size_;
};

GitFileType FileTypeStringToFileType(const std::string& file_type_string);

void LoadDirectory(const std::string& gitdir, 
		   const std::string& hash, 
		   const std::string& maybe_ssh, 
		   directory_container::DirectoryContainer* container);

struct GetHashIoctlArg {
public:
  GetHashIoctlArg() {}

  // Verify that transport worked.
  static constexpr size_t kSize = 40;
  void verify() { assert(size == kSize); }

  size_t size{kSize};
  char hash[kSize + 1]{};
};

constexpr int IOCTL_MAGIC_NUMBER = 0;
constexpr int IOCTL_GIT_HASH_COMMAND = 1;

constexpr int IOCTL_GIT_HASH = _IOR(IOCTL_MAGIC_NUMBER,
				    IOCTL_GIT_HASH_COMMAND, GetHashIoctlArg);

} // namespace gitlstree
#endif

