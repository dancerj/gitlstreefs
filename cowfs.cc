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
#include <sys/sysinfo.h>
#include <syslog.h>

#include <boost/filesystem.hpp>
#include <iostream>
#include <map>
#include <string>
#include <thread>

#include "cowfs_crypt.h"
#include "disallow.h"
#include "file_copy.h"
#include "relative_path.h"
#include "scoped_fd.h"
#include "strutil.h"

namespace fs = boost::filesystem; // std::experimental::filesystem;
using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::thread;
using std::to_string;
using std::vector;

namespace {
// Directory before mount.
int premount_dirfd = -1;
string repository_path;
}  // anonymous namespace

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

class ScopedTempFile {
public:
  ScopedTempFile(int dirfd, const string& basename, const string& opt) :
    dirfd_(dirfd),
    name_(basename + ".tmp" + opt + to_string(pthread_self())) {
    if (-1 == unlinkat(dirfd, name_.c_str(), 0) && errno != ENOENT) {
      syslog(LOG_ERR, "unlinkat %s %m", name_.c_str());
      abort();  // Probably a race condition?
    }
  }
  ~ScopedTempFile() {
    if (name_.size() > 0) {
      if (-1 == unlinkat(dirfd_, name_.c_str(), 0)) {
	syslog(LOG_ERR, "unlinkat tmpfile %s %m", name_.c_str());
	abort();   // Logic error somewhere or a race condition.
      }
    }
  }
  void clear() {
    name_.clear();
  };
  const string& get() const { return name_; };
  const char* c_str() const { return name_.c_str(); };
private:
  int dirfd_;
  string name_;
};

bool HardlinkOneFile(int dirfd_from, const string& from,
		     int dirfd_to, const string& to) {
  ScopedTempFile to_tmp(dirfd_to, to, "of");
  if (-1 == linkat(dirfd_from, from.c_str(), dirfd_to,
		   to_tmp.c_str(), 0)) {
    syslog(LOG_ERR, "linkat %m");
    return false;
  }
  if (-1 == renameat(dirfd_to, to_tmp.c_str(), dirfd_to, to.c_str())) {
    syslog(LOG_ERR, "renameat %m");
    return false;
  }
  to_tmp.clear();
  return true;
}

// Return empty on error.
string GetRepoItemPath(int dirfd, string relative_path) {
  string buf, repo_dir_name, repo_file_name;
  if (!ReadFromFile(dirfd, relative_path, &buf)) {
    return "";
  }
  gcrypt_string_get_git_style_relpath(&repo_dir_name, &repo_file_name, buf);
  return repository_path  + "/" + repo_dir_name + "/" + repo_file_name;
}

bool GarbageCollectOneRepoFile(const string& repo_file_path) {
  struct stat st;
  if (lstat(repo_file_path.c_str(), &st) != -1 && st.st_nlink == 1) {
    if (-1 == unlink(repo_file_path.c_str())) {
      if (errno == ENOENT) {
	// There was a parallel thread that deleted the file.
	return true;
      }
      syslog(LOG_ERR, "unlink garbage collection %s %m", repo_file_path.c_str());
      return false;
    }
    cout << "Garbage collected repo file " << repo_file_path << endl;
  }
  return true;
}

bool MaybeGcAfterHardlinkBreakForTarget(int dirfd, const string& target) {
  // Now, the file in the repository might be the only copy of the
  // data. Remove it if so, that we don't need to wait until global GC.
  // TODO: repository-critical section.

  // TODO: I'm reading the file into memory to get sha1sum after
  // having used sendfile to copy it; is that efficient?
  string repo_file_path(GetRepoItemPath(premount_dirfd, target));
  if (repo_file_path.size() == 0) {
    // soft-fail?
    return false;
  }
  return GarbageCollectOneRepoFile(repo_file_path);
}

// Called on open, which may end up modifying the content of the
// file. We will un-dedupe the files so that one file can be modified
// while leaving the other intact.
bool MaybeBreakHardlink(int dirfd, const string& target) {
  ScopedFd from_fd(openat(dirfd, target.c_str(), O_RDONLY, 0));

  if (from_fd.get() == -1) {
    if (errno == ENOENT) {
      // File not existing is okay, O_CREAT maybe specified.
      return true;
    }
    syslog(LOG_ERR, "open failed and not ENOENT: %m");
    return false;
  }
  // TODO: O_TRUNC might be an optimization.
  struct stat st{};
  if (-1 == fstat(from_fd.get(), &st)) {
    // I wonder why fstat can fail here.
    syslog(LOG_ERR, "fstat %s %m", target.c_str());
    return false;
  }

  // 0 is not an expected value, does the file system support hardlink?
  // TODO: this may happen if someone removed the file on another thread.
  if (st.st_nlink == 0) {
    syslog(LOG_ERR, "0 hardlink doesn't sound like a good filesystem for %s.",
	   target.c_str());
    return false;
  }

  if (st.st_nlink == 1) {
    // I don't need to break links if count is 1.
    return true;
  }

  ScopedTempFile to_tmp(dirfd, target, "mb");
  if (!FileCopyInternal(dirfd, from_fd.get(), st, to_tmp.get())) {
    int errno_from_copy = errno;
    // Copy did not succeed?
    syslog(LOG_ERR, "Copy failed %s %s %m", target.c_str(), to_tmp.c_str());
    to_tmp.clear();
    if (errno_from_copy == ENOENT) {
      // It's okay if someone else removed this file in the interim.
      return true;
    } else {
      return false;
    }
  }
  from_fd.clear();

  // Rename the new file to target location.
  if (-1 == renameat(dirfd, to_tmp.c_str(), dirfd, target.c_str())) {
    syslog(LOG_ERR, "renameat %s -> %s %m", to_tmp.c_str(), target.c_str());
    // if (errno == ENOENT) {
    //   // It's okay if someone else removed this file in the interim.
    //   return true;
    // }
    return false;
  }
  to_tmp.clear();

  if (!MaybeGcAfterHardlinkBreakForTarget(dirfd, target)) {
    return false;
  }
  return true;
}

