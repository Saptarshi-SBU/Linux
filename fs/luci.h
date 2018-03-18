/*
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/include/linux/minix_fs.h
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  from
 *
 *  fs/ext2.h
 *
 *  Copyright (C) 2017 Saptarshi Sen
 */
#ifndef _LUCI_H_
#define _LUCI_H_

#include <linux/fs.h>
#include <linux/blockgroup_lock.h>
#include <linux/percpu_counter.h>
#include <linux/rbtree.h>
#include <linux/debugfs.h>

/* data type for block offset of block group */
typedef int luci_grpblk_t;

/* data type for filesystem-wide blocks number */
typedef unsigned long luci_fsblk_t;


/*
 * Structure of the super block
 */
struct luci_super_block {
    __le32  s_inodes_count;     /* Inodes count */
    __le32  s_blocks_count;     /* Blocks count */
    __le32  s_r_blocks_count;   /* Reserved blocks count */
    __le32  s_free_blocks_count;    /* Free blocks count */
    __le32  s_free_inodes_count;    /* Free inodes count */
    __le32  s_first_data_block; /* First Data Block */
    __le32  s_log_block_size;   /* Block size */
    __le32  s_log_frag_size;    /* Fragment size */
    __le32  s_blocks_per_group; /* # Blocks per group */
    __le32  s_frags_per_group;  /* # Fragments per group */
    __le32  s_inodes_per_group; /* # Inodes per group */
    __le32  s_mtime;        /* Mount time */
    __le32  s_wtime;        /* Write time */
    __le16  s_mnt_count;        /* Mount count */
    __le16  s_max_mnt_count;    /* Maximal mount count */
    __le16  s_magic;        /* Magic signature */
    __le16  s_state;        /* File system state */
    __le16  s_errors;       /* Behaviour when detecting errors */
    __le16  s_minor_rev_level;  /* minor revision level */
    __le32  s_lastcheck;        /* time of last check */
    __le32  s_checkinterval;    /* max. time between checks */
    __le32  s_creator_os;       /* OS */
    __le32  s_rev_level;        /* Revision level */
    __le16  s_def_resuid;       /* Default uid for reserved blocks */
    __le16  s_def_resgid;       /* Default gid for reserved blocks */
    /*
     * These fields are for LUCI_DYNAMIC_REV superblocks only.
     *
     * Note: the difference between the compatible feature set and
     * the incompatible feature set is that if there is a bit set
     * in the incompatible feature set that the kernel doesn't
     * know about, it should refuse to mount the filesystem.
     *
     * e2fsck's requirements are more strict; if it doesn't know
     * about a feature in either the compatible or incompatible
     * feature set, it must abort and not try to meddle with
     * things it doesn't understand...
     */
    __le32  s_first_ino;        /* First non-reserved inode */
    __le16   s_inode_size;      /* size of inode structure */
    __le16  s_block_group_nr;   /* block group # of this superblock */
    __le32  s_feature_compat;   /* compatible feature set */
    __le32  s_feature_incompat;     /* incompatible feature set */
    __le32  s_feature_ro_compat;    /* readonly-compatible feature set */
    __u8    s_uuid[16];     /* 128-bit uuid for volume */
    char    s_volume_name[16];  /* volume name */
    char    s_last_mounted[64];     /* directory where last mounted */
    __le32  s_algorithm_usage_bitmap; /* For compression */
    /*
     * Performance hints.  Directory preallocation should only
     * happen if the LUCI_COMPAT_PREALLOC flag is on.
     */
    __u8    s_prealloc_blocks;  /* Nr of blocks to try to preallocate*/
    __u8    s_prealloc_dir_blocks;  /* Nr to preallocate for dirs */
    __u16   s_padding1;
    /*
     * Journaling support valid if EXT3_FEATURE_COMPAT_HAS_JOURNAL set.
     */
    __u8    s_journal_uuid[16]; /* uuid of journal superblock */
    __u32   s_journal_inum;     /* inode number of journal file */
    __u32   s_journal_dev;      /* device number of journal file */
    __u32   s_last_orphan;      /* start of list of inodes to delete */
    __u32   s_hash_seed[4];     /* HTREE hash seed */
    __u8    s_def_hash_version; /* Default hash version to use */
    __u8    s_reserved_char_pad;
    __u16   s_reserved_word_pad;
    __le32  s_default_mount_opts;
    __le32  s_first_meta_bg;    /* First metablock block group */
    __u32   s_reserved[190];    /* Padding to the end of the block */
};

