#include <assert.h>
#include <gcrypt.h>

#include <string>

#include "cowfs_crypt.h"
using std::string;

static string hex_string_representation(unsigned char* buf, size_t len) {
  string result;
  char h[] = "0123456789abcdef";
  for (size_t i = 0; i < len; ++i) {
    result += h[buf[i] / 16];
    result += h[buf[i] % 16];
  }
  return result;
}

string gcrypt_string(const string& buf) {
  gcry_md_hd_t hd;
  assert(0 == gcry_md_open(&hd, GCRY_MD_SHA1, 0));
  gcry_md_write(hd, buf.c_str(), buf.size());
  unsigned char* result = gcry_md_read(hd, GCRY_MD_SHA1);
  string result_string(hex_string_representation(result, 20));
  gcry_md_close(hd);
  return result_string;
}

void gcrypt_string_get_git_style_relpath(string* dir_name, string* file_name,
					 const string& buf) {
  string b(gcrypt_string(buf));
  *dir_name = b.substr(0,2);
  *file_name = b.substr(2);
}
