#include "ext2.h"

#include "libk.h"

// ====================================================
// ====================== Node ========================
// ====================================================

uint32_t Node::get_pbn_from_array(uint32_t array_pbn, uint32_t array_index) {
    uint32_t result_pbn = 0;
    cbr->BlockIO::read(array_pbn * block_size + array_index * sizeof(uint32_t), result_pbn);
    return result_pbn;
}

void Node::check_pbn(uint32_t pbn) {
    if (pbn == 0) {
        Debug::panic("Trying to read from physical block 0\n");
    }
}

uint32_t Node::logical_to_physical(uint32_t logical_block_number) {
    uint32_t index = logical_block_number;
    uint32_t numbers_per_block = block_size / 4;

    // direct
    if (index < 12) {
        uint32_t pbn = inode.direct_block[index];
        check_pbn(pbn);
        return pbn;
    }
    index -= 12;

    // singly indirect
    if (index < numbers_per_block) {
        uint32_t pbn = inode.singly_indirect_block;
        check_pbn(pbn);

        pbn = get_pbn_from_array(pbn, index);
        check_pbn(pbn);

        return pbn;
    }
    index -= numbers_per_block;

    // doubly indirect
    if (index < numbers_per_block * numbers_per_block) {
        uint32_t pbn = inode.doubly_indirect_block;
        check_pbn(pbn);

        uint32_t first_tier_index = index / numbers_per_block;
        pbn = get_pbn_from_array(pbn, first_tier_index);
        check_pbn(pbn);

        uint32_t second_tier_index = index % numbers_per_block;
        pbn = get_pbn_from_array(pbn, second_tier_index);

        return pbn;
    }
    index -= numbers_per_block * numbers_per_block;

    // triply indirect
    if (index < numbers_per_block * numbers_per_block * numbers_per_block) {
        uint32_t pbn = inode.doubly_indirect_block;
        check_pbn(pbn);

        uint32_t first_tier_index = index / (numbers_per_block * numbers_per_block);
        pbn = get_pbn_from_array(pbn, first_tier_index);
        check_pbn(pbn);

        uint32_t second_tier_index = (index / numbers_per_block) % numbers_per_block;
        pbn = get_pbn_from_array(pbn, second_tier_index);
        check_pbn(pbn);

        uint32_t third_tier_index = index % numbers_per_block;
        pbn = get_pbn_from_array(pbn, third_tier_index);

        return pbn;
    }
    index -= numbers_per_block * numbers_per_block * numbers_per_block;

    // at this point we know this is invalid
    Debug::panic("Invalid logical block number %lu\n", logical_block_number);
    return -1;
}

void Node::count_entries_jit() {
    if (!is_dir() || num_entries != (uint32_t)-1) {
        return;
    }

    // FIXME : could optimize for concurrency so don't have to this twice

    // count entries
    uint32_t entries = 0;

    uint32_t dir_size = size_in_bytes();
    Ext2_DirEntry* dir_entry = new Ext2_DirEntry{0};
    for (uint32_t offset = 0; offset < dir_size; offset += dir_entry->header.entry_len) {
        read_all(offset, sizeof(dir_entry->header), (char*)&dir_entry->header);
        entries += dir_entry->header.inumber != 0;
    }
    delete dir_entry;

    num_entries = entries;
}

uint32_t Node::read_block_data(uint32_t logical_block_number, char* buffer, uint32_t base_block_offset, uint32_t bytes_to_read) {
    uint32_t pbn = logical_to_physical(logical_block_number);
    uint32_t physical_offset = pbn * block_size + base_block_offset;
    uint32_t read_amt = cbr->read_all(physical_offset, bytes_to_read, buffer);
    return read_amt;
}

// --------------------- public ------------------------

Node::Node(uint32_t number, Ext2_Inode inode, Shared<CachedBlockReader> cbr) : BlockIO(cbr->block_size), num_entries(-1), inode(inode), cbr(cbr), number(number) {}

uint32_t Node::size_in_bytes() {
    return inode.size_lo32;
}

// need to read a logical block using the tree structure
void Node::read_block(uint32_t logical_block_number, char* buffer) {
    read_block_data(logical_block_number, buffer, 0, block_size);
}

int64_t Node::read(uint32_t offset, uint32_t desired_n, char* buffer) {
    auto sz = size_in_bytes();
    if (offset > sz) return -1;
    if (offset == sz) return 0;

    auto n = K::min(desired_n, sz - offset);
    auto block_number = offset / block_size;
    auto offset_in_block = offset % block_size;
    auto actual_n = K::min(block_size - offset_in_block, n);
    ASSERT(actual_n <= n);
    ASSERT(offset + actual_n <= sz);
    ASSERT(offset_in_block + actual_n <= block_size);
    uint32_t read_amt = read_block_data(block_number, buffer, offset_in_block, actual_n);
    ASSERT(read_amt == actual_n);
    return actual_n;
}

bool Node::is_dir() {
    return get_type() == EXT2_INODE_DIR_TYPECODE;
}

