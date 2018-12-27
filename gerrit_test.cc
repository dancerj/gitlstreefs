#include "jsonparser.h"
#include "strutil.h"

#include <fcntl.h>

#include <iostream>
#include <memory>
#include <string>

// Parses tree object from json, returns false if it was truncated and
// needs retry.
bool ParseTrees(const std::string& trees_string, std::function<void(const std::string& path,
								    int mode,
								    const std::string& sha)> file_handler) {
  // Try parsing github api v3 trees output.
  std::unique_ptr<jjson::Value> value = jjson::Parse(trees_string);

  for (const auto& file : (*value)["entries"].get_array()) {
    // "mode": 33188,
    // "type": "blob",
    // "id": "0eca3e92941236b77ad23a02dc0c000cd0da7a18",
    // "name": ".gitignore",

    file_handler(file->get("name").get_string(),
		 file->get("mode").get_int(),
		 file->get("id").get_string());
  }
  return true;
}

void GerritParserTest() {
  /*
    Try something like:
   https://chromium.googlesource.com/chromiumos/third_party/kernel/+/chromeos-4.4?format=JSON  // not useful, gives a diff?
   https://chromium.googlesource.com/chromiumos/third_party/kernel/+/chromeos-4.4/?format=JSON  // tree.
   https://chromium.googlesource.com/chromiumos/third_party/kernel/+/chromeos-4.4/?format=JSON&recursive=TRUE // retursive tree.
  does not include size.

  pp=0

  documentation at https://review.openstack.org/Documentation/rest-api.html#output
  */
  std::string gerrit_trees(ReadFromFileOrDie(AT_FDCWD, "testdata/gerrit-tree-recursive.json"));

  ParseTrees(gerrit_trees.substr(5), [](const std::string& path,
		       int mode,
		       const std::string& sha){
	       std::cout << "gerrit:" << path << " " << std::oct << mode << " " << sha << " " << std::endl;
	     });
}

int main(int argc, char** argv) {
  GerritParserTest();
  return 0;
}
