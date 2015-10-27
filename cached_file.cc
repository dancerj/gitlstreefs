/*
 * An implementation of a dumb file-backed cache.
 */

#include <assert.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>

#include "scoped_fd.h"

using std::string;
using std::function;
using std::unordered_map;

class Cache {
public:
  Cache(const string& cache_dir) : cache_dir_(cache_dir) {}
  // Get sha1 hash, and use fetch method to fetch if not available already.
  // TODO: Interface is awkward, obtain size and return size to caller.
  void* get(const string& name, size_t size, function<string()> fetch) {
    ScopedFd fd(open((cache_dir_ + name).c_str(), O_RDONLY));
    if (fd.get() == -1) {
      fd.reset(open((cache_dir_ + name).c_str(), O_RDWR | O_CREAT, 0666));
      if (fd.get() == -1) return nullptr;
      string result = fetch();
      write(fd.get(), result.data(), result.size());
    }
    void* m = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd.get(), 0);
    if (m == MAP_FAILED) {
      return nullptr;
    }
    mapped_files_[name] = m;
    return m;
  }
private:
  unordered_map<string, void*> mapped_files_{};
  const string cache_dir_;
};

static char kTestString[] = "HogeFuga";

int main(int argc, char** argv) {
  Cache c(".cache/");
  void* m = c.get("test1", 8, []() -> string { return string(kTestString); });
  string test(reinterpret_cast<char*>(m), 8);
  assert(test == kTestString);
  return 0;
}