bool Node::is_file() {
    return get_type() == EXT2_INODE_FILE_TYPECODE;
}

bool Node::is_symlink() {
    return get_type() == EXT2_INODE_SYMLINK_TYPECODE;
}

void Node::get_symbol(char* buffer) {
    if (!is_symlink()) {
        Debug::panic("Calling get_symbol() on a node that is not a symlink.\n");
    }

    uint32_t bytes = size_in_bytes();
    if (bytes <= 60) {
        memcpy(buffer, inode.direct_block, bytes);
    } else {
        read_all(0, bytes, buffer);
    }
}

uint32_t Node::n_links() {
    return inode.hard_links_count;
}

uint32_t Node::find(const char* name) {
    if (!is_dir()) {
        return 0;
    }

    uint32_t target_len = K::strlen(name);

    // go through entries
    uint32_t dir_size = size_in_bytes();
    Ext2_DirEntry* dir_entry = new Ext2_DirEntry{0};
    for (uint32_t offset = 0; offset < dir_size; offset += dir_entry->header.entry_len) {
        read_all(offset, sizeof(dir_entry->header), (char*)&dir_entry->header);

        // check skip or the length
        if (dir_entry->header.inumber == 0 || dir_entry->header.name_len != target_len) {
            continue;
        }

        // length matches, so check the name
        read_all(offset + sizeof(dir_entry->header), dir_entry->header.name_len, (char*)&dir_entry->name);
        dir_entry->name[dir_entry->header.name_len] = '\0';
        if (!K::streq(dir_entry->name, name)) {
            continue;
        }

        // found it
        uint32_t inumber = dir_entry->header.inumber;
        delete dir_entry;
        return inumber;
    }

    delete dir_entry;

    return 0;
}

uint32_t Node::entry_count() {
    if (!is_dir()) {
        Debug::panic("Called entry_count() on something that isn't a directory.\n");
    }
    count_entries_jit();
    return num_entries;
}

// ====================================================
// ====================== Ext2 ========================
// ====================================================

constexpr uint32_t CACHE_ASSOCIATIVITY = 8;
constexpr uint32_t CACHE_CAPACITY = 1024 * 1024;

Ext2::Ext2(Shared<Ide> ide) : superblock{}, root() {
    // load superblock
    ide->read(1024, superblock);

    // create cache
    cbr = Shared<CachedBlockReader>::make(ide, CACHE_ASSOCIATIVITY, get_block_size(), CACHE_CAPACITY);

    // read the block group descriptor table
    uint32_t bgdt_block = 1 + (get_block_size() == 1024);
    block_group_count = (superblock.blocks_count + (superblock.blocks_per_group - 1)) / superblock.blocks_per_group;
    block_groups_descriptor_table = new Ext2_BlockGroupDesc[block_group_count];
    cbr->read_all(bgdt_block * get_block_size(), block_group_count * sizeof(Ext2_BlockGroupDesc), (char*)block_groups_descriptor_table);

    // load root
    root = get_node(2);
}

Ext2::~Ext2() {
    delete[] block_groups_descriptor_table;
}

uint32_t Ext2::get_block_size() {
    return 1024 << superblock.log_block_size;
}

uint32_t Ext2::get_inode_size() {
    return 128;
}

Shared<Node> Ext2::get_node(uint32_t inumber) {
    if (inumber == 0 || inumber > superblock.inodes_count) {
        Debug::panic("Invalid inumber = %lu", inumber);
    }

    // find the block group
    uint32_t block_group_index = (inumber - 1) / superblock.inodes_per_group;
    uint32_t block_group_offset = (inumber - 1) % superblock.inodes_per_group;

    // load the block group
    uint32_t itable_pbn = block_groups_descriptor_table[block_group_index].inode_table_block;
    uint32_t inode_offset = itable_pbn * get_block_size() + block_group_offset * get_inode_size();

    Ext2_Inode inode;
    cbr->BlockIO::read(inode_offset, inode);

    Shared<Node> node = Shared<Node>::make(inumber, inode, cbr);
    return node;
}

Shared<Node> Ext2::find(Shared<Node> dir, const char* name) {
    uint32_t inumber = dir->find(name);
    if (inumber == 0) {
        return Shared<Node>::NUL;
    }
    return get_node(inumber);
}

Shared<Node> Ext2::find_by_path(Shared<Node> from, const char* path) {
    // check for empty string
    if (*path == '\0' || from == Shared<Node>::NUL) {
        return Shared<Node>::NUL;
    }

    // account for absolute possibility
    if (*path == '/') {
        from = root;
        while (*path == '/') path++;
    }

    // initliaze the cur
    Shared<Node> cur = from;

    // relatively traverse
    char* buffer = new char[256];

    while (cur != Shared<Node>::NUL && *path != '\0') {
        // read in the characters
        uint32_t len = 0;
        while (*path != '/' && *path != '\0') {
            buffer[len++] = *path;
            path++;
        }
        buffer[len] = '\0';
        while (*path == '/') path++;

        // search for the node
        cur = find(cur, buffer);
    }

    delete[] buffer;

    return cur;
}