/*
 * second extended-fs super-block data in memory
 */
struct luci_sb_info {
    unsigned long s_frag_size;  /* Size of a fragment in bytes */
    unsigned long s_frags_per_block;/* Number of fragments per block */
    unsigned long s_inodes_per_block;/* Number of inodes per block */
    unsigned long s_frags_per_group;/* Number of fragments in a group */
    unsigned long s_blocks_per_group;/* Number of blocks in a group */
    unsigned long s_inodes_per_group;/* Number of inodes in a group */
    unsigned long s_itb_per_group;  /* Number of inode table blocks per group */
    unsigned long s_gdb_count;  /* Number of group descriptor blocks */
    unsigned long s_desc_per_block; /* Number of group descriptors per block */
    unsigned long s_groups_count;   /* Number of groups in the fs */
    unsigned long s_overhead_last;  /* Last calculated overhead */
    unsigned long s_blocks_last;    /* Last seen block count */
    struct buffer_head * s_sbh; /* Buffer containing the super block */
    struct luci_super_block * s_lsb;    /* Pointer to the super block in the buffer */
    struct buffer_head ** s_group_desc;
    unsigned long  s_mount_opt;
    unsigned long s_sb_block;
    kuid_t s_resuid;
    kgid_t s_resgid;
    unsigned short s_mount_state;
    unsigned short s_pad;
    int s_addr_per_block_bits;
    int s_desc_per_block_bits;
    int s_inode_size;
    int s_first_ino;
    spinlock_t s_next_gen_lock;
    u32 s_next_generation;
    unsigned long s_dir_count;
    u8 *s_debts;
    struct percpu_counter s_freeblocks_counter;
    struct percpu_counter s_freeinodes_counter;
    struct percpu_counter s_dirs_counter;
    struct blockgroup_lock *s_blockgroup_lock;
    /*
     * s_lock protects against concurrent modifications of s_mount_state,
     * s_blocks_last, s_overhead_last and the content of superblock's
     * buffer pointed to by sbi->s_lsb.
     *
     * Note: It is used in luci_show_options() to provide a consistent view
     * of the mount options.
     */
    spinlock_t s_lock;
};

static inline struct luci_sb_info *LUCI_SB(struct super_block *sb)
{
    return sb->s_fs_info;
}

/*
 * Constants relative to the data blocks
 */
#define LUCI_NDIR_BLOCKS        12
#define LUCI_IND_BLOCK          LUCI_NDIR_BLOCKS
#define LUCI_DIND_BLOCK         (LUCI_IND_BLOCK + 1)
#define LUCI_TIND_BLOCK         (LUCI_DIND_BLOCK + 1)
#define LUCI_N_BLOCKS           (LUCI_TIND_BLOCK + 1)

/*
 * Structure of an inode on the disk
 */
struct luci_inode {
    __le16  i_mode;     /* File mode */
    __le16  i_uid;      /* Low 16 bits of Owner Uid */
    __le32  i_size;     /* Size in bytes */
    __le32  i_atime;    /* Access time */
    __le32  i_ctime;    /* Creation time */
    __le32  i_mtime;    /* Modification time */
    __le32  i_dtime;    /* Deletion Time */
    __le16  i_gid;      /* Low 16 bits of Group Id */
    __le16  i_links_count;  /* Links count */
    __le32  i_blocks;   /* Blocks count */
    __le32  i_flags;    /* File flags */
    union {
        struct {
            __le32  l_i_reserved1;
        } linux1;
        struct {
            __le32  h_i_translator;
        } hurd1;
        struct {
            __le32  m_i_reserved1;
        } masix1;
    } osd1;             /* OS dependent 1 */
    __le32  i_block[LUCI_N_BLOCKS];/* Pointers to blocks */
    __le32  i_generation;   /* File version (for NFS) */
    __le32  i_file_acl; /* File ACL */
    __le32  i_dir_acl;  /* Directory ACL */
    __le32  i_faddr;    /* Fragment address */
    union {
        struct {
            __u8    l_i_frag;   /* Fragment number */
            __u8    l_i_fsize;  /* Fragment size */
            __u16   i_pad1;
            __le16  l_i_uid_high;   /* these 2 fields    */
            __le16  l_i_gid_high;   /* were reserved2[0] */
            __u32   l_i_reserved2;
        } linux2;
        struct {
            __u8    h_i_frag;   /* Fragment number */
            __u8    h_i_fsize;  /* Fragment size */
            __le16  h_i_mode_high;
            __le16  h_i_uid_high;
            __le16  h_i_gid_high;
            __le32  h_i_author;
        } hurd2;
        struct {
            __u8    m_i_frag;   /* Fragment number */
            __u8    m_i_fsize;  /* Fragment size */
            __u16   m_pad1;
            __u32   m_i_reserved2[2];
        } masix2;
    } osd2;             /* OS dependent 2 */
};

