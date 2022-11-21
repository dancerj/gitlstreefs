// content-based cowdancer-like file system.
//
// At mount time it would:
// - hard-links same file based on content
// - gc to eliminate files no longer referenced.
//
// While mounted it would:
// - try to unlink the hardlinks before modification.
#define FUSE_USE_VERSION 35

#include <assert.h>
#include <fcntl.h>
#include <fuse.h>
#include <stddef.h>
#include <sys/file.h>
#include <sys/sysinfo.h>
#include <syslog.h>

#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "cowfs_crypt.h"
#include "disallow.h"
#include "file_copy.h"
#include "ptfs.h"
#include "scoped_fd.h"
#include "scoped_fileutil.h"
#include "strutil.h"
#include "update_rlimit.h"
#include "walk_filesystem.h"

using std::async;
using std::cerr;
using std::cout;
using std::endl;
using std::future;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::vector;

namespace {
string repository_path;

std::string Canonicalize(const std::string& path) {
  char* c = canonicalize_file_name(path.c_str());
  std::string r{c};
  free(c);
  return r;
}
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
  bool have_lock() const { return have_lock_; }

 private:
  bool GetLock(const string& path, const string& message) {
    fd_ = open(path.c_str(), O_CLOEXEC | O_CREAT | O_RDWR, 0777);
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
      perror("write");
      return false;
    }
    return true;
  }

  int fd_;
  bool have_lock_;
  DISALLOW_COPY_AND_ASSIGN(ScopedLock);
};

void GcTree(const string& repo) {
  cout << "GCing things we don't need" << endl;
  vector<string> to_delete{};
  WalkFilesystem(repo, [&to_delete](FTSENT* entry) {
    if (entry->fts_info == FTS_F && entry->fts_statp->st_nlink == 1) {
      // This is a stale file
      std::string path(entry->fts_path, entry->fts_pathlen);
      to_delete.emplace_back(path);
    }
  });
  for (const auto& s : to_delete) {
    if (-1 == unlink(s.c_str())) {
      cout << s << " is no longer needed " << endl;
      perror((string("unlink ") + s).c_str());
    }
  }
}

