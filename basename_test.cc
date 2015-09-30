#include <assert.h>

#include "basename.h"

int main(int argc, char** argv) {
  assert(BaseName("/hoge/fuga") == "fuga");
  assert(BaseName("/hoge/fuga/moge") == "moge");
  assert(BaseName("moge") == "moge");
  return 0;
}
