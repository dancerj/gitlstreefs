#if !defined(GITHUBFS_H__)
#define GITHUBFS_H__

#include <mutex>
#include <unordered_map>

#include "disallow.h"

namespace githubfs {

class GitTree;

enum GitFileType {
  TYPE_blob = 1,
  TYPE_tree = 2,
  TYPE_commit = 3
};

// Github api v3 response parsers.
// Parse tree content.
void ParseTrees(const std::string& trees_string, std::function<void(const std::string& path,
								    int mode,
								    const GitFileType type,
								    const std::string& sha,
								    const int size,
								    const std::string& url)> file_handler);

// Parse github commits list and return the tree hash.
// for /commits endpoint.
std::string ParseCommits(const std::string& commits_string);
// for /commits/hash endpoint.
std::string ParseCommit(const std::string& commit_string);

// Parse blob.
std::string ParseBlob(const std::string& blob_string);


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
  DISALLOW_COPY_AND_ASSIGN(FileElement);
};

GitFileType FileTypeStringToFileType(const std::string& file_type_string);

class GitTree {
public:
  GitTree(const char* hash, const char* github_api_prefix);
  FileElement* const get(const std::string& fullpath) const {
    assert(fullpath[0] != '/');
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

  const std::string& get_github_api_prefix() const {
    return github_api_prefix_;
  }
private:
  const std::string hash_;
  const std::string github_api_prefix_;
  void LoadDirectory(FileElement::FileElementMap* files,
		     const std::string& subdir, const std::string& tree_hash);
  // Full path without starting /
  std::unordered_map<std::string,
		     FileElement*> fullpath_to_files_;
  std::mutex path_mutex_{};

  std::unique_ptr<FileElement> root_;

  DISALLOW_COPY_AND_ASSIGN(GitTree);
};

} // namespace githubfs
#endif

