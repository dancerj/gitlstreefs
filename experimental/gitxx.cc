#include <functional>
#include <memory>
#include <string>

#include <git2.h>

#include "gitxx.h"

using std::function;
using std::string;
using std::unique_ptr;

namespace gitxx {

namespace {
class GitSingleton {
public:
  GitSingleton();
  ~GitSingleton();
};

GitSingleton::GitSingleton() {
#if LIBGIT2_SOVERSION == 21
  git_threads_init();
#else // 22
  git_libgit2_init();
#endif
}
GitSingleton::~GitSingleton() {
#if LIBGIT2_SOVERSION == 21
  git_threads_shutdown();
#else // 22
  git_libgit2_shutdown();
#endif
}
static GitSingleton git_singleton;
} // anonymous namespace

Tree::Tree(const Repository* repo, git_tree* tree) : repo_(repo), tree_(tree) {}
Tree::~Tree() { git_tree_free(tree_); }

void Tree::for_each_file(function<void(const string& name, int attribute, git_otype file_type, const string& sha1, int size, unique_ptr<Object> )> file_handler,
			 function<void(const string& name, Tree* tree)> tree_handler) const {
  size_t i, max_i = (int)git_tree_entrycount(tree_);
  char oidstr[GIT_OID_HEXSZ + 1];
  const git_tree_entry *te;

  for (i = 0; i < max_i; ++i) {
    te = git_tree_entry_byindex(tree_, i);

    git_oid_tostr(oidstr, sizeof(oidstr), git_tree_entry_id(te));
    size_t size = 0;
    git_object* object;
    git_tree_entry_to_object(&object, repo_->repo, te);
    unique_ptr<Object> cxx_object;
    if (GIT_OBJ_BLOB == git_tree_entry_type(te)) {
      cxx_object.reset(new Object(repo_, object));
      size = cxx_object->size();
    }

    string name(git_tree_entry_name(te));
    file_handler(name, git_tree_entry_filemode(te), git_tree_entry_type(te), oidstr, size, move(cxx_object));

    // Recurse subdirectories.
    if (GIT_OBJ_TREE == git_tree_entry_type(te)) {
      git_object* new_o;
      git_tree_entry_to_object(&new_o, repo_->repo, te);
      unique_ptr<Tree> t(new Tree(repo_, reinterpret_cast<git_tree*>(new_o)));
      tree_handler(name, t.get());
    }
  }
}

void Tree::dump(const string& path_prepend) const {
  for_each_file([&](const string& name, int attribute, git_otype file_type, const string& sha1, int size, unique_ptr<Object> object){
      // File
      printf("%06o %s %s\t%s%s\n",
	     attribute,
	     git_object_type2string(file_type),
	     sha1.c_str(),
	     path_prepend.c_str(),
	     name.c_str());
    }, [&](const string& name, Tree* subtree){
      // Subdirectory
      subtree->dump(path_prepend + name + "/");
    });
}

Object::Object(const Repository* repo, const string& rev) :
  repo_(repo) {
  git_revparse_single(&obj_, repo_->repo,
		      rev.c_str());
}
Object::Object(const Repository* repo, git_object* object) :
  obj_(object),
  repo_(repo)  {
}

Object::~Object() {
  if (obj_) {
    git_object_free(obj_);
  }
}

size_t Object::size() const {
  if (GIT_OBJ_BLOB != git_object_type(obj_)) {
    return 0;
  }
  const git_blob* blob = reinterpret_cast<git_blob*>(obj_);
  return git_blob_rawsize(blob);
}

string Object::GetBlobContent() const {
  if (GIT_OBJ_BLOB != git_object_type(obj_)) {
    return "";
  }
  const git_blob* blob = reinterpret_cast<git_blob*>(obj_);
  string blobcontent(reinterpret_cast<const char*>(git_blob_rawcontent(blob)),
		     (size_t)git_blob_rawsize(blob));
  return blobcontent;
}

unique_ptr<Tree> Object::GetTreeFromCommit() const {
  const git_commit* commit = reinterpret_cast<git_commit*>(obj_);
  git_tree* tree;
  git_commit_tree(&tree, commit);
  return unique_ptr<Tree>(new Tree(repo_, tree));
}

Repository::Repository(const string& repo_path) {
  git_repository_open_ext(&repo, repo_path.c_str(), 0, nullptr);
}
Repository::~Repository() {
  git_repository_free(repo);
}

// Obtain object matching the revision.
unique_ptr<Object> Repository::GetRevision(const string& rev) {
  return unique_ptr<Object>(new Object(this, rev));
}
} // namespace gitxx
