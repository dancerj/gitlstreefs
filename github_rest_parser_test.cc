#include <fstream>
#include <iostream>
#include <json_spirit.h>
#include <map>
#include <string>

using json_spirit::Value;
using std::cout;
using std::endl;
using std::ifstream;
using std::map;
using std::string;

const Value& GetObjectField(const json_spirit::Object& object, const string& name) {
  auto it = std::find_if(object.begin(), object.end(),
			 [&name](const json_spirit::Pair& a) -> bool{
			   return a.name_ == name;
			 });
  assert(it != object.end());
  return it->value_;
}

void ParseCommits() {
  // Try parsing github api v3 trees output.
  ifstream i("testdata/commits.json");
  Value commits;
  json_spirit::read(i, commits);
  for (const auto& commit : commits.get_array()) {
    string hash = GetObjectField(GetObjectField(GetObjectField(commit.get_obj(),
							       "commit").get_obj(),
						"tree").get_obj(),
				 "sha").get_str();
    cout << "hash: " << hash << endl;
  }
}

void ParseTrees() {
  // Try parsing github api v3 trees output.
  ifstream i("testdata/trees.json");
  Value value;
  json_spirit::read(i, value);
  for (const auto& tree : value.get_obj()) {
    // object is a vector of pair of key-value pairs.
    if (tree.name_== "tree") {
      for (const auto& file : tree.value_.get_array()) {
	// "path": ".gitignore",
	// "mode": "100644",
	// "type": "blob",
	// "sha": "0eca3e92941236b77ad23a02dc0c000cd0da7a18",
	// "size": 46,
	// "url": "https://api.github.com/repos/dancerj/gitlstreefs/git/blobs/0eca3e92941236b77ad23a02dc0c000cd0da7a18"
	map<string, const Value*> file_property;
	for (const auto& property : file.get_obj()) {
	  file_property[property.name_] = &property.value_;
	}
	cout << file_property["path"]->get_str() << " "
	     << file_property["mode"]->get_str() << " "
	     << file_property["type"]->get_str() << " "
	     << file_property["sha"]->get_str() << endl;
	if (file_property["type"]->get_str() == "blob") {
	  cout << "size: " << file_property["size"]->get_int() << endl;
	}
      }
      break;
    }
  }
}

int main(int argc, char** argv) {
  ParseTrees();
  ParseCommits();
}
