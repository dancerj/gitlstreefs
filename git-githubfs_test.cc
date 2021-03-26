#include "git-githubfs.h"

#include "get_current_dir.h"
#include "strutil.h"

#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

using githubfs::GitFileType;
using githubfs::ParseBlob;
using githubfs::ParseCommit;
using githubfs::ParseCommits;
using githubfs::ParseTrees;
using std::cout;
using std::endl;
using std::string;
using std::unique_ptr;
using std::unordered_map;

namespace {

#define TYPE(a) \
  { GitFileType::a, #a }
static unordered_map<GitFileType, string> file_type_to_string_map{
    TYPE(blob), TYPE(tree), TYPE(commit)};
#undef TYPE

void ParserTest() {
  // 1000 runs takes 1 second, mostly inside json_spirit.
  string commits(ReadFromFileOrDie(AT_FDCWD, "testdata/commits.json"));
  string commit(ReadFromFileOrDie(AT_FDCWD, "testdata/commit.json"));
  string trees(ReadFromFileOrDie(AT_FDCWD, "testdata/trees.json"));
  string blob(ReadFromFileOrDie(AT_FDCWD, "testdata/blob.json"));

  ParseCommits(commits);
  ParseCommit(commit);
  ParseTrees(trees, [](const string& path, int mode, const GitFileType fstype,
                       const string& sha, const int size, const string& url) {
    cout << path << " " << mode << " " << file_type_to_string_map[fstype] << " "
         << sha << " " << size << " " << url << endl;
  });
  string ret = ParseBlob(blob);
  cout << "blob content: " << ret << endl;
}

void TryReadFileTest(directory_container::DirectoryContainer* container,
                     const string& name) {
  // Try reading a file.
  cout << "Try reading: " << name << endl;
  githubfs::FileElement* fe;
  assert((fe = dynamic_cast<githubfs::FileElement*>(
              container->mutable_get(name))) != nullptr);
  fe->Open();
  constexpr size_t size = 4096;
  char buf[size];
  size_t read_size;
  assert((read_size = fe->Read(buf, size, 0)) > 0);
  assert(read_size <= size);
  string f(buf, read_size);
  cout << f << endl;
  fe->Release();
}

void ScenarioTest() {
  auto container = std::make_unique<directory_container::DirectoryContainer>();
  auto fs = std::make_unique<githubfs::GitTree>(
      "HEAD", "https://api.github.com/repos/dancerj/gitlstreefs",
      container.get(), GetCurrentDir() + "/.cache/");
  container->dump();

  assert(container->get("/dummytestdirectory/README") != nullptr);
  assert(container->get("/dummytestdirectory") != nullptr);
  assert(container->is_directory("/dummytestdirectory"));

  // root directory.
  assert(container->get("/") != nullptr);
  struct stat st;
  assert(container->Getattr("/", &st) == 0);
  assert(st.st_nlink == 2);
  assert(st.st_mode == (S_IFDIR | 0755));
  assert(container->Getattr("/dummytestdirectory", &st) == 0);
  assert(container->Getattr("/dummytestdirectory/", &st) == -ENOENT);
  assert(container->Getattr("/dummytestdirectory/README", &st) == 0);

  // TODO: This obtains a JSON response not the actual content.
  TryReadFileTest(container.get(), "/dummytestdirectory/README");
}

}  // namespace

int main(int argc, char** argv) {
  ParserTest();
  int iter = argv[1] ? atoi(argv[1]) : 0;
  for (int i = 0; i < iter; ++i) {
    // TODO: This uses up quota, so don't run by default.
    ScenarioTest();
  }
}
