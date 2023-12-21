#pragma once

#include "block_io.h"
#include "events.h"
#include "ide.h"
#include "semaphore.h"
#include "shared.h"

/**
 * abstract the block
 */
class BlockReader : public BlockIO {
    Shared<Ide> ide;

   public:
    BlockReader(Shared<Ide> ide, uint32_t block_size);
    virtual ~BlockReader() = default;

    // we assume buffer is big enough for the block size
    virtual void read_block(uint32_t block_number, char* buffer) override;

    virtual uint32_t size_in_bytes() override;

    template <typename T>
    void read(uint32_t offset, T& thing) {
        auto cnt = read_all(offset, sizeof(T), (char*)&thing);
        ASSERT(cnt == sizeof(T));
    }
};

/**
 * abstracts the cache
 */
class CachedBlockReader : public BlockReader {
    uint32_t associativity;
    uint32_t capacity;
    uint32_t sets;
    char* data;
    uint32_t* line_meta;
    uint32_t* set_counts;
    SpinLock* set_locks;

    inline void mark_valid(uint32_t& offset);
    inline bool is_valid(uint32_t offset);
    inline uint32_t get_tag(uint32_t offset);
    inline uint32_t get_set_id(uint32_t offset);
    inline uint32_t is_present(uint32_t block_number);
    inline uint32_t maybe_evict_and_load(uint32_t block_number);

    void lock_corresponding_set(uint32_t block_number);
    void unlock_corresponding_set(uint32_t block_number);
    uint32_t get_block_line_id(uint32_t block_number);
    void read_block_data(uint32_t block_number, char* buffer, uint32_t base_block_offset, uint32_t bytes_to_read);

   public:
    // assumes that associativity, block size, and capacity are powers of two
    CachedBlockReader(Shared<Ide> ide, uint32_t associativity, uint32_t block_size, uint32_t capacity);
    virtual ~CachedBlockReader();

    // we assume buffer is big enough for the block size
    virtual void read_block(uint32_t block_number, char* buffer) override;

    // Read up to "n" bytes starting at "offset" and put the restuls in "buffer".
    // returns:
    //   > 0  actual number of bytes read
    //   = 0  end (offset == size_in_bytes)
    //   -1   error (offset > size_in_bytes)
    virtual int64_t read(uint32_t offset, uint32_t n, char* buffer) override;

    template <typename T>
    void read(uint32_t offset, T& thing) {
        auto cnt = read_all(offset, sizeof(T), (char*)&thing);
        ASSERT(cnt == sizeof(T));
    }
};