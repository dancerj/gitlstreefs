#include <stdint.h>

#include <cassert>
#include <cmath>
#include <string_view>

// I probably don't really need the packed attribute because
// everything is at least 4 byte aligned. ext4_dir_entry (4 byte aligned) is 263
// byte size and the only odd one out.
class __attribute__((packed)) Le32 {
 public:
  void set(uint32_t value) { data_ = value; }
  uint32_t get() const { return data_; }

 private:
  uint32_t data_;
};

struct __attribute__((packed)) Le16 {
 public:
  void set(uint16_t value) { data_ = value; }
  uint16_t get() const { return data_; }

 private:
  uint16_t data_;
};

typedef uint8_t __u8;

/*
 * Super Block filesystems/ext4/globals.html#super-block
 */
struct ext4_super_block {
  Le32 inodes_count;
  Le32 blocks_count;
  Le32 r_blocks_count;
  Le32 free_blocks_count;
  Le32 free_inodes_count;
  Le32 first_data_block;
  Le32 log_block_size;
  // must equal log_block_size if bigalloc is not enabled.
  Le32 log_cluster_size;
  Le32 blocks_per_group;
  Le32 clusters_per_group;
  Le32 inodes_per_group;
  Le32 mtime;
  Le32 wtime;
  Le16 mnt_count;
  Le16 max_mnt_count;
  Le16 magic;
  Le16 state;
  Le16 errors;
  Le16 minor_rev_level;
  Le32 lastcheck;
  Le32 checkinterval;
  Le32 creator_os;
  Le32 rev_level;
  Le16 def_resuid;
  Le16 def_resgid;
  Le32 first_ino;
  Le16 inode_size;
  Le16 block_group_nr;
  Le32 feature_compat;
  Le32 feature_incompat;
  Le32 feature_ro_compat;
  __u8 uuid[16];
  char volume_name[16];
  char last_mounted[64];
  Le32 __unused_algorithm_usage_bitmap;
  __u8 __unused_prealloc_blocks;
  __u8 __unused_prealloc_dir_blocks;
  Le16 padding1;
  __u8 journal_uuid[16];
  Le32 journal_inum;
  Le32 journal_dev;
  Le32 last_orphan;
  Le32 hash_seed[4];
  __u8 def_hash_version;
  __u8 jnl_backup_type;
  Le16 desc_size;
  Le32 default_mount_opts;
  Le32 first_meta_bg;

  Le32 reserved[190]; /* Padding to the end of the block */

  long block_size() const {
    assert(log_block_size.get() >= 0);
    return 1024 << log_block_size.get();
  }
  long cluster_size() const {
    assert(log_cluster_size.get() >= 0);
    return 1024 << log_cluster_size.get();
  }

  int block_group() const {
    float n_block_groups_by_blocks =
        std::ceil(static_cast<float>(inodes_count.get()) /
                  static_cast<float>(inodes_per_group.get()));
    float n_block_groups_by_inodes =
        std::ceil(static_cast<float>(blocks_count.get()) /
                  static_cast<float>(blocks_per_group.get()));
    assert(n_block_groups_by_inodes == n_block_groups_by_blocks);
    return n_block_groups_by_blocks;
  }

  int block_group_descriptor_table_block_number() const {
    const int SUPERBLOCK_LOCATION = 1024;
    int superblock_block_number = SUPERBLOCK_LOCATION / block_size();
    return superblock_block_number + 1;
  }
};

// Check some entries so that what I typed in is reasonable.
static_assert(offsetof(ext4_super_block, creator_os) == 0x48);
static_assert(offsetof(ext4_super_block, first_meta_bg) == 0x104);
static_assert(sizeof(ext4_super_block) == 0x400);

/*
 * Structure of a blocks group descriptor
 */
