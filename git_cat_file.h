#ifndef GIT_CAT_FILE_H
#define GIT_CAT_FILE_H

#include <mutex>
#include <vector>
#include <string>

#include "disallow.h"

namespace GitCatFile {

// Metadata for response from git-cat-file daemon.
class GitCatFileMetadata {
public:
  explicit GitCatFileMetadata(const std::string& header);
  ~GitCatFileMetadata();

  // The header size, including the terminating newline.
  int first_line_size_{-1};

  // The size of the message content.
  int size_{-1};
  std::string sha1_{};
  std::string type_{};
};

// Internal utility class for bidirectional pipe for daemon process.
class BidirectionalPopen {
public:
  BidirectionalPopen(const std::vector<std::string>& command,
		     const std::string* cwd);
  ~BidirectionalPopen();
  void Write(const std::string& s) const;
  std::string Read(int max_size) const;
private:
  int read_fd_{-1};
  int write_fd_{-1};
  pid_t pid_{-1};

  DISALLOW_COPY_AND_ASSIGN(BidirectionalPopen);
};

class GitCatFileProcess {
public:
  explicit GitCatFileProcess(const std::string* cwd);
  ~GitCatFileProcess();

  std::string Request(const std::string& ref) const;
  struct ObjectNotFoundException {};

private:
  const BidirectionalPopen process_;
  mutable std::mutex m_;
};
}

#endif
