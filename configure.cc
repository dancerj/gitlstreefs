#include "configure.h"

int main() {
  std::string fuse_cflags =
      NinjaBuilder::PopenAndReadOrDie("pkg-config fuse3 --cflags");
  std::string fuse_libs =
      NinjaBuilder::PopenAndReadOrDie("pkg-config fuse3 --libs");
  NinjaBuilder::Config config;
  config.cxxflags =
      std::string(
          "-O2 -g --std=c++17 -Wall -Werror -D_FILE_OFFSET_BITS=64 -I. ") +
      fuse_cflags;
  config.ldflags = std::string("-pthread ") + fuse_libs;

  NinjaBuilder n(config);
  n.CclinkRule("cclinkwithgit2", "$gxx $in -o $out -lgit2 $ldflags");
  n.CclinkRule("cclinkcowfs", "$gxx $in -o $out -lgcrypt $ldflags");

  n.CompileLinkRunTest(
      "gitlstree_test",
      {"basename", "cached_file", "concurrency_limit", "directory_container",
       "get_current_dir", "git_cat_file", "gitlstree", "gitlstree_test",
       "scoped_timer", "stats_holder", "strutil"},
      {"out/fetch_test_repo.sh.result"});
  n.CompileLink(
      "gitlstree",
      {"basename", "cached_file", "concurrency_limit", "directory_container",
       "get_current_dir", "git_adapter", "git_cat_file", "gitlstree",
       "gitlstree_fusemain", "scoped_timer", "stats_holder", "strutil"});

  n.CompileLinkRunTest("strutil_test", {"strutil", "strutil_test"});
  n.CompileLink("ninjafs", {"basename", "directory_container",
                            "get_current_dir", "ninjafs", "strutil"});
  n.RunTestScript("ninjafs_test.sh", {"out/ninjafs"});
  n.CompileLink("hello_world", {"hello_world"});
  n.CompileLinkRunTest("basename_test", {"basename_test", "basename"});
  n.CompileLinkRunTest(
      "git-githubfs_test",
      {"base64decode", "basename", "cached_file", "concurrency_limit",
       "directory_container", "get_current_dir", "git-githubfs_test",
       "git-githubfs", "jsonparser", "scoped_timer", "stats_holder",
       "strutil"});
  n.CompileLink("git-githubfs",
                {"base64decode", "basename", "cached_file", "concurrency_limit",
                 "directory_container", "get_current_dir", "git_adapter",
                 "git-githubfs_fusemain", "git-githubfs", "jsonparser",
                 "scoped_timer", "stats_holder", "strutil"});
  n.CompileLinkRunTest("concurrency_limit_test",
                       {"concurrency_limit_test", "concurrency_limit"});
  n.CompileLinkRunTest(
      "directory_container_test",
      {"directory_container", "directory_container_test", "basename"});

  n.CompileLink("git_ioctl_client", {"git_ioctl_client"});
  n.CompileLinkRunTest("scoped_fd_test", {"scoped_fd_test"});
  n.CompileLinkRunTest("cached_file_test",
                       {"cached_file", "cached_file_test", "stats_holder"});
  n.CompileLink("cached_file_util", {
                                        "cached_file",
                                        "cached_file_util",
                                        "stats_holder",
                                    });
  n.CompileLinkRunTest("base64decode_test",
                       {"base64decode", "base64decode_test"});
  n.CompileLinkRunTest("base64decode_benchmark",
                       {"base64decode", "base64decode_benchmark", "strutil"});
  n.CompileLinkRunTest("scoped_fileutil_test",
                       {"scoped_fileutil_test", "scoped_fileutil"});
  n.CompileLinkRunTest("jsonparser_test", {"jsonparser_test", "jsonparser"});
  n.CompileLink("jsonparser_util",
                {"jsonparser_util", "jsonparser", "strutil"});
  n.CompileLinkRunTest("scoped_timer_test",
                       {"scoped_timer", "scoped_timer_test", "stats_holder"});
  n.CompileLinkRunTest("gitiles_test", {"base64decode", "gitiles_test",
                                        "jsonparser", "strutil"});
  n.CompileLinkRunTest("git_cat_file_test",
                       {"get_current_dir", "git_cat_file", "git_cat_file_test",
                        "scoped_timer", "stats_holder", "strutil"});
  n.RunTestScript("fetch_test_repo.sh");
  n.CompileLink("cowfs", {"cowfs", "cowfs_crypt", "file_copy", "ptfs",
                          "ptfs_handler", "relative_path", "scoped_fileutil",
                          "strutil", "update_rlimit"})
      .Cclink("cclinkcowfs");
  n.CompileLinkRunTest("cowfs_crypt_test", {"cowfs_crypt", "cowfs_crypt_test"})
      .Cclink("cclinkcowfs");
  n.RunTestScript("cowfs_test.sh", {"out/cowfs", "out/hello_world"});
  n.CompileLink("ptfs", {"ptfs_main", "ptfs", "ptfs_handler", "relative_path",
                         "scoped_fileutil", "strutil", "update_rlimit"});
  n.RunTestScript("ptfs_test.sh", {"out/ptfs"});
  n.CompileLink("file_copy_test", {"file_copy", "file_copy_test"});

  // Experimental code.
  n.CompileLink("experimental/gitfs",
                {"experimental/gitfs", "experimental/gitfs_fusemain", "strutil",
                 "get_current_dir", "experimental/gitxx", "basename"})
      .Cclink("cclinkwithgit2");
  n.CompileLinkRunTest(
       "experimental/gitfs_test",
       {"experimental/gitfs", "experimental/gitfs_test", "strutil",
        "get_current_dir", "experimental/gitxx", "basename"},
       {"out/fetch_test_repo.sh.result"})
      .Cclink("cclinkwithgit2");
  n.CompileLinkRunTest("experimental/libgit2test",
                       {"experimental/libgit2test", "experimental/gitxx"},
                       {"out/fetch_test_repo.sh.result"})
      .Cclink("cclinkwithgit2");
  n.CompileLink("experimental/hello_fuseflags",
                {"experimental/hello_fuseflags"});
  n.CompileLink("experimental/unkofs",
                {"experimental/unkofs", "experimental/roptfs", "relative_path",
                 "update_rlimit"});
  n.RunTestScript("experimental/unkofs_test.sh", {"out/experimental/unkofs"});
  n.CompileLink("experimental/globfs",
                {"experimental/globfs", "experimental/roptfs", "relative_path",
                 "update_rlimit"});
  n.RunTestScript("experimental/globfs_test.sh", {"out/experimental/globfs"});
  n.CompileLink("experimental/parallel_writer",
                {"experimental/parallel_writer"});
}
