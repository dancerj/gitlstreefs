/*
  A simplistic ext2 file system

  Allows you to mount ext2 files.

Example:

./out/experimental/ext2 --underlying_file=$(readlink -f ~/tmp/sid-ext2 )
~/tmp/hoge -d

*/
#define FUSE_USE_VERSION 32

#include "directory_container.h"
#include "experimental/linux_ext2.h"
#include "get_current_dir.h"
#include "strutil.h"

#include <assert.h>
#include <fuse.h>
#include <string.h>
#include <sys/mman.h>

#include <iostream>
#include <map>
#include <set>

namespace ext2fs {
class Ext2File;
}

namespace {

#define ASSERT_EQ(a, b)                                                 \
  if (a != b) {                                                         \
    std::cout << #a << " " << a << ", " << #b << " " << b << std::endl; \
    assert(a == b);                                                     \
  }

class InodeCache;

// File content derived from inode metadata.
struct FileContent {
  explicit FileContent(size_t initial_size) : size(initial_size) {}
  // Delete copy constructor
  FileContent(const FileContent &) = delete;
  FileContent operator=(const FileContent &) = delete;

  size_t size{0};
  // a vector of blocks of BlockSize() blocks.
  std::vector<const char *> blocks{};

  // TODO: My brain hurts. I need a unit test.
  void copy(size_t block_size, char *target, size_t size,
            off_t offset = 0) const {
    size_t b = offset / block_size;
    if (offset % block_size) {
      size_t to_copy = std::min(size, size - offset % block_size);
      memcpy(target, blocks[b] + offset % block_size, to_copy);

      b++;
      size -= to_copy;
      target += to_copy;
    }
    while (size > block_size) {
      memcpy(target, blocks[b], block_size);

      b++;
      size -= block_size;
      target += block_size;
    }
    if (size) {
      memcpy(target, blocks[b], size);
    }
  }
};

class Ext2Reader {
 public:
  Ext2Reader(directory_container::DirectoryContainer *fs, const char *filename,
             off_t filesize, off_t offset)
      : fs_(fs), filesize_(filesize), ok_(false) {
    ScopedFd fd(open(filename, O_RDONLY | O_CLOEXEC));
    assert(fd.get() != -1);
    fs_block_map_ = reinterpret_cast<char *>(
        mmap(nullptr, filesize, PROT_READ, MAP_SHARED, fd.get(), offset));
    if (fs_block_map_ == MAP_FAILED) {
      perror("mmap");
      std::cout << filename << " " << filesize << std::endl;
      return;
    }

    std::cerr << std::hex << (long)fs_block_map_ << std::dec << std::endl;

    const int SUPERBLOCK_LOCATION = 1024;
    s_ = reinterpret_cast<const ext4_super_block *>(fs_block_map_ +
                                                    SUPERBLOCK_LOCATION);

    ASSERT_EQ(s_->magic.get(), 0xef53);

    // I am using struct ext4_dir_entry_2.
    ASSERT_EQ((s_->feature_incompat.get() & EXT4_FEATURE_INCOMPAT_FILETYPE),
              EXT4_FEATURE_INCOMPAT_FILETYPE);

    // Also other incompat flags aren't there.
    // TODO support more things.
    ASSERT_EQ((s_->feature_incompat.get() & ~EXT4_FEATURE_INCOMPAT_FILETYPE),
              0);

    group_desc_ = reinterpret_cast<const ext4_group_desc *>(
        ReadBlock(s_->block_group_descriptor_table_block_number()));

    const ext4_inode &root = *Inode(2);
    ASSERT_EQ(root.mode.get(), 040755);
    WalkDir("/", root.block[0].get());
    ok_ = true;
  }

  bool ok() const { return ok_; }

  ~Ext2Reader() { munmap(fs_block_map_, filesize_); }

  const char *ReadBlock(int n) { return fs_block_map_ + n * s_->block_size(); }

