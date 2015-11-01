#include "cached_file.h"

#include <assert.h>

#include <string>

using std::string;

static char kTestString[] = "HogeFuga";

int main(int argc, char** argv) {
  Cache c(".cache/");
  const Cache::Memory* m = c.get("test1", []() -> string { return string(kTestString); });
  string test(reinterpret_cast<const char*>(m->memory()), m->size());
  assert(test == kTestString);
  return 0;
}
