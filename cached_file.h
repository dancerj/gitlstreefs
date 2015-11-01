#ifndef __CACHED_FILE_H__
#define __CACHED_FILE_H__
#include <functional>
#include <string>
#include <unordered_map>

#include "disallow.h"

class Cache {
public:
  // Holds mmap result and frees it if necessary.
  class Memory {
  public:
    Memory(void* m, size_t s);

    // Move constructor.
    Memory(Memory&& m);
    // Move assignment.
    Memory& operator=(Memory&& m);
    ~Memory();
    const void* memory() const;
    size_t size() const;

  private:
    void* memory_;
    size_t size_;
    DISALLOW_COPY_AND_ASSIGN(Memory);
  };

  Cache(const std::string& cache_dir);

  // Get sha1 hash, and use fetch method to fetch if not available already.
  const Memory* get(const std::string& name, std::function<std::string()> fetch);

private:
  std::unordered_map<std::string, Memory> mapped_files_{};
  const std::string cache_dir_;
  DISALLOW_COPY_AND_ASSIGN(Cache);
};

#endif