/*
 * second extended file system inode data in memory
 */
struct luci_inode_info {
    __le32  i_data[15];
    __u32   i_flags;
    __u32   i_faddr;
    __u8    i_frag_no;
    __u8    i_frag_size;
    __u16   i_state;
    __u32   i_file_acl;
    __u32   i_dir_acl;
    __u32   i_dtime;

    /*
     * i_block_group is the number of the block group which contains
     * this file's inode.  Constant across the lifetime of the inode,
     * it is used for making block allocation decisions - we try to
     * place a file's data blocks near its inode block, and new inodes
     * near to their parent directory's inode.
     */
    __u32   i_block_group;
    /* current block group servicing new block allocations */
    __u32   i_active_block_group;

    /* block reservation info */
    struct luci_block_alloc_info *i_block_alloc_info;

    __u32   i_dir_start_lookup;
#ifdef CONFIG_LUCI_FS_XATTR
    /*
     * Extended attributes can be read independently of the main file
     * data. Taking i_mutex even when reading would cause contention
     * between readers of EAs and writers of regular file data, so
     * instead we synchronize on xattr_sem when reading or changing
     * EAs.
     */
    struct rw_semaphore xattr_sem;
#endif
    rwlock_t i_meta_lock;

    /*
     * truncate_mutex is for serialising luci_truncate() against
     * luci_getblock().  It also protects the internals of the inode's
     * reservation data structures: luci_reserve_window and
     * luci_reserve_window_node.
     */
    struct mutex truncate_mutex;
    struct inode    vfs_inode;
    struct list_head i_orphan;  /* unlinked but open inodes */
};

/*
 * Inode dynamic state flags
 */
#define LUCI_STATE_NEW          0x00000001 /* inode is newly created */

static inline struct luci_inode_info *LUCI_I(struct inode *inode)
{
    return container_of(inode, struct luci_inode_info, vfs_inode);
}

/*
 * Structure of a blocks group descriptor
 */
struct luci_group_desc
{
    __le32  bg_block_bitmap;        /* Blocks bitmap block */
    __le32  bg_inode_bitmap;        /* Inodes bitmap block */
    __le32  bg_inode_table;     /* Inodes table block */
    __le16  bg_free_blocks_count;   /* Free blocks count */
    __le16  bg_free_inodes_count;   /* Free inodes count */
    __le16  bg_used_dirs_count; /* Directories count */
    __le16  bg_pad;
    __le32  bg_reserved[3];
};

/*
 * Macro-instructions used to manage group descriptors
 */
#define LUCI_BLOCKS_PER_GROUP(s)    (LUCI_SB(s)->s_blocks_per_group)
#define LUCI_DESC_PER_BLOCK(s)      (LUCI_SB(s)->s_desc_per_block)
#define LUCI_INODES_PER_GROUP(s)    (LUCI_SB(s)->s_inodes_per_group)
#define LUCI_DESC_PER_BLOCK_BITS(s) (LUCI_SB(s)->s_desc_per_block_bits)

static inline luci_fsblk_t
luci_group_first_block_no(struct super_block *sb, unsigned long group_no)
{
    return group_no * (luci_fsblk_t)LUCI_BLOCKS_PER_GROUP(sb) +
        le32_to_cpu(LUCI_SB(sb)->s_lsb->s_first_data_block);
}

/*
 * Structure of a directory entry
 */

struct luci_dir_entry {
    __le32  inode;          /* Inode number */
    __le16  rec_len;        /* Directory entry length */
    __le16  name_len;       /* Name length */
    char    name[];         /* File name, up to LUCI_NAME_LEN */
};

/*
 * The new version of the directory entry.  Since LUCI structures are
 * stored in intel byte order, and the name_len field could never be
 * bigger than 255 chars, it's safe to reclaim the extra byte for the
 * file_type field.
 */