// This part may run as daemon, error failure is not visible.
bool FindOutRepoAndMaybeHardlink(int target_dirfd, const string& target_filename,
				 const string& repo) {
  string buf, repo_dir_name, repo_file_name;
  if (!ReadFromFile(target_dirfd, target_filename, &buf)) {
    syslog(LOG_ERR, "Can't read from %s", target_filename.c_str());
    // Can't read from file.
    return false;
  }
  gcrypt_string_get_git_style_relpath(&repo_dir_name, &repo_file_name, buf);
  string repo_file_path(repo + "/" + repo_dir_name + "/" + repo_file_name);
  struct stat st;
  if (lstat(repo_file_path.c_str(), &st) == -1 && errno == ENOENT) {
    // If it doesn't exist, we hardlink to there.
    // First try to make subdirectory if it doesn't exist.
    // TODO: what's a reasonable umask for this repo?
    if (mkdir((repo + "/" + repo_dir_name).c_str(), 0700) == -1) {
      if (errno != EEXIST) {
	syslog(LOG_ERR, "Can't create directory %s %m", repo_dir_name.c_str());
	return false;
      }
    }
    if (!HardlinkOneFile(target_dirfd, target_filename, AT_FDCWD, repo_file_path))
      return false;
    // syslog(LOG_DEBUG, "New file %s", target_filename.c_str());
  } else {
    // Hardlink from repo.
    if (!HardlinkOneFile(AT_FDCWD, repo_file_path, target_dirfd, target_filename))
      return false;
    // syslog(LOG_DEBUG, "Deduped %s", repo_file_path.c_str());
  }
  return true;
}

// This is an offline process at startup not running as a daemon, so
// this can fail with an error message.
void HardlinkTree(const string& repo, const string& directory) {
  cout << "Hardlinking files we do need" << endl;
  int ncpu = get_nprocs();
  vector<vector<string> > to_hardlink(ncpu);
  int cpu = 0;
  auto end = fs::recursive_directory_iterator();
  for(auto it = fs::recursive_directory_iterator(directory); it != end; ++it) {
    // it points to a directory_entry().
    fs::path p(*it);
    if (fs::is_regular_file(p) && !fs::is_symlink(p)) {
      if (fs::hard_link_count(p) == 1) {
	to_hardlink[cpu].emplace_back(p.string());
	++cpu;
	cpu %= ncpu;
      }
    }
  }
  vector<thread> jobs;
  for (int i = 0; i < ncpu; ++i) {
    jobs.emplace_back(thread(bind([i, &repo](const vector<string>* tasks){
	    for (const auto& s: *tasks) {
	      assert(FindOutRepoAndMaybeHardlink(AT_FDCWD, s, repo));
	    }
	    cout << "task " << i << " with " << tasks->size() << " tasks" << endl;
	  }, &to_hardlink[i])));
  }
  for (auto& job : jobs) { job.join(); }
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
    if (!MaybeBreakHardlink(premount_dirfd, relative_path)) {
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
  bool mutable_access = ((fcntl(fd, F_GETFL) & O_ACCMODE) != O_RDONLY);

  int ret = close(fd);
  if (-1 == ret) ret = -errno;

  if (mutable_access) {
    assert(repository_path.size() > 0);
    string relative_path(GetRelativePath(path));
    if (!FindOutRepoAndMaybeHardlink(premount_dirfd, relative_path.c_str(),
				     repository_path)) {
      syslog(LOG_ERR, "FindOutRepoAndMaybeHardlink failed");
    }
  }
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
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  ScopedFd fd(openat(premount_dirfd, relative_path.c_str(), O_WRONLY));
  if (fd.get() == -1) {
    return -errno;
  }
  WRAP_ERRNO(ftruncate(fd.get(), size));
}

static int fs_unlink(const char *path) {
  if (*path == 0)
    return -ENOENT;
  string relative_path(GetRelativePath(path));
  string repo_file_path(GetRepoItemPath(premount_dirfd, relative_path));
  int ret = unlinkat(premount_dirfd, relative_path.c_str(), 0);
  if (-1 == ret) ret = -errno;

  if (!GarbageCollectOneRepoFile(repo_file_path)) {
    syslog(LOG_ERR, "GarbageCollectOneRepoFile failed");
  }

  return ret;
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
  assert(init_gcrypt());  // Initialize gcrypt before starting threads.
  openlog("cowfs", LOG_PERROR | LOG_PID, LOG_USER);

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
  repository_path = fs::canonical(conf.repository).string();
  GcTree(conf.repository);
  HardlinkTree(conf.repository, conf.underlying_path);
  premount_dirfd = open(conf.underlying_path, O_PATH|O_DIRECTORY);
  assert(premount_dirfd != -1);

  int ret = fuse_main(args.argc, args.argv, &o, NULL);
  fuse_opt_free_args(&args);
  return ret;
}
