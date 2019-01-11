#include "cached_file.h"

#include <assert.h>

#include <iostream>
#include <string>

using std::string;

static char kTestString[] = "HogeFuga";

int main(int argc, char** argv) {
  std::cout << "Wait for lock." << std::endl;
  Cache c("out/cached_file_test_cache/");
  std::cout << "Start of main test." << std::endl;
  const Cache::Memory* m = c.get("test1", [](string* ret) -> bool {
      *ret = string(kTestString);
      return true;
    });
  const Cache::Memory* m2 = c.get("test1", [](string* ret) -> bool {
      return false;
    });
  string test(m->memory_charp(), m->size());
  assert(test == kTestString);
  string test2(m2->memory_charp(), m2->size());
  assert(test2 == kTestString);
  assert(m2->get_copy() == kTestString);
  return 0;
}