struct luci_dir_entry_2 {
    __le32  inode;          /* Inode number */
    __le16  rec_len;        /* Directory entry length */
    __u8    name_len;       /* Name length */
    __u8    file_type;
    char    name[];         /* File name, up to LUCI_NAME_LEN */
};

/*
 * Ext2 directory file types.  Only the low 3 bits are used.  The
 * other bits are reserved for now.
 */
enum {
    LUCI_FT_UNKNOWN     = 0,
    LUCI_FT_REG_FILE    = 1,
    LUCI_FT_DIR     = 2,
    LUCI_FT_CHRDEV      = 3,
    LUCI_FT_BLKDEV      = 4,
    LUCI_FT_FIFO        = 5,
    LUCI_FT_SOCK        = 6,
    LUCI_FT_SYMLINK     = 7,
    LUCI_FT_MAX
};

/*
 * Define LUCI_RESERVATION to reserve data blocks for expanding files
 */
#define LUCI_DEFAULT_RESERVE_BLOCKS     8
/*max window size: 1024(direct blocks) + 3([t,d]indirect blocks) */
#define LUCI_MAX_RESERVE_BLOCKS         1027
#define LUCI_RESERVE_WINDOW_NOT_ALLOCATED 0
/*
 * The second extended file system version
 */
#define LUCIFS_DATE     "95/08/09"
#define LUCIFS_VERSION      "0.5b"

/*
 * Special inode numbers
 */
#define LUCI_BAD_INO         1  /* Bad blocks inode */
#define LUCI_ROOT_INO        2  /* Root inode */
#define LUCI_BOOT_LOADER_INO     5  /* Boot loader inode */
#define LUCI_UNDEL_DIR_INO   6  /* Undelete directory inode */

/* First non-reserved inode for old luci filesystems */
#define LUCI_GOOD_OLD_FIRST_INO 11

/*
 * Macro-instructions used to manage several block sizes
 */
#define LUCI_MIN_BLOCK_SIZE     1024
#define LUCI_MAX_BLOCK_SIZE     4096
#define LUCI_MIN_BLOCK_LOG_SIZE       10
#define LUCI_BLOCK_SIZE(s)      ((s)->s_blocksize)
#define LUCI_ADDR_PER_BLOCK(s)      (LUCI_BLOCK_SIZE(s) / sizeof (__u32))
#define LUCI_BLOCK_SIZE_BITS(s)     ((s)->s_blocksize_bits)
#define LUCI_ADDR_PER_BLOCK_BITS(s) (LUCI_SB(s)->s_addr_per_block_bits)
#define LUCI_INODE_SIZE(s)      (LUCI_SB(s)->s_inode_size)
#define LUCI_FIRST_INO(s)       (LUCI_SB(s)->s_first_ino)

/*
 * Macro-instructions used to manage fragments
 */
#define LUCI_MIN_FRAG_SIZE      1024
#define LUCI_MAX_FRAG_SIZE      4096
#define LUCI_MIN_FRAG_LOG_SIZE        10
#define LUCI_FRAG_SIZE(s)       (LUCI_SB(s)->s_frag_size)
#define LUCI_FRAGS_PER_BLOCK(s)     (LUCI_SB(s)->s_frags_per_block)


/*
 * File system states
 */
#define LUCI_VALID_FS           0x0001  /* Unmounted cleanly */
#define LUCI_ERROR_FS           0x0002  /* Errors detected */

/*
 * Mount flags
 */
#define LUCI_MOUNT_CHECK        0x000001  /* Do mount-time checks */
#define LUCI_MOUNT_OLDALLOC     0x000002  /* Don't use the new Orlov allocator */
#define LUCI_MOUNT_GRPID        0x000004  /* Create files with directory's group */
#define LUCI_MOUNT_DEBUG        0x000008  /* Some debugging messages */
#define LUCI_MOUNT_ERRORS_CONT      0x000010  /* Continue on errors */
#define LUCI_MOUNT_ERRORS_RO        0x000020  /* Remount fs ro on errors */
#define LUCI_MOUNT_ERRORS_PANIC     0x000040  /* Panic on errors */
#define LUCI_MOUNT_MINIX_DF     0x000080  /* Mimics the Minix statfs */
#define LUCI_MOUNT_NOBH         0x000100  /* No buffer_heads */
#define LUCI_MOUNT_NO_UID32     0x000200  /* Disable 32-bit UIDs */
#define LUCI_MOUNT_XATTR_USER       0x004000  /* Extended user attributes */
#define LUCI_MOUNT_POSIX_ACL        0x008000  /* POSIX Access Control Lists */
#define LUCI_MOUNT_XIP          0x010000  /* Execute in place */
#define LUCI_MOUNT_USRQUOTA     0x020000  /* user quota */
#define LUCI_MOUNT_GRPQUOTA     0x040000  /* group quota */
#define LUCI_MOUNT_RESERVATION      0x080000  /* Preallocation */
#define LUCI_MOUNT_EXTENTS      0x100000  /* Extent allocation */