bool HardlinkOneFile(int dirfd_from, const string& from, int dirfd_to,
                     const string& to) {
  struct stat st1 {};
  struct stat st2 {};
  if ((-1 != fstatat(dirfd_from, from.c_str(), &st1, 0)) &&
      (-1 != fstatat(dirfd_to, to.c_str(), &st2, 0)) &&
      st1.st_ino == st2.st_ino) {
    // Already hardlinked. Should usually not happen, because we would
    // have broken a link somewhere else.
    // TODO: remove these extra stat steps and make it debug-only operations.
    return true;
  }

  ScopedTempFile to_tmp(dirfd_to, to, "of");
  if (-1 == linkat(dirfd_from, from.c_str(), dirfd_to, to_tmp.c_str(), 0)) {
    syslog(LOG_ERR, "linkat %s %s %m", from.c_str(), to_tmp.c_str());
    to_tmp.clear();
    return false;
  }
  if (-1 == renameat(dirfd_to, to_tmp.c_str(), dirfd_to, to.c_str())) {
    syslog(LOG_ERR, "renameat %s %s %m", to_tmp.c_str(), to.c_str());
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
  return repository_path + "/" + repo_dir_name + "/" + repo_file_name;
}

bool GarbageCollectOneRepoFile(const string& repo_file_path) {
  struct stat st;
  if (lstat(repo_file_path.c_str(), &st) != -1 && st.st_nlink == 1) {
    if (-1 == unlink(repo_file_path.c_str())) {
      if (errno == ENOENT) {
        // There was a parallel thread that deleted the file.
        return true;
      }
      syslog(LOG_ERR, "unlink garbage collection %s %m",
             repo_file_path.c_str());
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
  string repo_file_path(
      GetRepoItemPath(ptfs::PtfsHandler::premount_dirfd_, target));
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

  ScopedFileLockWithDelete lock(dirfd, target);
  if (from_fd.get() == -1) {
    if (errno == ENOENT) {
      // File not existing is okay, O_CREAT maybe specified.
      return true;
    }
    syslog(LOG_ERR, "open failed and not ENOENT: %m");
    return false;
  }
  // TODO: O_TRUNC might be an optimization.
  struct stat st {};
  if (-1 == fstat(from_fd.get(), &st)) {
    // I wonder why fstat can fail here.
    syslog(LOG_ERR, "fstat %s %m", target.c_str());
    return false;
  }

  // 0 is not an expected value, does the file system support hardlink?
  // TODO: This may happen if someone removed the file from another thread.
  if (st.st_nlink == 0) {
    syslog(LOG_ERR, "0 hardlink doesn't sound like a good filesystem for %s.",
           target.c_str());
    return true;
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
    return false;
  }
  to_tmp.clear();

  if (!MaybeGcAfterHardlinkBreakForTarget(dirfd, target)) {
    return false;
  }
  return true;
}

// This part may run as daemon, error failure is not visible.
bool FindOutRepoAndMaybeHardlink(int target_dirfd,
                                 const string& target_filename,
                                 const string& repo) {
  ScopedFileLockWithDelete lock(target_dirfd, target_filename);
  string buf, repo_dir_name, repo_file_name;
  if (!ReadFromFile(target_dirfd, target_filename, &buf)) {
    syslog(LOG_ERR, "Can't read from %s", target_filename.c_str());
    // Can't read from file.
    return false;
  }
  gcrypt_string_get_git_style_relpath(&repo_dir_name, &repo_file_name, buf);
  string repo_file_path(repo + "/" + repo_dir_name + "/" + repo_file_name);
  struct stat repo_st;
  if (lstat(repo_file_path.c_str(), &repo_st) == -1 && errno == ENOENT) {
    // If it doesn't exist, we hardlink to there.
    // First try to make subdirectory if it doesn't exist.
    // TODO: what's a reasonable umask for this repo?
    if (mkdir((repo + "/" + repo_dir_name).c_str(), 0700) == -1) {
      if (errno != EEXIST) {
        syslog(LOG_ERR, "Can't create directory %s %m", repo_dir_name.c_str());
        return false;
      }
    }
    if (!HardlinkOneFile(target_dirfd, target_filename, AT_FDCWD,
                         repo_file_path)) {
      syslog(LOG_DEBUG, "New file failed %s", target_filename.c_str());
      return false;
    }
  } else {
    // Hardlink from repo; deletes the target file.
    if (!HardlinkOneFile(AT_FDCWD, repo_file_path, target_dirfd,
                         target_filename)) {
      syslog(LOG_DEBUG, "Dedupe failed %s", target_filename.c_str());
      return false;
    }
  }
  return true;
}

// This is an offline process at startup not running as a daemon, so
// this can fail with an error message.
void HardlinkTree(const string& repo, const string& directory) {
  cout << "Hardlinking files we do need" << endl;
  const int ncpu = get_nprocs();
  vector<vector<string> > to_hardlink(ncpu);
  int cpu = 0;
  WalkFilesystem(directory, [&to_hardlink, &cpu, ncpu](FTSENT* entry) {
    if (entry->fts_info == FTS_F && entry->fts_statp->st_nlink == 1) {
      // A regular file and not a symlink.
      std::string path(entry->fts_path, entry->fts_pathlen);
      to_hardlink[cpu].emplace_back(path);
      ++cpu;
      cpu %= ncpu;
    }
  });
  vector<future<void> > jobs;
  for (int i = 0; i < ncpu; ++i) {
    vector<string>& tasks = to_hardlink[i];

    jobs.emplace_back(async([i, &repo, &tasks]() {
      for (const auto& s : tasks) {
        assert(FindOutRepoAndMaybeHardlink(AT_FDCWD, s, repo));
      }
      cout << "task " << i << " with " << tasks.size() << " tasks" << endl;
    }));
  }
}

class CowFileHandle : public ptfs::FileHandle {
 public:
  CowFileHandle(const string& relative_path, int fd)
      : FileHandle(fd), relative_path_(relative_path) {}
  virtual ~CowFileHandle() {}
  const char* relative_path_c_str() const { return relative_path_.c_str(); }

 private:
  string relative_path_;
  DISALLOW_COPY_AND_ASSIGN(CowFileHandle);
};

class CowFileSystemHandler : public ptfs::PtfsHandler {
 public:
  CowFileSystemHandler() {}

  virtual ~CowFileSystemHandler() {}

  virtual int Open(const std::string& relative_path, int open_flags,
                   std::unique_ptr<ptfs::FileHandle>* fh) override {
    if ((open_flags & O_ACCMODE) != O_RDONLY) {
      // Break hardlink on open if necessary.
      if (!MaybeBreakHardlink(premount_dirfd_, relative_path)) {
        return -EIO;
      }
    }

    int fd = openat(premount_dirfd_, relative_path.c_str(), open_flags);
    if (fd == -1) return -ENOENT;

    fh->reset(new CowFileHandle(relative_path, fd));
    return 0;
  }

  virtual int Create(const std::string& relative_path, int open_flags,
                     mode_t mode,
                     std::unique_ptr<ptfs::FileHandle>* fh) override {
    if ((open_flags & O_ACCMODE) != O_RDONLY) {
      // Break hardlink on open if necessary.
      if (!MaybeBreakHardlink(premount_dirfd_, relative_path)) {
        return -EIO;
      }
    }

    int fd = openat(premount_dirfd_, relative_path.c_str(), open_flags, mode);
    if (fd == -1) return -ENOENT;

    fh->reset(new CowFileHandle(relative_path, fd));
    return 0;
  }

  virtual int Release(int access_flags, ptfs::FileHandle* fh) override {
    CowFileHandle* cow_fh = dynamic_cast<CowFileHandle*>(fh);

    int ret = close(cow_fh->fd_release());
    if (-1 == ret) ret = -errno;
    const bool mutable_access = ((access_flags & O_ACCMODE) != O_RDONLY);
    if (mutable_access) {
      assert(repository_path.size() > 0);
      if (!FindOutRepoAndMaybeHardlink(premount_dirfd_,
                                       cow_fh->relative_path_c_str(),
                                       repository_path)) {
        syslog(LOG_ERR, "FindOutRepoAndMaybeHardlink failed");
      }
    }
    return ret;
  }

  virtual int Unlink(const string& relative_path) override {
    string repo_file_path(GetRepoItemPath(premount_dirfd_, relative_path));
    int ret = ptfs::PtfsHandler::Unlink(relative_path);
    if (!GarbageCollectOneRepoFile(repo_file_path)) {
      syslog(LOG_ERR, "GarbageCollectOneRepoFile failed");
    }
    return ret;
  }

 private:
  string path;
  DISALLOW_COPY_AND_ASSIGN(CowFileSystemHandler);
};

struct cowfs_config {
  char* repository{nullptr};
  char* lock_path{nullptr};
  char* underlying_path{nullptr};
};

#define MYFS_OPT(t, p, v) \
  { t, offsetof(cowfs_config, p), v }

static struct fuse_opt cowfs_opts[] = {
    MYFS_OPT("--repository=%s", repository, 0),
    MYFS_OPT("--lock_path=%s", lock_path, 0),
    MYFS_OPT("--underlying_path=%s", underlying_path, 0), FUSE_OPT_END};
#undef MYFS_OPT

int main(int argc, char** argv) {
  assert(init_gcrypt());  // Initialize gcrypt before starting threads.
  openlog("cowfs", LOG_PERROR | LOG_PID, LOG_USER);
  UpdateRlimit();  // We need more than 1024 files open.
  umask(0);

  struct fuse_operations o = {};
  ptfs::FillFuseOperations<CowFileSystemHandler>(&o);
  fuse_args args = FUSE_ARGS_INIT(argc, argv);
  cowfs_config conf{};
  fuse_opt_parse(&args, &conf, cowfs_opts, nullptr);

  if (!conf.underlying_path || !conf.lock_path || !conf.repository) {
    cerr << argv[0]
         << " [mountpoint] --lock_path= --underlying_path= --repository= "
         << endl;
    return EXIT_FAILURE;
  }
  ScopedLock fslock(conf.lock_path, "cowfs");
  repository_path = Canonicalize(conf.repository);
  GcTree(conf.repository);
  HardlinkTree(conf.repository, conf.underlying_path);
  ptfs::PtfsHandler::premount_dirfd_ =
      open(conf.underlying_path, O_PATH | O_DIRECTORY);
  if (-1 == ptfs::PtfsHandler::premount_dirfd_) {
    perror("open underlying_path");
    return EXIT_FAILURE;
  }
  int ret = fuse_main(args.argc, args.argv, &o, nullptr);
  fuse_opt_free_args(&args);
  return ret;
}
