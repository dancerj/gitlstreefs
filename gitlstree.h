#if !defined(GITLSTREE_H__)
#define GITLSTREE_H__

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
  typedef directory_container::DirectoryContainer<FileElement> DirectoryContainer;
  FileElement(const DirectoryContainer* parent,
	      int attribute, GitFileType file_type,
	      const std::string& sha1, int size);
  int attribute_;
  GitFileType file_type_;
  std::string sha1_;
  int size_;

  void Open();
  ssize_t Read(char *buf, size_t size, off_t offset);
  virtual int Getattr(struct stat *stbuf);
private:
  // If file content is read, this should be populated.
  std::unique_ptr<std::string> buf_{};
  std::mutex buf_mutex_{};

  const DirectoryContainer* parent_;
};

GitFileType FileTypeStringToFileType(const std::string& file_type_string);

void LoadDirectory(const std::string& gitdir, 
		   const std::string& hash, 
		   const std::string& maybe_ssh, 
		   FileElement::DirectoryContainer* container);

} // namespace gitlstree
#endif

