// content-based cowdancer-like file system.
//
// At mount time it would:
// - hard-links same file based on content
// - gc to eliminate files no longer referenced.
//
// While mounted it would:
// - try to unlink the hardlinks before modification.
#define FUSE_USE_VERSION 26

#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <fuse.h>
#include <sys/file.h>

#include <boost/filesystem.hpp>
#include <iostream>
#include <string>

#include "cowfs_crypt.h"
#include "disallow.h"
#include "file_copy.h"
#include "relative_path.h"
#include "scoped_fd.h"

namespace fs = boost::filesystem; // std::experimental::filesystem;
using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::vector;

// Directory before mount.
int premount_dirfd = -1;

class ScopedRelativeFileFd {
public:
  ScopedRelativeFileFd(const char* path, int mode) : fd_(-1), errno_(0) {
    if (*path == 0) {
      errno_ = ENOENT;
      return;
    }
    string relative_path(GetRelativePath(path));
    fd_ = openat(premount_dirfd, relative_path.c_str(), mode);
    if (fd_ == -1) {
      errno_ = errno;
    }
  }

  ~ScopedRelativeFileFd() {
    if (fd_ != -1) {
      close(fd_);
    }
  }

  int get() const { return fd_; }
  int get_errno() const { return errno_; }
private:
  int fd_;
  int errno_;
};

class ScopedLock {
public:
  ScopedLock(const string& path, const string& message)
    : have_lock_(GetLock(path, message)) {}
  ~ScopedLock() {
    if (fd_ != -1) {
      if (close(fd_) == -1) {
	perror("close lock");
      }
    }
  }
  bool have_lock() const {
    return have_lock_;
  }

  bool GetLock(const string& path, const string& message) {
    fd_ = open(path.c_str(), O_CLOEXEC|O_CREAT|O_RDWR, 0777);
    if (fd_ == -1) {
      perror("open lockfile");
      return false;
    }
    int lock_result = flock(fd_, LOCK_EX);
    if (lock_result != 0) {
      perror("flock");
      return false;
    }
    if (-1 == ftruncate(fd_, 0)) {
      perror("ftruncate");
      return false;
    }
    // For debugging, dump a message about who's locking this for what.
    if (-1 == write(fd_, message.c_str(), message.size())) {
      perror("write"); return false;
    }
    return true;
  }
private:
  int fd_;
  bool have_lock_;
  DISALLOW_COPY_AND_ASSIGN(ScopedLock);
};

void GcTree(const string& repo) {
  cout << "GCing things we don't need" << endl;
  auto end = fs::recursive_directory_iterator();
  vector<string> to_delete{};
  for(auto it = fs::recursive_directory_iterator(repo); it != end; ++it) {
    // it points to a directory_entry().
    fs::path p(*it);
    if (fs::is_regular_file(p) && !fs::is_symlink(p)) {
      // cout << p.string() << " nlink=" << fs::hard_link_count(p) << endl;
      if (fs::hard_link_count(p) == 1) {
	// This is a stale file
	to_delete.emplace_back(p.string());
      }
    }
  }
  for (const auto& s: to_delete) {
    if (-1 == unlink(s.c_str())) {
      cout << s << " is no longer needed " << endl;
      perror((string("unlink ") + s).c_str());
    }
  }
}

string ReadFile(int dirfd, const string& filename) {
  ScopedFd fd(openat(dirfd, filename.c_str(), O_RDONLY));
  assert(fd.get() != -1);
  struct stat s;
  string buf;
  assert(-1 != fstat(fd.get(), &s));
  buf.resize(s.st_size);
  assert(s.st_size == read(fd.get(), &buf[0], s.st_size));
  return buf;
}