  const ext4_group_desc *GroupDesc(int n) const {
    // This seems to be 32 bytes size, but sometimes 64 bytes on ext4 with
    // 64 bit stuff.
    const int block_group_descriptor_size = 32;
    return reinterpret_cast<const ext4_group_desc *>(
        reinterpret_cast<const char *>(group_desc_) +
        n * block_group_descriptor_size);
  }

  const ext4_inode *Inode(int inode_number) {
    const size_t inodes_per_group = s_->inodes_per_group.get();
    const size_t inode_size = s_->inode_size.get();
    size_t index = (inode_number - 1) % inodes_per_group;
    size_t block_group = (inode_number - 1) / inodes_per_group;
    size_t containing_block = (index * inode_size) / s_->block_size();
    size_t index_inside_block = (index * inode_size) % s_->block_size();
    const char *c = ReadBlock(GroupDesc(block_group)->inode_table_lo.get() +
                              containing_block) +
                    index_inside_block;
    return reinterpret_cast<const ext4_inode *>(c);
  }

  void WalkDir(std::string_view path, int block);

  std::unique_ptr<FileContent> GetFileContent(const ext4_inode *inode) {
    auto ret = std::make_unique<FileContent>(inode->size_lo.get());
    // The block size is in 512 blocks for some reason and needs to
    // be translated to the 4k or 1k blocks this thing is actually
    // using.
    size_t actual_blocks =
        (inode->size_lo.get() + BlockSize() - 1) / BlockSize();

    // I need to reduce the actual blocks by the indirect blocks if
    // I go from the i_blocks_lo. i_size gives you the more correct
    // data size.

    for (size_t i = 0; i < 12; ++i) {
      if (inode->block[i].get() && i < actual_blocks) {
        ret->blocks.push_back(ReadBlock(inode->block[i].get()));
      }
    }

    const size_t indirect_block_contents = BlockSize() / sizeof(__le32);
    // Go for the indirect block #1.
    if (actual_blocks >= 12) {
      auto a =
          reinterpret_cast<const __le32 *>(ReadBlock(inode->block[12].get()));
      auto num_to_read = std::min(indirect_block_contents, actual_blocks - 12);
      for (size_t i = 0; i < num_to_read; ++i) {
        ret->blocks.push_back(ReadBlock(a[i]));
      }
    }

    const size_t doubly_indirect_block_contents =
        (BlockSize() / sizeof(__le32)) * indirect_block_contents;
    if (actual_blocks >= 12 + indirect_block_contents) {
      // Doubly indirect block.
      auto a =
          reinterpret_cast<const __le32 *>(ReadBlock(inode->block[13].get()));
      auto num_to_read = std::min(doubly_indirect_block_contents,
                                  actual_blocks - 12 - indirect_block_contents);
      for (size_t i = 0; i < num_to_read; i++) {
        auto b = reinterpret_cast<const __le32 *>(
            ReadBlock(a[i / indirect_block_contents]));
        if (b[i % indirect_block_contents]) {
          ret->blocks.push_back(ReadBlock(b[i % indirect_block_contents]));
        }
      }
    }

    if (actual_blocks >=
        12 + indirect_block_contents + doubly_indirect_block_contents) {
      // TODO
      ASSERT_EQ(1, 0);
    }

    return ret;
  }

  size_t BlockSize() const { return s_->block_size(); }

 private:
  directory_container::DirectoryContainer *fs_;
  char *fs_block_map_;
  const ext4_super_block *s_;
  const ext4_group_desc *group_desc_;
  std::map<std::string, const ext4_inode *> path_to_inodes;
  std::map<const ext4_inode *, std::unique_ptr<InodeCache>> inode_cache_;
  off_t filesize_;
  bool ok_;
};

// A cache of things related to ext4_inode.
class InodeCache {
 public:
  InodeCache(const ext4_inode *inode) : inode_(inode) {}
  ~InodeCache() {}
  const ext4_inode *i() const { return inode_; }

