#include "cached_file.h"

#include <assert.h>

#include <string>

using std::string;

static char kTestString[] = "HogeFuga";

int main(int argc, char** argv) {
  Cache c(".cache/");
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
  assert(test == kTestString);
  return 0;
}
