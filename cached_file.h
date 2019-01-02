#ifndef CACHED_FILE_H_
#define CACHED_FILE_H_
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

#include "disallow.h"
#include "scoped_fd.h"

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

  explicit Cache(const std::string& cache_dir);
  ~Cache();

  // Get sha1 hash, and use fetch method to fetch if not available already.
  const Memory* get(const std::string& name, std::function<bool(std::string*)> fetch);
  bool release(const std::string& name, const Memory* item);

  // Garbage collect old cache items.
  bool Gc();

private:
  void GetFileName(const std::string& key, std::string*, std::string*) const;

  std::unordered_map<std::string, Memory> mapped_files_{};
  std::mutex mutex_{};

  const std::string cache_dir_;
  // for directory.
  ScopedFd file_lock_;
  DISALLOW_COPY_AND_ASSIGN(Cache);
};

#endif
