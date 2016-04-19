#include <assert.h>
#include <stdio.h>

#include <string>

#include "cowfs_crypt.h"

using std::string;

int main() {
  init_gcrypt();
  assert(gcrypt_string("hello world")
	 == "2aae6c35c94fcfb415dbe95f408b9ce91ee846ed");

  string dir_name, file_name;
  gcrypt_string_get_git_style_relpath(&dir_name, &file_name,
				      "hello world");
  assert(dir_name == "2a");
  assert(file_name == "ae6c35c94fcfb415dbe95f408b9ce91ee846ed");
}
