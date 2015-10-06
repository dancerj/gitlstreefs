#include <fstream>
#include <iostream>
#include <json_spirit.h>
#include <string>
#include <sys/stat.h>
#include <memory>

#include "git-githubfs.h"
#include "strutil.h"

using githubfs::GitFileType;
using githubfs::ParseBlob;
using githubfs::ParseCommits;
using githubfs::ParseTrees;
using json_spirit::Value;
using std::cout;
using std::endl;
using std::string;
using std::unique_ptr;

void ParserTest() {
  // 1000 runs takes 1 second, mostly inside json_spirit.
  string commits(ReadFromFileOrDie("testdata/commits.json"));
  string trees(ReadFromFileOrDie("testdata/trees.json"));
  string blob(ReadFromFileOrDie("testdata/blob.json"));

  ParseCommits(commits);
  ParseTrees(trees, [](const string& path,
		       int mode,
		       const GitFileType fstype,
		       const string& sha,
		       const int size,
		       const string& url){
	       cout << path << " " << mode << " " << fstype << " " << sha << " " << size << " " << url << endl;
	     });
  string ret = ParseBlob(blob);
  cout << "blob content: " << ret << endl;
}

void TryReadFileTest(githubfs::GitTree* fs, const string& name) {
  // Try reading a file.
  cout << "Try reading: " << name << endl;
  githubfs::FileElement* fe;;
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
  unique_ptr<githubfs::GitTree> fs(new githubfs::GitTree("HEAD", 
							 "https://api.github.com/repos/dancerj/gitlstreefs"));
  fs->dump();

  assert(fs->get("dummytestdirectory/README") != nullptr);
  assert(fs->get("dummytestdirectory") != nullptr);
  assert(fs->get("dummytestdirectory")->file_type_ == githubfs::TYPE_tree);
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

  // TODO: This obtains a JSON response not the actual content.
  TryReadFileTest(fs.get(), "dummytestdirectory/README");
}

int main(int argc, char** argv) {
  ParserTest();
  int iter = argv[1]?atoi(argv[1]):0;
  for (int i = 0; i < iter; ++i) {
    // TODO: This uses up quota, so don't run by default.
    ScenarioTest();
  }
}
