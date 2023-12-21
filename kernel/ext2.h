#ifndef _ext2_h_
#define _ext2_h_

#include "atomic.h"
#include "cache.h"
#include "ide.h"
#include "shared.h"

// NOTE : most of these structs assume litte endian

#define EXT2_MAGIC_SIG ((0xef53))
#define EXT2_DIR_ENTRY_MAX_NAME_LEN ((255))

#define EXT2_INODE_DIR_TYPECODE ((0x4000))
#define EXT2_INODE_FILE_TYPECODE ((0x8000))
#define EXT2_INODE_SYMLINK_TYPECODE ((0xa000))

#define __packed __attribute__((packed))

struct Ext2_DirEntry {
    struct Ext2_DirEntryHeader {
        uint32_t inumber;   /* the inumber of the entity */
        uint16_t entry_len; /* the length of this entry*/
        uint8_t name_len;   /* the length of the name*/
        uint8_t file_type;  /* the type of the entity */
    } header;
    char name[EXT2_DIR_ENTRY_MAX_NAME_LEN + 1]; /* the name, with one slot for null terminator */
} __packed;

// adapted from OS dev https://wiki.osdev.org/Ext2
struct Ext2_Inode {
    uint16_t type_perms;            /* Type and Permissions  */
    uint16_t uid;                   /* User ID */
    uint32_t size_lo32;             /* Lower 32 bits of size in bytes */
    uint32_t last_access_time;      /* Last Access Time (in POSIX time) */
    uint32_t creation_time;         /* Creation Time (in POSIX time) */
    uint32_t last_modif_time;       /* Last Modification time (in POSIX time) */
    uint32_t deletion_time;         /* Deletion time (in POSIX time) */
    uint16_t gid;                   /* Group ID */
    uint16_t hard_links_count;      /* Count of hard links (directory entries) to this inode. When this reaches 0, the data blocks are marked as unallocated. */
    uint32_t disk_sectors_used;     /* Count of disk sectors (not Ext2 blocks) in use by this inode, not counting the actual inode structure nor directory entries linking to the inode. */
    uint32_t flags;                 /* Flags */
    uint8_t os_val1[4];             /* Operating System Specific value #1 */
    uint32_t direct_block[12];      /* Direct Block Pointers */
    uint32_t singly_indirect_block; /* Singly Indirect Block Pointer (Points to a block that is a list of block pointers to data) */
    uint32_t doubly_indirect_block; /* Doubly Indirect Block Pointer (Points to a block that is a list of block pointers to Singly Indirect Blocks) */
    uint32_t triply_indirect_block; /* Triply Indirect Block Pointer (Points to a block that is a list of block pointers to Doubly Indirect Blocks) */
    uint32_t generation_number;     /* Generation number (Primarily used for NFS) */
    uint32_t extended_attr_block;   /* In Ext2 version 0, this field is reserved. In version >= 1, Extended attribute block (File ACL). */
    uint32_t size_hi32;             /* In Ext2 version 0, this field is reserved. In version >= 1, Upper 32 bits of file size (if feature bit set) if it's a file, Directory ACL if it's a directory */
    uint32_t fragment_block;        /* Block address of fragment */
    uint8_t os_val2[12];            /*  Operating System Specific Value #2 */
} __packed;

struct Ext2_BlockGroupDesc {
    uint32_t data_usage_bitmap_block;  /* the block address of the data bitmap */
    uint32_t inode_usage_bitmap_block; /* the block address of the inode bitmap */
    uint32_t inode_table_block;        /* block address of itable*/
    uint16_t num_free_data;            /* number of free data blocks*/
    uint16_t num_free_inode;           /* number of free inodes */
    uint16_t num_dirs;                 /* number of directories */
    uint8_t padding[14];
} __packed;

struct Ext2_Superblock {
    uint32_t inodes_count;         /* inodes count */
    uint32_t blocks_count;         /* blocks count */
    uint32_t r_blocks_count;       /* superuser reserved blocks count */
    uint32_t free_blocks_count;    /* free blocks count */
    uint32_t free_inodes_count;    /* free inodes count */
    uint32_t superblock_block_num; /* superblock's block num */
    uint32_t log_block_size;       /* block size */
    uint32_t log_frag_size;        /* fragment size */
    uint32_t blocks_per_group;     /* num blocks per group */
    uint32_t frags_per_group;      /* num fragments per group */
    uint32_t inodes_per_group;     /* num inodes per group */
    uint32_t mtime;                /* last mount time */
    uint32_t wtime;                /* last write time */
    uint16_t mnt_count;            /* mount count since last consistency check*/
    uint16_t max_mnt_count;        /* mount count max till consistency check*/
    uint16_t magic;                /* magic signature */
    uint16_t state;                /* file system state */
    uint16_t errors;               /* errors behavior */
    uint16_t minor_rev_level;      /* minor revision level */
    uint32_t lastcheck;            /* time of last check */
    uint32_t checkinterval;        /* max. time between checks */
    uint32_t creator_os;           /* OS id */
    uint32_t major_rev_level;      /* major revision level */
    uint16_t uid_res;              /* uid for reserved blocks */
    uint16_t gid_res;              /* gid for reserved blocks */
    uint8_t padding[940];          /* either for later features in ext2 or ext3 or padding*/
} __packed;

