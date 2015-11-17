#include <assert.h>

#include <string>
#include <vector>

using std::string;
using std::vector;

static const char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
constexpr int64_t kEmptyChar = -1;

string base64decode(const string& b64) {
  string output;
  vector<int64_t> lookup(256);
  size_t i;
  for (auto& v: lookup) { v = kEmptyChar; }
  for (i = 0; i < sizeof(kAlphabet); ++i) {
    lookup[kAlphabet[i]] = i;
  }

  for (size_t position = 0; position < b64.size(); ) {
    uint64_t result = 0;
    int nchars = -1;  // valid input chars that are not '=', 4 valid
		      // input chars should give us 3 output chars.
    for (int j = 0; j < 4 && position < b64.size(); ++j) {
      int64_t t = 0;
      do {
	t = lookup[b64[position++]];
      } while (t == kEmptyChar && position < b64.size());
      if (t != kEmptyChar) {
	result |= (t << ((3 - j) * 6));
	nchars++;
      }
    }
    for (int j = 0; j < nchars; ++j) {
      char c = char((result >> ((2 - j) * 8)) & 255);
      output += c;
    }
  }
  return output;
}