struct ext4_group_desc {
  Le32 block_bitmap_lo;
  Le32 inode_bitmap_lo;
  Le32 inode_table_lo;
  Le16 free_blocks_count_lo;
  Le16 free_inodes_count_lo;
  Le16 used_dirs_count_lo;
  Le16 flags;
  Le32 exclude_bitmap_lo;
  Le16 block_bitmap_csum_lo;
  Le16 inode_bitmap_csum_lo;
  Le16 itable_unused_lo;
  Le16 checksum;
  Le32 block_bitmap_hi;
  Le32 inode_bitmap_hi;
  Le32 inode_table_hi;
  Le16 free_blocks_count_hi;
  Le16 free_inodes_count_hi;
  Le16 used_dirs_count_hi;
  Le16 itable_unused_hi;
  Le32 exclude_bitmap_hi;
  Le16 block_bitmap_csum_hi;
  Le16 inode_bitmap_csum_hi;
  Le32 reserved;
};

static_assert(offsetof(ext4_group_desc, checksum) == 0x1e);
static_assert(sizeof(ext4_group_desc) == 64);

/*
 * Constants relative to the data blocks
 */
enum Blocks {
  EXT4_NDIR_BLOCKS = 12,
  EXT4_IND_BLOCK = 12,
  EXT4_DIND_BLOCK,
  EXT4_TIND_BLOCK,
  EXT4_N_BLOCKS
};
static_assert(EXT4_NDIR_BLOCKS == 12);
static_assert(EXT4_DIND_BLOCK == 13);
static_assert(EXT4_N_BLOCKS == 15);

/*
 * Structure of an inode on the disk
 *
 * https://docs.kernel.org/filesystems/ext4/dynamic.html#index-nodes
 */
struct ext4_inode {
  Le16 mode;
  Le16 uid;
  // Size is the actual file data size.
  Le32 size_lo;
  Le32 atime;
  Le32 ctime;
  Le32 mtime;
  Le32 dtime;
  Le16 gid;
  Le16 links_count;
  /* apparently this isn't the ext4 blocks but the number of disk blocks (512
   * bytes), why????!! */
  // Blocks is the actual size including metadata, indirect blocks.
  Le32 blocks_lo;
  Le32 flags;
  // For linux this OSD is version.
  Le32 l_version;
  Le32 block[EXT4_N_BLOCKS];
  Le32 generation;
  Le32 file_acl_lo;
  Le32 size_high;
  Le32 __unused_obso_faddr;
  // For linux
  Le16 l_blocks_high;
  Le16 l_file_acl_high;
  Le16 l_uid_high;
  Le16 l_gid_high;
  Le16 l_checksum_lo;
  Le16 l_reserved;
  Le16 extra_isize;
  Le16 checksum_hi;
  Le32 ctime_extra;
  Le32 mtime_extra;
  Le32 atime_extra;
  Le32 crtime;
  Le32 crtime_extra;
  Le32 version_hi;
  Le32 projid;
  // On disk format has some more padding (inline data) after this, and usually
  // inodes are inode_size as defined in superblock.
};

static_assert(offsetof(ext4_inode, block) == 0x28);
static_assert(offsetof(ext4_inode, extra_isize) == 0x80);
static_assert(sizeof(ext4_inode) == 0xa0);

/*
 * Structure of a directory entry
 */
#define EXT4_NAME_LEN 255
/*
 * Base length of the ext4 directory entry excluding the name length
 */
#define EXT4_BASE_DIR_LEN (sizeof(struct ext4_dir_entry_2) - EXT4_NAME_LEN)

/*
 * directory entry. Contains file_type in the unused more significant
 * bits of name_len since name_len was always < 256
 */
struct __attribute__((packed)) ext4_dir_entry_2 {
  Le32 inode;
  // I hear a rumour that rec_len is 4-byte aligned, why?
  // directory entry length.
  Le16 rec_len;
  __u8 name_len;
  __u8 file_type;
  char name[EXT4_NAME_LEN];

  const ext4_dir_entry_2* next() const {
    return reinterpret_cast<const ext4_dir_entry_2*>(
        reinterpret_cast<const char*>(this) + rec_len.get());
  }

  std::string_view filename() const { return std::string_view(name, name_len); }
};

static_assert(offsetof(ext4_dir_entry_2, name) == 0x8);
static_assert(sizeof(ext4_dir_entry_2) == 263);

/*
 * Feature set definitions
 */

#define EXT4_FEATURE_INCOMPAT_FILETYPE 0x0002