  const FileContent *CachedFileContent(Ext2Reader *reader) {
    if (content_) {
      return content_.get();
    }
    content_ = reader->GetFileContent(inode_);
    return content_.get();
  }

 private:
  const ext4_inode *inode_;
  std::unique_ptr<FileContent> content_{};
};

std::unique_ptr<Ext2Reader> global_ext2_reader{};
std::unique_ptr<directory_container::DirectoryContainer> fs;

}  // namespace

namespace ext2fs {

class Ext2File : public directory_container::File {
 public:
  explicit Ext2File(InodeCache *icache) : icache_(icache) {}
  virtual ~Ext2File() {}

  virtual int Getattr(struct stat *stbuf) override {
    stbuf->st_mode = icache_->i()->mode.get();
    stbuf->st_size = icache_->i()->size_lo.get();

    return 0;
  }

  virtual int Open() override { return 0; }

  virtual int Release() override {
    // TODO: do I ever need to clean up?
    return 0;
  }

  virtual ssize_t Read(char *target, size_t size, off_t offset) override {
    auto contents = icache_->CachedFileContent(global_ext2_reader.get());

    if (offset < static_cast<off_t>(contents->size)) {
      if (offset + size > contents->size) {
        size = contents->size - offset;
      }
      contents->copy(global_ext2_reader->BlockSize(), target, size, offset);
    } else
      size = 0;
    return size;
  }

  virtual ssize_t Readlink(char *target, size_t size) override {
    const size_t contents_size = icache_->i()->size_lo.get();
    const char *name = reinterpret_cast<const char *>(icache_->i()->block);
    auto contents = icache_->CachedFileContent(global_ext2_reader.get());

    if (size > contents_size) {
      size = contents_size;
      target[size] = 0;
    } else {
      // TODO: is this an error condition that we can't fit in the final 0 ?
    }

    if (contents_size > EXT4_N_BLOCKS * 4) {
      // data is in direct blocks.
      contents->copy(global_ext2_reader->BlockSize(), target, size);
    } else {
      // data is directly written in block[] area.
      memcpy(target, name, size);
    }

    return 0;
  }

 private:
  InodeCache *icache_;
};

bool LoadDirectory(const char *ext2_file) {
  assert(ext2_file != nullptr);
  struct stat st;
  assert(-1 != stat(ext2_file, &st));

  // Needs an instance to keep around reference to mmap during the lifetime of
  // the file system.
  fs.reset(new directory_container::DirectoryContainer());
  global_ext2_reader =
      std::make_unique<Ext2Reader>(fs.get(), ext2_file, st.st_size, 0);
  return global_ext2_reader->ok();
}

static int fs_getattr(const char *path, struct stat *stbuf,
                      fuse_file_info *fi) {
  // First request for .Trash after mount from GNOME gets called with
  // path and fi == null.
  if (fi) {
    auto f = reinterpret_cast<directory_container::File *>(fi->fh);
    assert(f != nullptr);
    return f->Getattr(stbuf);
  } else {
    return fs->Getattr(path, stbuf);
  }
}

static int fs_opendir(const char *path, struct fuse_file_info *fi) {
  if (*path == 0) return -ENOENT;
  const auto d =
      dynamic_cast<directory_container::Directory *>(fs->mutable_get(path));
  fi->fh = reinterpret_cast<uint64_t>(d);
  return 0;
}

static int fs_releasedir(const char *, struct fuse_file_info *fi) {
  if (fi->fh == 0) return -EBADF;
  return 0;
}

static int fs_readdir(const char *, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      fuse_readdir_flags) {
  filler(buf, ".", nullptr, 0, fuse_fill_dir_flags{});
  filler(buf, "..", nullptr, 0, fuse_fill_dir_flags{});
  const directory_container::Directory *d =
      reinterpret_cast<directory_container::Directory *>(fi->fh);
  if (!d) return -ENOENT;
  d->for_each(
      [&](const std::string &s, const directory_container::File *unused) {
        filler(buf, s.c_str(), nullptr, 0, fuse_fill_dir_flags{});
      });
  return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi) {
  if (*path == 0) return -ENOENT;
  directory_container::File *f = fs->mutable_get(path);
  if (!f) return -ENOENT;
  fi->fh = reinterpret_cast<uint64_t>(f);

  return f->Open();
}

static int fs_read(const char *, char *target, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
  auto f = reinterpret_cast<directory_container::File *>(fi->fh);
  if (!f) return -ENOENT;

  return f->Read(target, size, offset);
}

static int fs_readlink(const char *path, char *buf, size_t size) {
  if (*path == 0) return -ENOENT;
  directory_container::File *f = fs->mutable_get(path);
  if (!f) return -ENOENT;

  return f->Readlink(buf, size);
}

void *fs_init(fuse_conn_info *, fuse_config *config) {
  config->nullpath_ok = 1;
  return nullptr;
}

}  // namespace ext2fs

