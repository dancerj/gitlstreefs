#include <assert.h>

#include "base64decode.h"

int main(int argc, char** argv) {
  assert(base64decode("aGVsbG8gd29ybGQK") == "hello world\n");
  assert(base64decode("Mg==") == "2");
  assert(base64decode("@#$%") == "");

  // Ignore whitespace characters.
  assert(base64decode("aGVsbG8gd29ybGQK\n") == "hello world\n");
  assert(base64decode("I2lmICFkZWZpbmVkKFNUUlVUSUxfSF9fKQojZGVmaW5lIFNUUlVUSUxfSF9f\nCiNpbmNsdWRlIDxzdHJpbmc+CnN0ZDo6c3RyaW5nIFJlYWRGcm9tRmlsZU9y\nRGllKGNvbnN0IHN0ZDo6c3RyaW5nJiBmaWxlbmFtZSk7CnN0ZDo6c3RyaW5n\nIFBvcGVuQW5kUmVhZE9yRGllKGNvbnN0IHN0ZDo6c3RyaW5nJiBjb21tYW5k\nLAoJCQkgICAgICBpbnQqIG1heWJlX2V4aXRfY29kZSA9IG51bGxwdHIpOwoj\nZW5kaWYK\n") == "#if !defined(STRUTIL_H__)\n#define STRUTIL_H__\n#include <string>\nstd::string ReadFromFileOrDie(const std::string& filename);\nstd::string PopenAndReadOrDie(const std::string& command,\n			      int* maybe_exit_code = nullptr);\n#endif\n");

}
