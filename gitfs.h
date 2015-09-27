#if !defined(GITFIND_H__)
#define GITFIND_H__

#include <unordered_map>
#include <mutex>

#include "gitxx.h"

namespace gitfs {

class GitTree;
struct FileElement {
public:
  FileElement(GitTree* parent,
	      int attribute, git_otype file_type,
	      const std::string& sha1, int size,
	      std::unique_ptr<gitxx::Object> object);
  int attribute_;
  git_otype file_type_;
  std::string sha1_;
  int size_;

  void for_each_filename(std::function<void(const std::string& filename)>
			 callback) const {
    for (const auto& f : files_) {
      callback(f.first);
    }
  }

  ssize_t Read(char *buf, size_t size, off_t offset);

  typedef std::unordered_map<std::string,
			     std::unique_ptr<FileElement> > FileElementMap;
  // If directory, keep the subdirectories here.
  FileElementMap files_{};

private:
  // If file content is read, this should be populated.
  std::unique_ptr<std::string> buf_{};
  std::mutex buf_mutex_{};

  const GitTree* parent_;
  const std::unique_ptr<gitxx::Object> object_;
  // TODO, do I need any locking?
};

class GitTree {
public:
  GitTree(const char* revision_ref, const std::string& gitdir);
  FileElement* const get(const std::string& fullpath) const {
    auto it = fullpath_to_files_.find(fullpath);
    if (it != fullpath_to_files_.end()) {
      return it->second;
    }
    return nullptr;
  }

  /**
   * @return 0 on success, -errno on fail
   * @param fullpath Full path starting with "/"
   */
  int Getattr(const std::string& fullpath, struct stat *stbuf) const;
  void dump() const;
private:
  gitxx::Repository repo_;
  void LoadDirectory(FileElement::FileElementMap* files,
		     const std::string& subdir,
		     gitxx::Tree* tree);
  // Full path without starting /
  std::unordered_map<std::string,
		     FileElement*> fullpath_to_files_;
  std::unique_ptr<FileElement> root_;
};

} // namespace gitfs
#endif

