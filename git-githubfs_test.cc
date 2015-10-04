#include <fstream>
#include <iostream>
#include <json_spirit.h>
#include <map>
#include <string>

#include "git-githubfs.h"
#include "strutil.h"

using json_spirit::Value;
using std::cout;
using std::endl;
using std::ifstream;
using std::map;
using std::string;
using githubfs::ParseTrees;
using githubfs::ParseCommits;

int main(int argc, char** argv) {
  // 1000 runs takes 1 second, mostly inside json_spirit.
  string commits(ReadFromFileOrDie("testdata/commits.json"));
  string trees(ReadFromFileOrDie("testdata/trees.json"));

  ParseTrees(trees);
  ParseCommits(commits);
}
