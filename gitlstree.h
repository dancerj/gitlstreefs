#if !defined(GITLSTREE_H__)
#define GITLSTREE_H__

#include <mutex>
#include <unordered_map>

namespace gitlstree {

class GitTree;

enum GitFileType {
  TYPE_blob = 1,
  TYPE_tree = 2,
  TYPE_commit = 3
};

struct FileElement {
public:
  FileElement(GitTree* parent,
	      int attribute, GitFileType file_type,
	      const std::string& sha1, int size);
  int attribute_;
  GitFileType file_type_;
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
  // TODO, do I need any locking?
};

GitFileType FileTypeStringToFileType(const std::string& file_type_string);

class GitTree {
public:
  GitTree(const char* hash, const char* ssh, const std::string& gitdir);
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
  const std::string& gitdir() const { return gitdir_; }

  std::string RunGitCommand(const std::string& command) const;
private:
  // Directory for git directory. Needed because fuse chdir to / on
  // becoming a daemon.
  const std::string gitdir_;
  const std::string hash_;
  const std::string ssh_;

  void LoadDirectory(FileElement::FileElementMap* files,
		     const std::string& subdir);
  // Full path without starting /
  std::unordered_map<std::string,
		     FileElement*> fullpath_to_files_;
  std::unique_ptr<FileElement> root_;
  std::mutex path_mutex_{};
};

} // namespace gitlstree
#endif

