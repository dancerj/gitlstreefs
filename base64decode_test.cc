#include <assert.h>

#include "base64decode.h"

std::string operator"" _b64(const char* str, std::size_t len) {
  std::string s(str, len);
  return base64decode(s);
}

int main(int argc, char** argv) {
  assert("aGVsbG8gd29ybGQK"_b64 == "hello world\n");
  assert("Mg=="_b64 == "2");
  assert("@#$%"_b64 == "");

  // Ignore whitespace characters.
  assert("aGVsbG8gd29ybGQK\n"_b64 == "hello world\n");
  assert(
      "I2lmICFkZWZpbmVkKFNUUlVUSUxfSF9fKQojZGVmaW5lIFNUUlVUSUxfSF9f\nCiNpbmNsdWRlIDxzdHJpbmc+CnN0ZDo6c3RyaW5nIFJlYWRGcm9tRmlsZU9y\nRGllKGNvbnN0IHN0ZDo6c3RyaW5nJiBmaWxlbmFtZSk7CnN0ZDo6c3RyaW5n\nIFBvcGVuQW5kUmVhZE9yRGllKGNvbnN0IHN0ZDo6c3RyaW5nJiBjb21tYW5k\nLAoJCQkgICAgICBpbnQqIG1heWJlX2V4aXRfY29kZSA9IG51bGxwdHIpOwoj\nZW5kaWYK\n"_b64 ==
      R"(#if !defined(STRUTIL_H__)
#define STRUTIL_H__
#include <string>
std::string ReadFromFileOrDie(const std::string& filename);
std::string PopenAndReadOrDie(const std::string& command,
			      int* maybe_exit_code = nullptr);
#endif
)");

  // Make sure === works.
  assert("A===\n"_b64.size() == 0);
  assert("AA==\n"_b64.size() == 1);
  assert("AAA=\n"_b64.size() == 2);
  assert("AAAA\n"_b64.size() == 3);
}
