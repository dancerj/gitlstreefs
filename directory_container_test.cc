#include "directory_container.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <iostream>

#include "basename.h"

using std::function;
using std::map;
using std::string;
using std::unique_ptr;
using std::vector;
using std::pair;
using std::unordered_map;
using std::cout;
using std::endl;

// Concrete class.
class GitFile : public directory_container::File {
public:
  GitFile() {}
  virtual ~GitFile() {}
  virtual int Getattr(struct stat *stbuf) {
    return 0;
  }
};

int main() {
  directory_container::DirectoryContainer<GitFile> d;
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
  d.for_each("/hoge", [](const string& name, const directory_container::File* f) {
	cout << "under hoge: " << name << endl;
      });
}
