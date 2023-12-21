#include "cache.h"

#include "libk.h"

// ====================================================
// ====================== Block =======================
// ====================================================

BlockReader::BlockReader(Shared<Ide> ide, uint32_t block_size) : BlockIO(block_size), ide(ide) {
    ASSERT(block_size % ide->block_size == 0);
}

void BlockReader::read_block(uint32_t block_number, char* buffer) {
    uint32_t read_amount = ide->read_all(block_number * block_size, block_size, buffer);
    if (read_amount != block_size) {
        Debug::panic("Could not read full block. Likely an invalid block number %lu", block_number);
    }
}

uint32_t BlockReader::size_in_bytes() {
    return ide->size_in_bytes();
}

// ====================================================
// ====================== Cache =======================
// ====================================================

inline void CachedBlockReader::mark_valid(uint32_t& offset) {
    offset |= 0x1;
}

inline bool CachedBlockReader::is_valid(uint32_t offset) {
    return offset & 0x1;
}

inline uint32_t CachedBlockReader::get_tag(uint32_t offset) {
    return offset / block_size / sets;
}

inline uint32_t CachedBlockReader::get_set_id(uint32_t offset) {
    return offset / block_size % sets;
}

// returns the line id if the block number is in the cache. otherwise INT_MAX (-1)
inline uint32_t CachedBlockReader::is_present(uint32_t block_number) {
    uint32_t set_id = get_set_id(block_number * block_size);
    uint32_t tag = get_tag(block_number * block_size);

    // search set
    for (uint32_t i = set_id * associativity; i < (set_id + 1) * associativity; i++) {
        if (is_valid(line_meta[i]) && get_tag(line_meta[i]) == tag) {
            return i;
        }
    }

    return -1;
}

// return the line id with the loaded values from block number
inline uint32_t CachedBlockReader::maybe_evict_and_load(uint32_t block_number) {
    // we need to evict and load new copy
    uint32_t set_id = get_set_id(block_number * block_size);

    // let's look for the line to use
    uint32_t line_id = -1;

    // look for available
    for (uint32_t i = set_id * associativity; i < (set_id + 1) * associativity; i++) {
        if (!is_valid(line_meta[i])) {
            line_id = i;
            break;
        }
    }

    // maybe no availables, so evict a line
    if (line_id == (uint32_t)-1) {
        line_id = set_counts[set_id];
        set_counts[set_id] = (set_counts[set_id] + 1) % associativity;
    }

    // load the data
    BlockReader::read_block(block_number, data + (line_id * block_size));

    // update metadata
    line_meta[line_id] = block_number * block_size;
    mark_valid(line_meta[line_id]);

    return line_id;
}

void CachedBlockReader::lock_corresponding_set(uint32_t block_number) {
    uint32_t set_id = get_set_id(block_number * block_size);
    set_locks[set_id].lock();
}

void CachedBlockReader::unlock_corresponding_set(uint32_t block_number) {
    uint32_t set_id = get_set_id(block_number * block_size);
    set_locks[set_id].unlock();
}

// takes in the set_id to obtain a line. returns the line id.
// also locks the line to protect from concurrency
uint32_t CachedBlockReader::get_block_line_id(uint32_t block_number) {
    // check if its in the cache
    uint32_t present = is_present(block_number);

    if (present != (uint32_t)-1) {
        return present;
    }

    // forcefully load the block number
    return maybe_evict_and_load(block_number);
}

void CachedBlockReader::read_block_data(uint32_t block_number, char* buffer, uint32_t base_block_offset, uint32_t bytes_to_read) {
    lock_corresponding_set(block_number);
    uint32_t line_id = get_block_line_id(block_number);
    uint32_t data_offset = line_id * block_size + base_block_offset;
    memcpy(buffer, data + data_offset, bytes_to_read);
    unlock_corresponding_set(block_number);
}

CachedBlockReader::CachedBlockReader(Shared<Ide> ide, uint32_t associativity, uint32_t block_size, uint32_t capacity) : BlockReader(ide, block_size),
                                                                                                                 associativity(associativity),
                                                                                                                 capacity(capacity) {
    ASSERT(capacity % (associativity * block_size) == 0);
    sets = capacity / associativity / block_size;
    data = new char[capacity];
    bzero(data, capacity);
    line_meta = new uint32_t[sets * associativity];
    bzero(line_meta, sets * associativity * sizeof(uint32_t));
    set_counts = new uint32_t[sets];
    bzero(line_meta, sets * sizeof(uint32_t));
    set_locks = new SpinLock[sets]{};
}

CachedBlockReader::~CachedBlockReader() {
    delete[] data;
    delete[] line_meta;
    delete[] set_counts;
}

void CachedBlockReader::read_block(uint32_t block_number, char* buffer) {
    read_block_data(block_number, buffer, 0, block_size);
}

int64_t CachedBlockReader::read(uint32_t offset, uint32_t desired_n, char* buffer) {
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
    read_block_data(block_number, buffer, offset_in_block, actual_n);
    return actual_n;
}