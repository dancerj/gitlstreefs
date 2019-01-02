#include "base64decode.h"
#include "jsonparser.h"
#include "strutil.h"

#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <iostream>
#include <memory>
#include <string>

// This returns base64 data in text format.
// https://chromium.googlesource.com/chromiumos/third_party/kernel/+/chromeos-4.4/.gitignore?format=TEXT
std::string FetchBlob(const std::string& blob_text_string) {
  return base64decode(blob_text_string);
}

void FetchBlobTest() {
  std::string gitiles_blob(ReadFromFileOrDie(AT_FDCWD, "testdata/gitiles-blob.txt"));
  std::string fetched = FetchBlob(gitiles_blob);
  assert(fetched.size() == 1325);
  assert(fetched.find("NOTE!") != std::string::npos);
}

// Parses tree object from json, returns true on success.
//
// host_prject_branch_url needs to not end with a /. Should contain
// http://HOST/PROJECT/+/BRANCH that is used for the original tree
// request which should have been
// http://HOST/PROJECT/+/BRANCH/?format=JSON&recursive=TRUE&long=1
bool ParseTrees(const std::string host_project_branch_url,
		const std::string& trees_string,
		std::function<void(const std::string& path,
				   int mode,
				   const std::string& sha,
				   const int size,
				   const std::string& target,
				   const std::string& url)> file_handler) {
  // I assume the URL doesn't end at /.
  assert(host_project_branch_url[host_project_branch_url.size() - 1] != '/');

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

    /*
     * Gitiles API seems to require a commit revision+path for obtaining a blob.
     * according to https://github.com/google/gitiles/issues/51
     * Make sure we have one.
     */
    std::string name = file->get("name").get_string();
    std::string url = host_project_branch_url + "/" + name + "?format=TEXT";
    assert(file != nullptr);
    file_handler(name,
		 mode,
		 std::string(file->get("id").get_string()),
		 size,
		 target,
		 url);
  }
  return true;
}

void GitilesParserTest() {
  /*
    Things I tried:

    https://chromium.googlesource.com/chromiumos/third_party/kernel/+/chromeos-4.4?format=JSON
    -- not useful, gives information about the diff.

    https://chromium.googlesource.com/chromiumos/third_party/kernel/+/chromeos-4.4/?format=JSON
    -- gives a single tree.

    https://chromium.googlesource.com/chromiumos/third_party/kernel/+/chromeos-4.4/?format=JSON&recursive=TRUE&long=1
    -- Recursive tree that contains size also:

    Probably useful to read through:
    https://github.com/google/gitiles/blob/master/java/com/google/gitiles/GitilesFilter.java
    https://github.com/google/gitiles/blob/master/java/com/google/gitiles/TreeJsonData.java
  */
  std::string gitiles_trees(ReadFromFileOrDie(AT_FDCWD, "testdata/gitiles-tree-recursive.json"));

  ParseTrees("https://chromium.googlesource.com/chromiumos/third_party/kernel/+/chromeos-4.4",
	     gitiles_trees.substr(5), [](const std::string& path,
					 int mode,
					 const std::string& sha,
					 int size,
					 const std::string& target,
					 const std::string& url) {
	       std::cout << "gitiles:" << path << " " << std::oct << mode << " " << sha << " "
			 << std::dec << size
			 << (target.empty()?"":"->") << " " << target << " " << url << std::endl;
	     });
}

int main(int argc, char** argv) {
  GitilesParserTest();
  FetchBlobTest();
  return 0;
}
