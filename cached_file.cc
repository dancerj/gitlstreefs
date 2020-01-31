/*
 * An implementation of a dumb file-backed cache.
 */
#include "cached_file.h"
#include "stats_holder.h"
#include "walk_filesystem.h"

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
#include <vector>

#include "scoped_fd.h"

using std::function;
using std::lock_guard;
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
size_t Cache::Memory::size() const { return size_; }
const char* Cache::Memory::memory_charp() const {
  return static_cast<const char*>(memory_);
}
std::string Cache::Memory::get_copy() const {
  return std::string(memory_charp(), size_);
}

Cache::Cache(const string& cache_dir) :
  cache_dir_(cache_dir),
  file_lock_(-1) {
  if (-1 == mkdir(cache_dir_.c_str(), 0700) && errno != EEXIST) {
    perror((string("Cannot create cache dir ") + cache_dir).c_str());
    abort();
  }
  assert(cache_dir_[cache_dir_.size() - 1] == '/');
  string lock_file_name{cache_dir + "lock"};
  file_lock_.reset(open(lock_file_name.c_str(),
			O_CREAT|O_RDWR|O_CLOEXEC, 0700));
  if (file_lock_.get() == -1) {
    perror((string("Cannot open ") + lock_file_name).c_str());
    abort();
  }
  assert(flock(file_lock_.get(), LOCK_EX) != -1);
  assert(cache_dir_[cache_dir_.size()-1] == '/');
}

Cache::~Cache() {
  assert(flock(file_lock_.get(), LOCK_UN) != -1);
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
    // TODO: This is RPC that may take arbitrary amount of time, we
    // shouldn't be blocking others. However it does not properly:
    // handle multiple requests to one cache entry.

    // l.unlock();
    if (!fetch(&result)) {
      // Uncached fetching failed.
      std::cout << "Uncached fetching failed: " << name << std::endl;
      return nullptr;
    }
    // l.lock();

    string temporary(cache_file_path + ".tmp");
    unlink(temporary.c_str());  // Make sure the file does not exist.
    {
      fd.reset(open(temporary.c_str(), O_RDWR | O_CREAT | O_EXCL, 0666));
      if (fd.get() == -1) {
	perror((string("open ") + temporary).c_str());
	return nullptr;
      }
      assert(result.size() ==
	     static_cast<size_t>(write(fd.get(), result.data(), result.size())));
    }
    assert(-1 != rename(temporary.c_str(), cache_file_path.c_str()));
  }
  struct stat stbuf;
  assert(0==fstat(fd.get(), &stbuf));
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
  lock_guard<mutex> l(mutex_);

  auto it = mapped_files_.find(name);
  if (it != mapped_files_.end()) {
    return false;
  }
  assert(&it->second == item);
  mapped_files_.erase(it);
  return true;
}

namespace {
}  // anonymous namespace

bool Cache::Gc() {
  lock_guard<mutex> l(mutex_);

  time_t now = time(nullptr);
  std::vector<std::string> to_delete{};
  stats_holder::StatsHolder stats;
  assert(WalkFilesystem(cache_dir_, [&to_delete, now, &stats](FTSENT* entry) {
      if (entry->fts_info == FTS_F) {
	std::string path(entry->fts_path, entry->fts_pathlen);
	std::string name(entry->fts_name, entry->fts_namelen);
	struct stat* st = entry->fts_statp;
	time_t delta = now - st->st_atime;
	/*
	   std::cout << path <<
	   " atime_delta_days:" << (delta / 60 / 60 / 24) <<
	   " size:" << st->st_size << std::endl;
	*/
	stats.Add("size", st->st_size);
	stats.Add("age", delta);
	// .cache/bf/82c3eab3768308dfe445c7f8a314858cec09e0
	if (name.size() == 38 && (delta / 60 / 60 / 24) > 60) {
	  // This is probably a cache file, and is probably hasn't been used for a while
	  to_delete.emplace_back(path);
	}
      }
    }));
  for (const auto& path : to_delete) {
    std::cout << "garbage collect old files : " << path << std::endl;
    if (-1 == unlink(path.c_str())) {
      perror(path.c_str());
      return false;
    }
  }
  std::cout << stats.Dump() << std::endl;
  return true;
}
