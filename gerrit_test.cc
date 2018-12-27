#include "jsonparser.h"
#include "strutil.h"

#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <iostream>
#include <memory>
#include <string>

// Parses tree object from json, returns false if it was truncated and
// needs retry.
bool ParseTrees(const std::string& trees_string, std::function<void(const std::string& path,
								    int mode,
								    const std::string& sha,
								    const int size,
								    const std::string& target)> file_handler) {
  // Try parsing github api v3 trees output.
  std::unique_ptr<jjson::Value> value = jjson::Parse(trees_string);

  for (const auto& file : (*value)["entries"].get_array()) {
    // "mode": 33188,  // an actual number.
    // "type": "blob",
    // "id": "0eca3e92941236b77ad23a02dc0c000cd0da7a18",
    // "name": ".gitignore",
    // "size": 1325
    // size can be none if mode is 40960, in which case 'target' contains the symlink target.
    mode_t mode = file->get("mode").get_int();

    int size = 0;
    std::string target{};
    if (S_ISLNK(mode)) {
      target = file->get("target").get_string();
    } else {
      size = file->get("size").get_int();
    }

    assert(file != nullptr);
    file_handler(std::string(file->get("name").get_string()),
		 mode,
		 std::string(file->get("id").get_string()),
		 size,
		 target);
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

  long=1
  Apparently I got it wrong; this component is called gitiles.

  pp=0 -- doesn't seem to do anything.

  documentation at https://review.openstack.org/Documentation/rest-api.html#output is something different?

  https://github.com/google/gitiles/blob/master/java/com/google/gitiles/GitilesFilter.java
  https://github.com/google/gitiles/blob/master/java/com/google/gitiles/TreeJsonData.java
  */
  std::string gerrit_trees(ReadFromFileOrDie(AT_FDCWD, "testdata/gerrit-tree-recursive.json"));

  ParseTrees(gerrit_trees.substr(5), [](const std::string& path,
					int mode,
					const std::string& sha,
					int size, 
					const std::string& target) {
	       std::cout << "gerrit:" << path << " " << std::oct << mode << " " << sha << " "
			 << std::dec << size
			 << (target.empty()?"":"->") << " " << target << std::endl;
	     });
}

int main(int argc, char** argv) {
  GerritParserTest();
  return 0;
}