// A wrapper around an i-node
class Node : public BlockIO {  // we implement BlockIO because we
                               // represent data

    /**
     * the number of entries in the directory, or MAX if not computed yet
     * we are safe to use MAX as we could not have a disk large enough to
     * reach that limit. we would run out of inodes and then data nodes
     */
    uint32_t num_entries;

    Ext2_Inode inode;
    Shared<CachedBlockReader> cbr;

   public:
    // i-number of this node
    const uint32_t number;

   private:
    uint32_t get_pbn_from_array(uint32_t array_pbn, uint32_t array_index);
    void check_pbn(uint32_t pbn);
    uint32_t logical_to_physical(uint32_t logical_block_number);
    void count_entries_jit();
    uint32_t read_block_data(uint32_t logical_block_number, char* buffer, uint32_t base_block_offset, uint32_t bytes_to_read);

   public:
    Node(uint32_t number, Ext2_Inode inode, Shared<CachedBlockReader> cbr);

    virtual ~Node() {}

    // How many bytes does this i-node represent
    //    - for a file, the size of the file
    //    - for a directory, implementation dependent
    //    - for a symbolic link, the length of the name
    uint32_t size_in_bytes();

    // read the given block (panics if the block number is not valid)
    // remember that block size is defined by the file system not the device
    virtual void read_block(uint32_t logical_block_number, char* buffer) override;

    // Read up to "n" bytes starting at "offset" and put the restuls in "buffer".
    // returns:
    //   > 0  actual number of bytes read
    //   = 0  end (offset == size_in_bytes)
    //   -1   error (offset > size_in_bytes)
    virtual int64_t read(uint32_t offset, uint32_t n, char* buffer) override;

    inline uint16_t get_type() {
        return inode.type_perms & 0xf000;
    }

    // true if this node is a directory
    bool is_dir();

    // true if this node is a file
    bool is_file();

    // true if this node is a symbolic link
    bool is_symlink();

    // If this node is a symbolic link, fill the buffer with
    // the name the link referes to.
    //
    // Panics if the node is not a symbolic link
    //
    // The buffer needs to be at least as big as the the value
    // returned by size_in_byte()
    void get_symbol(char* buffer);

    // Returns the number of hard links to this node
    uint32_t n_links();

    // return the inumber of the child with the name. or 0 if could not find
    uint32_t find(const char* name);

    // Returns the number of entries in a directory node
    //
    // Panics if not a directory
    uint32_t entry_count();

    template <typename T>
    void read(uint32_t offset, T& thing) {
        auto cnt = read_all(offset, sizeof(T), (char*)&thing);
        ASSERT(cnt == sizeof(T));
    }
};

// This class encapsulates the implementation of the Ext2 file system
class Ext2 {
    // the superblock
    Ext2_Superblock superblock;

    // The filesystem
    Shared<CachedBlockReader> cbr;

    // the block groups
    uint32_t block_group_count;
    Ext2_BlockGroupDesc* block_groups_descriptor_table;

   public:
    // The root directory for this file system
    Shared<Node> root;

    // Mount an existing file system residing on the given device
    // Panics if the file system is invalid
    Ext2(Shared<Ide> ide);
    virtual ~Ext2();

    // Returns the block size of the file system. Doesn't have
    // to match that of the underlying device
    uint32_t get_block_size();

    // Returns the actual size of an i-node. Ext2 specifies that
    // an i-node will have a minimum size of 128B but could have
    // more bytes for extended attributes
    uint32_t get_inode_size();

    // Returns the node with the given i-number
    Shared<Node> get_node(uint32_t inumber);

    // If the given node is a directory, return a reference to the
    // node linked to that name in the directory.
    //
    // Returns a null reference if "name" doesn't exist in the directory
    //
    // Panics if "dir" is not a directory
    Shared<Node> find(Shared<Node> dir, const char* name);

    // traverses the given path to find a file. 
    // supports relative and absolute paths (which start with a '/')
    // supports a variety of path styles such as "hello/", "hello", "hello/hello", "hello/hello/", etc
    // returns null reference if the node can't be found
    Shared<Node> find_by_path(Shared<Node> from, const char* path);
};


#endif