namespace {
void Ext2Reader::WalkDir(std::string_view path, int block) {
  const ext4_dir_entry_2 *begin =
      reinterpret_cast<const ext4_dir_entry_2 *>(ReadBlock(block));
  // This for loop isn't obvious.
  for (auto dirent = begin;
       reinterpret_cast<const char *>(dirent) <
       reinterpret_cast<const char *>(begin) + s_->block_size();
       dirent = dirent->next()) {
    if (dirent->filename() != "." && dirent->filename() != "..") {
      const auto inode = Inode(dirent->inode.get());
      std::string new_path =
          std::string(path) + std::string(dirent->filename());
      if ((inode->mode.get() & S_IFMT) == S_IFDIR) {
        // If it's a directory, recurse.

        // lost+found contains 12 blocks (24 == i_blocks_lo ),
        // consuming all direct blocks.

        // TODO walk all blocks.
        WalkDir(new_path + "/", inode->block[0].get());
      } else {
        InodeCache *icache = inode_cache_[inode].get();
        if (!icache) {
          icache =
              (inode_cache_[inode] = std::make_unique<InodeCache>(inode)).get();
        }
        fs_->add(new_path, std::make_unique<ext2fs::Ext2File>(icache));
      }
    }
  }
};
}  // anonymous namespace

using namespace ext2fs;

struct ext2fs_config {
  char *underlying_file{nullptr};
};

#define MYFS_OPT(t, p, v) \
  { t, offsetof(ext2fs_config, p), v }

static struct fuse_opt ext2fs_opts[] = {
    MYFS_OPT("--underlying_file=%s", underlying_file, 0), FUSE_OPT_END};
#undef MYFS_OPT

int main(int argc, char *argv[]) {
  struct fuse_operations o = {};
#define DEFINE_HANDLER(n) o.n = &fs_##n
  DEFINE_HANDLER(getattr);
  DEFINE_HANDLER(init);
  DEFINE_HANDLER(open);
  DEFINE_HANDLER(opendir);
  DEFINE_HANDLER(read);
  // NOTE: read_buf shouldn't be implemented because using FD isn't useful.
  DEFINE_HANDLER(readdir);
  DEFINE_HANDLER(readlink);
  DEFINE_HANDLER(releasedir);
#undef DEFINE_HANDLER

  fuse_args args = FUSE_ARGS_INIT(argc, argv);
  ext2fs_config conf{};
  fuse_opt_parse(&args, &conf, ext2fs_opts, nullptr);

  if (conf.underlying_file == nullptr) {
    fprintf(stderr,
            "Usage: %s [mountpoint] "
            "--underlying_file=file\n",
            argv[0]);
    return EXIT_FAILURE;
  }

  if (!LoadDirectory(conf.underlying_file)) {
    std::cout << "Failed to load ext2 file." << std::endl;
    return EXIT_FAILURE;
  }

  int ret = fuse_main(args.argc, args.argv, &o, nullptr);
  fuse_opt_free_args(&args);
  return ret;
}
