#include "strutil.h"

#include <assert.h>

void test_PopenAndReadOrDie2() {
  // Check that basic input/output is correct.
  assert(PopenAndReadOrDie2({"echo", "hello", "world"}) == "hello world\n");

  // Check that size of larger data is correct.
  assert(PopenAndReadOrDie2(
             {"dd", "if=/dev/zero", "count=10", "bs=512", "status=none"})
             .size() == 5120);
}

void test_SplitStringUsing() {
  auto res = SplitStringUsing("hoge  fuga c onaaaaaaaaxx", ' ', true);
  assert(res.size() == 4);
  assert(res[0] == "hoge");
  assert(res[1] == "fuga");
  assert(res[2] == "c");
  assert(res[3] == "onaaaaaaaaxx");
}

int main(int argc, char** argv) {
  test_PopenAndReadOrDie2();
  test_SplitStringUsing();
  return 0;
}
