#include "directory_container.h"

#include "basename.h"

#include <assert.h>

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>


using std::cout;
using std::endl;
using std::function;
using std::map;
using std::pair;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::vector;

// Concrete class.
class GitFile : public directory_container::File {
public:
  GitFile() {}
  virtual ~GitFile() {}
  virtual int Getattr(struct stat *stbuf) override {
    return 0;
  }
  virtual ssize_t Read(char *buf, size_t size, off_t offset) override {
    return -EINVAL;
  }
  virtual int Open() override {
    return -EINVAL;
  };
  virtual int Release() override {
    return -EINVAL;
  };
};

int main() {
  directory_container::DirectoryContainer d;
  d.add("/this/dir", std::make_unique<GitFile>());
  d.add("/the", std::make_unique<GitFile>());
  d.add("/a", std::make_unique<GitFile>());
  d.add("/hoge/bbb", std::make_unique<GitFile>());
  d.add("/hoge/ccc", std::make_unique<GitFile>());
  d.add("/foo/bbbdir/ccc", std::make_unique<GitFile>());
  d.add("/hoge/bbbdir/ccc", std::make_unique<GitFile>());

  d.dump();

  assert(d.is_directory("/this"));
  assert(d.is_directory("/hoge"));
  assert(d.is_directory("/"));
  assert(!d.is_directory("/hog"));
  assert(!d.is_directory("/a"));

  assert(!d.is_directory("/hoge/ccc"));
  assert(d.get("/hoge/ccc"));

  int count_hoge = 0;
  d.for_each("/hoge", [&count_hoge](const string& name, const directory_container::File* f) {
	cout << "under hoge: " << name << endl;
	count_hoge ++;
      });
  assert(count_hoge == 3);
}
