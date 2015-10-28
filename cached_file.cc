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
  // Holds mmap result and frees it if necessary.
  class Memory {
  public:
    Memory(void* m, size_t s) : memory_(m), size_(s) {}

    // Move constructor.
    Memory(Memory&& m) : memory_(m.memory_), size_(m.size_) {
      m.memory_ = MAP_FAILED;
    }
    // Move assignment.
    Memory& operator=(Memory&& m) {
      memory_ = m.memory_;
      size_ = m.size_;
      m.memory_ = MAP_FAILED;
      return *this;
    }

    ~Memory() {
      if (memory_ != MAP_FAILED) {
	munmap(memory_, size_);
      }
    }
    const void* memory() const { return memory_; }
    size_t size() const { return size_; }

  private:
    void* memory_;
    size_t size_;
    DISALLOW_COPY_AND_ASSIGN(Memory);
  };
  Cache(const string& cache_dir) : cache_dir_(cache_dir) {}

  // Get sha1 hash, and use fetch method to fetch if not available already.
  const Memory* get(const string& name, function<string()> fetch) {
    // Check if we've already mapped the cache to memory.
    auto it = mapped_files_.find(name);
    if (it != mapped_files_.end()) {
      return &it->second;
    }

    // Try if we've cached to file.
    ScopedFd fd(open((cache_dir_ + name).c_str(), O_RDONLY));
    if (fd.get() == -1) {
      // Populate cache.
      fd.reset(open((cache_dir_ + name).c_str(), O_RDWR | O_CREAT, 0666));
      if (fd.get() == -1) return nullptr;
      string result = fetch();
      write(fd.get(), result.data(), result.size());
    }
    struct stat stbuf;
    fstat(fd.get(), &stbuf);
    size_t size = stbuf.st_size;
    void* m = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd.get(), 0);
    if (m == MAP_FAILED) {
      return nullptr;
    }
    auto emplace_result = mapped_files_.emplace(name, Memory(m, size));
    return &emplace_result.first->second;
  }

private:
  unordered_map<string, Memory> mapped_files_{};
  const string cache_dir_;
  DISALLOW_COPY_AND_ASSIGN(Cache);
};

static char kTestString[] = "HogeFuga";

int main(int argc, char** argv) {
  Cache c(".cache/");
  const Cache::Memory* m = c.get("test1", []() -> string { return string(kTestString); });
  string test(reinterpret_cast<const char*>(m->memory()), m->size());
  assert(test == kTestString);
  return 0;
}
