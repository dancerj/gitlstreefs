#ifndef GIT_GITHUBFS_H_
#define GIT_GITHUBFS_H_

#include <mutex>
#include <unordered_map>

#include "cached_file.h"
#include "disallow.h"
#include "directory_container.h"

namespace githubfs {

enum GitFileType {
  TYPE_blob,
  TYPE_tree,
  TYPE_commit
};

// Github api v3 response parsers.
// Parse tree content.
bool ParseTrees(const std::string& trees_string,
		std::function<void(const std::string& path,
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

class GitTree;

struct FileElement : public directory_container::File {
public:
  FileElement(int attribute, const std::string& sha1, int size, GitTree* parent);
  virtual int Open() override;
  virtual ssize_t Read(char *buf, size_t size, off_t offset) override;
  virtual ssize_t Readlink(char *buf, size_t size) override;
  virtual int Getattr(struct stat *stbuf) override;
  virtual int Release() override;

private:
  ssize_t maybe_cat_file_locked();

  int attribute_;
  std::string sha1_;
  int size_;

  GitTree* parent_;
  const Cache::Memory* memory_{};
  std::mutex buf_mutex_{};
  DISALLOW_COPY_AND_ASSIGN(FileElement);
};

class GitTree {
public:
  GitTree(const char* hash, const char* github_api_prefix,
	  directory_container::DirectoryContainer* c,
	  const std::string& cache_dir);
  ~GitTree();
  const std::string& get_github_api_prefix() const { return github_api_prefix_; }
  Cache& cache() { return cache_; }

private:
  void LoadDirectoryInternal(const std::string& subdir, const std::string& tree_hash,
			     bool remote_recurse);

  // Directory for git directory. Needed because fuse chdir to / on
  // becoming a daemon.
  const std::string github_api_prefix_;
  directory_container::DirectoryContainer* container_;
  Cache cache_;
};

} // namespace githubfs
#endif
