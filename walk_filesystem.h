#ifndef WALK_FILESYSTEM_H_
#define WALK_FILESYSTEM_H_
#include <fts.h>

#include <functional>

bool WalkFilesystem(const std::string& dir,
                    std::function<void(FTSENT* entry)> cb) {
  // fts wants a mutable directory name, why?
  std::string mutable_dir(dir);
  char* const paths[] = {&mutable_dir[0], nullptr};
  FTS* f = fts_open(paths, FTS_PHYSICAL, nullptr /* use default ordering */);
  if (!f) {
    perror("fts_open");
    return false;
  }

  FTSENT* entry;
  while ((entry = fts_read(f)) != nullptr) {
    cb(entry);
  }
  if (errno) {
    perror("fts_read");
    fts_close(f);  // Ignore error here since we're already in error.
    return false;
  }
  if (fts_close(f) == -1) {
    perror("fts_close");
    return false;
  }
  return true;
}
#endif
