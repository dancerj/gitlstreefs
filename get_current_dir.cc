#include "get_current_dir.h"

#include <unistd.h>

#include <string>

using std::string;

string GetCurrentDir() {
  char* s = get_current_dir_name();
  string ss(s);
  free(s);
  return ss;
}
