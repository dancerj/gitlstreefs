#include <assert.h>
#include <iostream>
#include <memory>
#include <string>
#include <sys/stat.h>

#include "get_current_dir.h"
#include "gitfs.h"

using std::cout;
using std::endl;
using std::string;
using std::unique_ptr;

void TryReadFileTest(gitfs::GitTree* fs, const string& name) {
  // Try reading a file.
  cout << "Try reading: " << name << endl;
  gitfs::FileElement* fe;;
  assert((fe = fs->get(name)) != nullptr);
  constexpr size_t size = 4096;
  char buf[size];
  size_t read_size;
  assert((read_size = fe->Read(buf, size, 0)) > 0);
  assert(read_size <= size);
  string f(buf, read_size);
  cout << f << endl;
}

void ScenarioTest() {
  auto fs = std::make_unique<gitfs::GitTree>("HEAD", GetCurrentDir());
  fs->dump();

  assert(fs->get("dummytestdirectory/README") != nullptr);
  assert(fs->get("dummytestdirectory") != nullptr);
  assert(fs->get("dummytestdirectory")->file_type_ == GIT_OBJ_TREE);
  cout << std::oct << fs->get("dummytestdirectory")->attribute_ << endl;

  // root directory.
  assert(fs->get("") != nullptr);
  struct stat st;
  assert(fs->Getattr("/", &st) == 0);
  assert(st.st_nlink == 2);
  assert(st.st_mode == (S_IFDIR | 0755));
  assert(fs->Getattr("/dummytestdirectory", &st) == 0);
  assert(fs->Getattr("/dummytestdirectory/", &st) == -ENOENT);
  assert(fs->Getattr("/dummytestdirectory/README", &st) == 0);
  TryReadFileTest(fs.get(), "dummytestdirectory/README");
}

int main(int argc, char** argv) {
  int iter = argv[1]?atoi(argv[1]):1;
  for (int i = 0; i < iter; ++i) {
    ScenarioTest();
  }
}