#define clear_opt(o, opt)       o &= ~opt
#define set_opt(o, opt)         o |= opt

/*
 * Maximal mount counts between two filesystem checks
 */
#define LUCI_DFL_MAX_MNT_COUNT      20  /* Allow 20 mounts */
#define LUCI_DFL_CHECKINTERVAL      0   /* Don't use interval check */

/*
 * Behaviour when detecting errors
 */
#define LUCI_ERRORS_CONTINUE        1   /* Continue execution */
#define LUCI_ERRORS_RO          2   /* Remount fs read-only */
#define LUCI_ERRORS_PANIC       3   /* Panic */
#define LUCI_ERRORS_DEFAULT     LUCI_ERRORS_CONTINUE
#define EFSCORRUPTED                    EUCLEAN /* Filesystem is corrupted */

/*
 * Revision levels
 */
#define LUCI_GOOD_OLD_REV   0   /* The good old (original) format */
#define LUCI_DYNAMIC_REV    1   /* V2 format w/ dynamic inode sizes */

#define LUCI_CURRENT_REV    LUCI_GOOD_OLD_REV
#define LUCI_MAX_SUPP_REV   LUCI_DYNAMIC_REV

#define LUCI_GOOD_OLD_INODE_SIZE 128

/*
 * Default values for user and/or group using reserved blocks
 */
#define LUCI_DEF_RESUID     0
#define LUCI_DEF_RESGID     0

/*
 * Default mount options
 */
#define LUCI_DEFM_DEBUG     0x0001
#define LUCI_DEFM_BSDGROUPS 0x0002
#define LUCI_DEFM_XATTR_USER    0x0004
#define LUCI_DEFM_ACL       0x0008
#define LUCI_DEFM_UID16     0x0010

/*
 * luci mount options
 */
struct luci_mount_options {
    unsigned long s_mount_opt;
    kuid_t s_resuid;
    kgid_t s_resgid;
};

/*
 * LUCI_DIR_PAD defines the directory entries boundaries
 *
 * NOTE: It must be a multiple of 4
 */
#define LUCI_DIR_PAD            4
#define LUCI_DIR_ROUND          (LUCI_DIR_PAD - 1)
#define LUCI_DIR_REC_LEN(name_len)  (((name_len) + 8 + LUCI_DIR_ROUND) & \
                     ~LUCI_DIR_ROUND)
#define LUCI_MAX_REC_LEN        ((1<<16)-1)

#define LUCI_NAME_LEN           255
#define LUCI_SUPER_MAGIC    0xEF53
#define LUCI_LINK_MAX           32000
#define LUCI_MAX_DEPTH          4

#define LUCI_SB_MAGIC_OFFSET    0x38
#define LUCI_SB_BLOCKS_OFFSET   0x04
#define LUCI_SB_BSIZE_OFFSET    0x18

static inline void verify_offsets(void)
{
#define A(x,y) BUILD_BUG_ON(x != offsetof(struct luci_super_block, y));
    A(LUCI_SB_MAGIC_OFFSET, s_magic);
    A(LUCI_SB_BLOCKS_OFFSET, s_blocks_count);
    A(LUCI_SB_BSIZE_OFFSET, s_log_block_size);
#undef A
}

/*
 * Define LUCIFS_DEBUG to produce debug messages
 */
/*
 * Debug code
 */

//#define DEBUG_BMAP

//debugfs params
typedef struct debugfs {
    struct dentry *dirent;
    u32 debug;
    struct dentry *dirent_dbg;
    u32 layout;
    struct dentry *dirent_layout;
    u64 latency;
    struct dentry *dirent_lat;
}debugfs_t;

extern debugfs_t dbgfsparam;

#define luci_dbg(f, a...)  { \
	            if (dbgfsparam.debug) \
                       printk (KERN_DEBUG "LUCI-FS %s :"f"\n", __func__, ## a); \
                    }
