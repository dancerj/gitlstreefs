#include "cached_file.h"

#include <assert.h>

#include <iostream>
#include <string>

using std::string;

static char kTestString[] = "HogeFuga";

int main(int argc, char** argv) {
  std::cout << "Wait for lock." << std::endl;
  Cache c(".cache/");
  std::cout << "Start of main test." << std::endl;
  const Cache::Memory* m = c.get("test1", [](string* ret) -> bool {
      *ret = string(kTestString);
      return true;
    });
  const Cache::Memory* m2 = c.get("test1", [](string* ret) -> bool {
      return false;
    });
  string test(reinterpret_cast<const char*>(m->memory()), m->size());
  assert(test == kTestString);
  string test2(reinterpret_cast<const char*>(m2->memory()), m2->size());
  assert(test2 == kTestString);
  string test3(m2->memory_charp(), m2->size());
  assert(test3 == kTestString);
  assert(m2->get_copy() == kTestString);
  return 0;
}