// Not using dirfd.
bool HardlinkOneFile(int dirfd_from, const string& from,
		     int dirfd_to, const string& to) {
  string to_tmp(to + ".tmp");
  if (-1 == linkat(dirfd_from, from.c_str(), dirfd_to, to_tmp.c_str(), 0)) {
    perror("linkat");
    return false;
  }
  if (-1 == renameat(dirfd_to, to_tmp.c_str(), dirfd_to, to.c_str())) {
    perror("renameat");
    return false;
  }
  return true;
}

bool MaybeBreakHardlink(int dirfd, const string& target) {
  string to_tmp(target + ".tmp");

  {
    ScopedFd from_fd(openat(dirfd, target.c_str(), O_RDONLY, 0));

    if (from_fd.get() == -1) {
      if (errno == ENOENT) {
	// File not existing is okay, O_CREAT maybe specified.
	return true;
      }
      perror("open failed and not ENOENT");
      return false;
    }
    // TODO: O_TRUNC might be an optimization.
    struct stat st{};
    if (-1 == fstat(from_fd.get(), &st)) {
      // I wonder why fstat can fail here.
      perror("fstat");
      return false;
    }

    // 0 is not an expected value, does the file system support hardlink?
    if (st.st_nlink == 0) {
      cout << "0 hardlink doesn't sound like a good filesystem." << endl;
      return false;
    }

    if (st.st_nlink == 1) {
      // I don't need to break links if count is 1.
      return true;
    }
    FileCopyInternal(dirfd, from_fd.get(), st, to_tmp);
  }
  // TODO: update metadata.

  if (-1 == renameat(dirfd, to_tmp.c_str(), dirfd, target.c_str())) {
    perror("renameat");
    return false;
  }
  return true;
}

bool FindOutRepoAndMaybeHardlink(int target_dirfd, const string& target_filename,
				 const string& repo) {
  string repo_dir_name, repo_file_name;
  gcrypt_string_get_git_style_relpath(&repo_dir_name, &repo_file_name,
				      ReadFile(target_dirfd, target_filename));
  string repo_file_path(repo + "/" + repo_dir_name + "/" + repo_file_name);
  struct stat st;
  if (lstat(repo_file_path.c_str(), &st) == -1 && errno == ENOENT) {
    // If it doesn't exist, we hardlink to there.
    // First try to make subdirectory if it doesn't exist.
    // TODO: what's a reasonable umask for this repo?
    if (mkdir((repo + "/" + repo_dir_name).c_str(), 0700) == -1) {
      if (errno != EEXIST) return false;
    }
    if (!HardlinkOneFile(target_dirfd, target_filename, AT_FDCWD, repo_file_path))
      return false;
  } else {
    // Hardlink from repo.
    if (!HardlinkOneFile(AT_FDCWD, repo_file_path, target_dirfd, target_filename))
      return false;
  }
  return true;
}

void HardlinkTree(const string& repo, const string& directory) {
  cout << "Hardlinking files we do need" << endl;

  auto end = fs::recursive_directory_iterator();
  for(auto it = fs::recursive_directory_iterator(directory); it != end; ++it) {
    // it points to a directory_entry().
    fs::path p(*it);
    if (fs::is_regular_file(p) && !fs::is_symlink(p)) {
      if (fs::hard_link_count(p) == 1) {
	assert(FindOutRepoAndMaybeHardlink(AT_FDCWD, p.string(), repo));
      }
    }
  }
}

#define WRAP_ERRNO(f)				\
  if(-1 == f) {					\
    return -errno;				\
  } else{					\
    return 0;					\
  }
#define WRAP_ERRNO_OR_RESULT(T, f)				\
  T res = f;							\
  if(-1 == res) {						\
    return -errno;						\
  } else {							\
    return res;							\
  }

