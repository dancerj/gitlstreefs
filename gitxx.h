#ifndef __GITXX_H__
#define __GITXX_H__
#include <git2.h>

#include "disallow.h"

namespace gitxx {
class Repository;
class Object;

// Representation of a Tree.
class Tree {
public:
  Tree(const Repository* repo, git_tree* tree);
  ~Tree();
  void for_each_file(std::function<void(const std::string& name, 
					int attribute, 
					git_otype file_type, 
					const std::string& sha1, 
					int size, 
					std::unique_ptr<Object> object)> file_handler,
		     std::function<void(const std::string& name,
					Tree* tree)> tree_handler) const;
  void dump(const std::string& path_prepend) const;
private:
  const Repository* repo_;
  git_tree *tree_;

  DISALLOW_COPY_AND_ASSIGN(Tree);
};

// Representation of a Git object, such as blob...
class Object {
public:
  Object(const Repository* repo, const std::string& rev);
  Object(const Repository* repo, git_object* object);
  ~Object();
  std::string GetBlobContent() const;
  size_t size() const;
  std::unique_ptr<Tree> GetTreeFromCommit() const;

private:
  git_object *obj_;
  const Repository* repo_;
  DISALLOW_COPY_AND_ASSIGN(Object);
};

class Repository {
public:
  explicit Repository(const std::string& repo_path);
  ~Repository();

  // Obtain object matching the revision.
  std::unique_ptr<Object> GetRevision(const std::string& rev);
private:
  friend Object;
  friend Tree;
  git_repository *repo;
  DISALLOW_COPY_AND_ASSIGN(Repository);
};
}  // namespace gitxx
#endif
