#include "cached_file.h"

#include <assert.h>
#include <fts.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace {
bool walk(const std::string& dir, std::function<void(FTSENT* entry)> cb) {
  // fts wants a mutable directory name, why?
  std::string mutable_dir(dir);
  char * const paths[] = { &mutable_dir[0], nullptr };
  FTS *f = fts_open(paths, FTS_PHYSICAL,
		    NULL /* use default ordering */);
  if (!f) {
    perror("fts_open");
    return false;
  }

  FTSENT* entry;
  while((entry = fts_read(f)) != NULL) {
    cb(entry);
  }
  if (errno) {
    perror("fts_read");
    return false;
  }
  if (fts_close(f) == -1) {
    perror("fts_close");
    return false;
  }
  return true;
}
}  // anonymous namespace

bool Gc() {
  time_t now = time(nullptr);
  std::vector<std::string> to_delete{};
  assert(walk(".cache", [&to_delete, now](FTSENT* entry) {
      if (entry->fts_info == FTS_F) {
	std::string path(entry->fts_path, entry->fts_pathlen);
	std::string name(entry->fts_name, entry->fts_namelen);
	struct stat* st = entry->fts_statp;
	time_t delta = now - st->st_atime;
	std::cout << path <<
	  " atime_delta_days:" << (delta / 60 / 60 / 24) << std::endl;
	// .cache/bf/82c3eab3768308dfe445c7f8a314858cec09e0
	if (name.size() == 38 && (delta / 60 / 60 / 24) > 60) {
	  // This is probably a cache file, and is probably hasn't been used for a while
	  to_delete.push_back(path);
	}
      }
    }));
  for (const auto& path : to_delete) {
    std::cout << "garbage collect old files : " << path << std::endl;
    if (-1 == unlink(path.c_str())) {
      perror(path.c_str());
      return false;
    }
  }
  return true;
}

int main(int argc, char** argv) {
  if (Gc()) {
    exit(EXIT_SUCCESS);
  } else {
    exit (EXIT_FAILURE);
  }
}
