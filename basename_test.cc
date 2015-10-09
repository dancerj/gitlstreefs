#include <assert.h>

#include "basename.h"

void TestBaseName() {
  assert(BaseName("/hoge/fuga") == "fuga");
  assert(BaseName("/hoge/fuga/moge") == "moge");
  assert(BaseName("moge") == "moge");
}

void TestDirName() {
  assert(DirName("/hoge/fuga") == "/hoge/");
  assert(DirName("/hoge/fuga/moge") == "/hoge/fuga/");
  assert(DirName("/moge") == "/");
}

int main(int argc, char** argv) {
  TestBaseName();
  TestDirName();
  return 0;
}
