/*
 * An implementation of a dumb file-backed cache.
 */
#include "cached_file.h"

#include <assert.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>

#include "scoped_fd.h"

using std::function;
using std::mutex;
using std::string;
using std::unique_lock;
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

Cache::Cache(const string& cache_dir) : cache_dir_(cache_dir) {
  file_lock_ = open((cache_dir + "lock").c_str(),
		    O_CREAT|O_RDWR|O_CLOEXEC, 0700);
  // TODO maybe not crashing and giving better error message is
  // better.
  assert(file_lock_ != -1);
  assert(flock(file_lock_, LOCK_EX) != -1);
}

Cache::~Cache() {
  assert(flock(file_lock_, LOCK_UN) != -1);
  close(file_lock_);
}

void Cache::GetFileName(const string& name,
			string* dir_name, string* file_name) const {
  *dir_name = cache_dir_ + name.substr(0, 2);
  *file_name = name.substr(2);
}

// Get sha1 hash, and use fetch method to fetch if not available already.
const Cache::Memory* Cache::get(const string& name,
				function<bool(string*)> fetch) {
  unique_lock<mutex> l(mutex_);
  // Check if we've already mapped the cache to memory.
  auto it = mapped_files_.find(name);
  if (it != mapped_files_.end()) {
    return &it->second;
  }

  string cache_file_dir;
  string cache_file_name;
  GetFileName(name, &cache_file_dir, &cache_file_name);
  string cache_file_path(cache_file_dir + "/" + cache_file_name);
  if (-1 == mkdir(cache_file_dir.c_str(), 0700) && (errno != EEXIST)) {
    perror((string("mkdir ") + cache_file_dir).c_str());
    return nullptr;
  }
  // Try if we've cached to file.
  ScopedFd fd(open(cache_file_path.c_str(), O_RDONLY));
  if (fd.get() == -1) {
    // Populate cache.
    string result;
    l.unlock();
    // This is RPC that may take arbitrary amount of time, we
    // shouldn't be blocking others. TODO: handle multiple requests to
    // one cache entry.
    if (!fetch(&result)) {
      // Uncached fetching failed.
      return nullptr;
    }
    l.lock();

    string temporary(cache_file_path + ".tmp");
    fd.reset(open(temporary.c_str(), O_RDWR | O_CREAT, 0666));
    if (fd.get() == -1) {
      perror((string("open ") + temporary).c_str());
      return nullptr;
    }
    assert(result.size() ==
	   static_cast<size_t>(write(fd.get(), result.data(), result.size())));
    assert(-1 != rename(temporary.c_str(), cache_file_path.c_str()));
  }
  struct stat stbuf;
  fstat(fd.get(), &stbuf);
  size_t size = stbuf.st_size;
  void* m = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd.get(), 0);
  if (m == MAP_FAILED) {
    perror(("mmap " + cache_file_path).c_str());
    return nullptr;
  }
  auto emplace_result = mapped_files_.emplace(name, Memory(m, size));
  return &emplace_result.first->second;
}

bool Cache::release(const string& name, const Cache::Memory* item) {
  // Delete the name, assert that the value was item.
  unique_lock<mutex> l(mutex_);

  auto it = mapped_files_.find(name);
  if (it != mapped_files_.end()) {
    return false;
  }
  assert(&it->second == item);
  mapped_files_.erase(it);
  return true;
}
