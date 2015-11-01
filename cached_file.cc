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

#include "cached_file.h"
#include "scoped_fd.h"

using std::string;
using std::function;
using std::unordered_map;

Cache::Memory::Memory(void* m, size_t s) : memory_(m), size_(s) {}

// Move constructor.
Cache::Memory::Memory(Cache::Memory&& m) : memory_(m.memory_), size_(m.size_) {
  m.memory_ = MAP_FAILED;
}

// Move assignment.
Cache::Memory& Cache::Memory::operator=(Cache::Memory&& m) {
  memory_ = m.memory_;
  size_ = m.size_;
  m.memory_ = MAP_FAILED;
  return *this;
}

Cache::Memory::~Memory() {
  if (memory_ != MAP_FAILED) {
    munmap(memory_, size_);
  }
}
const void* Cache::Memory::memory() const { return memory_; }
size_t Cache::Memory::size() const { return size_; }

Cache::Cache(const string& cache_dir) : cache_dir_(cache_dir) {}

// Get sha1 hash, and use fetch method to fetch if not available already.
const Cache::Memory* Cache::get(const string& name, function<string()> fetch) {
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
