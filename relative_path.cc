#include "relative_path.h"

#include <string>
#include <assert.h>

using std::string;

string GetRelativePath(const char* path) {
  // Input is /absolute/path/below
  // convert to a relative path.

  assert(*path != 0);

  if(path[1] == 0) {
    // special-case / ? "" isn't a good relative path.
    return "./";
  }
  return path + 1;
}
