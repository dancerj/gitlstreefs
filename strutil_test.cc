#include "strutil.h"

#include <assert.h>

void test_PopenAndReadOrDie2() {
  // Check that basic input/output is correct.
  assert(PopenAndReadOrDie2({"echo", "hello", "world"})
	 == "hello world\n");

  // Check that size of larger data is correct.
  assert(PopenAndReadOrDie2({"dd", "if=/dev/zero", "count=10", "bs=512", "status=none"}).size()
	 == 5120);
}

int main(int argc, char** argv) {
  test_PopenAndReadOrDie2();
  return 0;
}