static int fs_getattr(const char *path, struct stat *stbuf) {
  memset(stbuf, 0, sizeof(struct stat));
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));

  WRAP_ERRNO(fstatat(premount_dirfd, relative_path.c_str(),
		     stbuf, AT_SYMLINK_NOFOLLOW));
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi) {
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));

  // Directory would contain . and ..
  // filler(buf, ".", NULL, 0);
  // filler(buf, "..", NULL, 0);
  struct dirent **namelist{nullptr};
  int scandir_count = scandirat(premount_dirfd,
				relative_path.c_str(),
				&namelist,
				nullptr,
				nullptr);
  if (scandir_count == -1) {
    return -ENOENT;
  }
  for(int i = 0; i < scandir_count; ++i) {
    filler(buf, namelist[i]->d_name, 0, 0);
    free(namelist[i]);
  }
  free(namelist);
  return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi) {
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));

  if ((fi->flags & O_ACCMODE) != O_RDONLY) {
    // Break hardlink on open if necessary.
    if (!MaybeBreakHardlink(premount_dirfd, path)) {
      return -EIO;
    }
  }
  int fd = openat(premount_dirfd, relative_path.c_str(),
		  fi->flags,
		  0666 /* mknod should have been called already? */);
  if (fd == -1)
    return -ENOENT;
  fi->fh = static_cast<uint64_t>(fd);

  return 0;
}

static int fs_read(const char *path, char *target, size_t size, off_t offset,
		   struct fuse_file_info *fi) {
  int fd = fi->fh;
  if (fd == -1)
    return -ENOENT;

  WRAP_ERRNO_OR_RESULT(ssize_t, pread(fd, target, size, offset));
}

static int fs_write(const char *path, const char *buf, size_t size,
		    off_t offset, struct fuse_file_info *fi) {
  int fd = fi->fh;
  if (fd == -1)
    return -ENOENT;

  WRAP_ERRNO_OR_RESULT(ssize_t, pwrite(fd, buf, size, offset));
}

static int fs_release(const char *path, struct fuse_file_info *fi) {
  int fd = fi->fh;
  if (fd == -1)
    return -EBADF;
  if (*path == 0)
    return -EBADF;
  string relative_path(GetRelativePath(path));

  int ret = close(fd);
  if (-1 == ret) ret = -errno;

  // TODO: Dedup

  return ret;
}

static int fs_mknod(const char *path, mode_t mode, dev_t rdev) {
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  WRAP_ERRNO(mknodat(premount_dirfd, relative_path.c_str(), mode, rdev));
}

static int fs_chmod(const char *path, mode_t mode)
{
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  WRAP_ERRNO(fchmodat(premount_dirfd,
   		      relative_path.c_str(), mode, 0));
}

static int fs_chown(const char *path, uid_t uid, gid_t gid)
{
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  WRAP_ERRNO(fchownat(premount_dirfd,
		      relative_path.c_str(), uid, gid, AT_SYMLINK_NOFOLLOW));
}

static int fs_utimens(const char *path, const struct timespec ts[2]) {
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  WRAP_ERRNO(utimensat(premount_dirfd,
		       relative_path.c_str(), ts, AT_SYMLINK_NOFOLLOW));
}

static int fs_truncate(const char *path, off_t size) {
  ScopedRelativeFileFd fd(path, O_WRONLY);
  if (fd.get() == -1) {
    return -fd.get_errno();
  }
  WRAP_ERRNO(ftruncate(fd.get(), size));
}

static int fs_unlink(const char *path) {
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  WRAP_ERRNO(unlinkat(premount_dirfd, relative_path.c_str(), 0));
}

static int fs_rename(const char *from, const char *to) {
  if (*from == 0)
    return -ENOENT;
  if (*to == 0)
    return -ENOENT;
  string from_s(GetRelativePath(from));
  string to_s(GetRelativePath(to));
  WRAP_ERRNO(renameat(premount_dirfd, from_s.c_str(), premount_dirfd, to_s.c_str()));
}

static int fs_mkdir(const char *path, mode_t mode) {
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  WRAP_ERRNO(mkdirat(premount_dirfd, relative_path.c_str(), mode));
}

static int fs_rmdir(const char *path) {
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  WRAP_ERRNO(unlinkat(premount_dirfd, relative_path.c_str(), AT_REMOVEDIR));
}