#define luci_dbg_inode(inode, f, a...)  { \
	            if (dbgfsparam.debug) \
                       printk (KERN_DEBUG "LUCI-FS %s :inode :%lu :"f"\n", __func__, \
                          inode->i_ino, ## a); \
                    }
#define luci_info(f, a...)  { \
                    printk (KERN_INFO "LUCI-FS %s : "f"\n", __func__, ## a); \
                    }
#define luci_info_inode(inode, f, a...)  { \
                    printk (KERN_INFO "LUCI-FS %s :inode :%lu :"f"\n", __func__, \
                        inode->i_ino, ## a); \
                    }
#define luci_err(f, a...)  { \
                    printk (KERN_ERR "LUCI-FS %s : error "f"\n", __func__, ## a); \
                    }
#define luci_err_inode(inode, f, a...)  { \
                    printk (KERN_ERR "LUCI-FS %s : error inode :%lu :"f"\n", __func__, \
                       inode->i_ino, ## a); \
                    }
#define luci_inode_latency(inode, f, a...)  { \
                    if (dbgfsparam.latency) \
                         printk (KERN_INFO "LUCI-FS %s : inode :%lu :"f"\n", \
                             __func__, inode->i_ino, ## a); \
                    }

#define BYTE_SHIFT 3

#define SECTOR_SHIFT 9

#define SECTOR_SIZE (1 << SECTOR_SHIFT)

static inline unsigned long
sector_align(unsigned long n)
{
    sector_t nsec = (n + SECTOR_SIZE - 1)/SECTOR_SIZE;
    return (long)nsec << SECTOR_SHIFT;
}


typedef struct {
   __le32 *p; // block entry
   __le32 key;
   struct buffer_head *bh;
} Indirect;

/* super.c */
struct luci_group_desc *
luci_get_group_desc(struct super_block *sb,
   unsigned int block_group, struct buffer_head **bh);
int
luci_write_inode(struct inode *inode, struct writeback_control *wbc);
int luci_truncate(struct inode *inode, loff_t size);

/* dir.c */
ino_t luci_inode_by_name(struct inode *, const struct qstr *);
struct luci_dir_entry_2 * luci_find_entry (struct inode *,const struct qstr *, struct page **);
inline unsigned luci_rec_len_from_disk(__le16 dlen);
inline __le16 luci_rec_len_to_disk(unsigned dlen);
struct luci_dir_entry_2 * luci_find_entry (struct inode * dir,
        const struct qstr * child, struct page ** res);
int luci_delete_entry(struct luci_dir_entry_2*, struct page*);
unsigned luci_chunk_size(struct inode *inode);
unsigned luci_sectors_per_block(struct inode *inode);
int luci_empty_dir(struct inode *dir);

struct page * luci_get_page(struct inode *dir, unsigned long n);
void luci_put_page(struct page *page);
unsigned luci_last_byte(struct inode *inode, unsigned long page_nr);
int luci_match (int len, const char * const name, struct luci_dir_entry_2 * de);
int luci_prepare_chunk(struct page *page, loff_t pos, unsigned len);
int luci_commit_chunk(struct page *page, loff_t pos, unsigned len);

/* inode.c */
#define COMPR_CREATE_ALLOC  0x01
#define COMPR_BLK_UPDATE    0x02
#define COMPR_BLK_INSERT    0x04

extern struct inode *luci_iget (struct super_block *, unsigned long);
extern int luci_get_block(struct inode *, sector_t, struct buffer_head *, int);
extern int luci_dump_layout(struct inode * inode);
unsigned long luci_find_leaf_block(struct inode * inode, unsigned long i_block);
int luci_insert_leaf_block(struct inode * inode, unsigned long i_block,
    unsigned long block);

/* ialloc.c */
extern struct buffer_head *
read_inode_bitmap(struct super_block *sb, unsigned long block_group);
extern struct buffer_head *
read_block_bitmap(struct super_block *sb, unsigned long block_group);
extern void luci_free_inode (struct inode * inode);
extern void luci_release_inode(struct super_block *sb, int group, int dir);
extern struct inode *
   luci_new_inode(struct inode *dir, umode_t mode, const struct qstr *qstr);
int luci_new_block(struct inode *, unsigned int, unsigned long *);
int luci_free_block(struct inode *inode, unsigned long block);

extern const struct inode_operations luci_file_inode_operations;
extern const struct file_operations luci_file_operations;
extern const struct inode_operations luci_dir_inode_operations;
extern const struct file_operations luci_dir_operations;
extern const struct address_space_operations luci_aops;
#endif
