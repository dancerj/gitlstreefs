#include "base64decode.h"

#include <assert.h>

#include <array>
#include <string>

#include "disallow.h"

using std::array;
using std::string;

namespace {
static constexpr int64_t kEmptyChar = -1;
class Base64Decoder {
 public:
  Base64Decoder() {
    constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    lookup_.fill(kEmptyChar);
    for (size_t i = 0; i < sizeof(kAlphabet); ++i) {
      lookup_[kAlphabet[i]] = i;
    }
  }
  ~Base64Decoder() {}

  string doit(const string& b64) const {
    string output;
    output.reserve(b64.size() * 0.7);

    for (size_t position = 0; position < b64.size();) {
      uint64_t result = 0;
      int j;
      for (j = 0; j < 4 && position < b64.size(); ++j) {
        int64_t t = 0;
        do {
          t = lookup_[b64[position++]];
        } while (t == kEmptyChar && position < b64.size());
        if (t != kEmptyChar) {
          result |= (t << ((3 - j) * 6));
        } else {
          break;
        }
      }
      for (int k = 0; k < j - 1; ++k) {
        char c = char((result >> ((2 - k) * 8)) & 255);
        output += c;
      }
    }
    return output;
  }

 private:
  array<int64_t, 256> lookup_{};
  DISALLOW_COPY_AND_ASSIGN(Base64Decoder);
};

static Base64Decoder b;
}  // anonymous namespace

string base64decode(const string& b64) { return b.doit(b64); }