static int fs_readlink(const char *path, char *buf, size_t size) {
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  int res;
  if ((res = readlinkat(premount_dirfd,
			relative_path.c_str(), buf, size - 1)) == -1) {
    return -errno;
  } else {
    buf[res] = '\0';
    return 0;
  }
}

static int fs_symlink(const char *from, const char *to) {
  if (*from == 0)
    return -ENOENT;
  if (*to == 0)
    return -ENOENT;
  string to_s(GetRelativePath(to));
  WRAP_ERRNO(symlinkat(from, premount_dirfd, to_s.c_str()));
}

static int fs_link(const char *from, const char *to) {
  if (*from == 0)
    return -ENOENT;
  if (*to == 0)
    return -ENOENT;
  string from_s(GetRelativePath(from));
  string to_s(GetRelativePath(to));
  WRAP_ERRNO(linkat(premount_dirfd, from_s.c_str(),
 		    premount_dirfd, to_s.c_str(), 0));
}

static int fs_statfs(const char *path, struct statvfs *stbuf) {
  WRAP_ERRNO(fstatvfs(premount_dirfd, stbuf));
}

struct cowfs_config {
  char* repository{nullptr};
  char* lock_path{nullptr};
  char* underlying_path{nullptr};
};

#define MYFS_OPT(t, p, v) { t, offsetof(cowfs_config, p), v }

static struct fuse_opt cowfs_opts[] = {
  MYFS_OPT("--repository=%s", repository, 0),
  MYFS_OPT("--lock_path=%s", lock_path, 0),
  MYFS_OPT("--underlying_path=%s", underlying_path, 0),
  FUSE_OPT_END
};
#undef MYFS_OPT

int main(int argc, char** argv) {

  struct fuse_operations o = {};
#define DEFINE_HANDLER(n) o.n = &fs_##n
  DEFINE_HANDLER(chmod);
  DEFINE_HANDLER(chown);
  DEFINE_HANDLER(getattr);
  DEFINE_HANDLER(link);
  DEFINE_HANDLER(mkdir);
  DEFINE_HANDLER(mknod);
  DEFINE_HANDLER(open);
  DEFINE_HANDLER(read);
  DEFINE_HANDLER(readdir);
  DEFINE_HANDLER(readlink);
  DEFINE_HANDLER(release);
  DEFINE_HANDLER(rename);
  DEFINE_HANDLER(rmdir);
  DEFINE_HANDLER(statfs);
  DEFINE_HANDLER(symlink);
  DEFINE_HANDLER(truncate);
  DEFINE_HANDLER(unlink);
  DEFINE_HANDLER(utimens);
  DEFINE_HANDLER(write);
  // DEFINE_HANDLER(fsync);
  // DEFINE_HANDLER(fallocate);
  // DEFINE_HANDLER(setxattr);
  // DEFINE_HANDLER(getxattr);
  // DEFINE_HANDLER(listxattr);
  // DEFINE_HANDLER(removexattr);
#undef DEFINE_HANDLER

  fuse_args args = FUSE_ARGS_INIT(argc, argv);
  cowfs_config conf{};
  fuse_opt_parse(&args, &conf, cowfs_opts, NULL);

  if (!conf.underlying_path || !conf.lock_path || !conf.repository) {
    cerr << argv[0]
	 << " [mountpoint] --lock_path= --underlying_path= --repository= "
	 << endl;
    return 1;
  }
  assert(conf.underlying_path);
  assert(conf.lock_path);
  ScopedLock fslock(conf.lock_path, "cowfs");
  GcTree(conf.repository);
  HardlinkTree(conf.repository, conf.underlying_path);
  premount_dirfd = open(conf.underlying_path, O_PATH|O_DIRECTORY);
  assert(premount_dirfd != -1);

  int ret = fuse_main(args.argc, args.argv, &o, NULL);
  fuse_opt_free_args(&args);
  return ret;
}
