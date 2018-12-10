#ifndef GIT_GITHUBFS_H_
#define GIT_GITHUBFS_H_

#include <mutex>
#include <unordered_map>

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

struct FileElement : public directory_container::File {
public:
  FileElement(int attribute, const std::string& sha1, int size);
  virtual int Open() override;
  virtual ssize_t Read(char *buf, size_t size, off_t offset) override;
  virtual int Getattr(struct stat *stbuf) override;
  int Release();
  void GetHash(char* hash) const;

private:
  int attribute_;
  std::string sha1_;
  int size_;

  std::unique_ptr<std::string> buf_{};
  std::mutex buf_mutex_{};
  DISALLOW_COPY_AND_ASSIGN(FileElement);
};

class GitTree {
public:
  GitTree(const char* hash, const char* github_api_prefix,
	  directory_container::DirectoryContainer* c);
  ~GitTree();
};

} // namespace githubfs
#endif
