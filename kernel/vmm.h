#ifndef _VMM_H_
#define _VMM_H_

#include "debug.h"
#include "filesystem.h"
#include "flags.h"
#include "genericds.h"
#include "libk.h"
#include "physmem.h"
#include "shared.h"
#include "stdint.h"

using namespace Generic;

// ========================= MM ===========================
// a namespace containing general things about paged memory
// ========================================================

namespace MM {

// --------- constants ---------

constexpr uint32_t PAGE_SIZE = PhysMem::FRAME_SIZE;
constexpr uint32_t ENTRIES_PER_PAGE = PAGE_SIZE / sizeof(uint32_t);
constexpr uint32_t ENTRIES_PER_DIR = ENTRIES_PER_PAGE * ENTRIES_PER_PAGE;

// --------- types ---------

class PageNum;
typedef uint32_t MemoryAddress;

// --------- declarations ---------

class PageNum {
    uint32_t pn;

   public:
    static constexpr uint32_t BAD_NUM = -1;
    static inline PageNum bad();

    inline PageNum();
    inline PageNum(uint32_t pn);

    inline uint32_t pdi() const;
    inline uint32_t pti() const;
    inline MemoryAddress to_address() const;
    inline operator uint32_t&();
    inline operator const uint32_t&() const;
};

inline PageNum page_num(MemoryAddress addr);
inline MemoryAddress page_down(MemoryAddress addr);
inline MemoryAddress page_up(MemoryAddress addr);

// --------------- definitions --------------

// +++ PageNum

inline PageNum PageNum::bad() {
    return PageNum(BAD_NUM);
}

inline PageNum::PageNum() : pn(BAD_NUM) {}
inline PageNum::PageNum(uint32_t pn) : pn(pn) {}

inline uint32_t PageNum::pdi() const {
    return pn / ENTRIES_PER_PAGE;
}

inline uint32_t PageNum::pti() const {
    return pn % ENTRIES_PER_PAGE;
}

inline MemoryAddress PageNum::to_address() const {
    return pn * PAGE_SIZE;
}

inline PageNum::operator uint32_t&() {
    return pn;
}

inline PageNum::operator const uint32_t&() const {
    return pn;
}

// --------- interface ---------

inline PageNum page_num(MemoryAddress addr) {
    return addr / PAGE_SIZE;
}

inline MemoryAddress page_down(MemoryAddress addr) {
    return page_num(addr) * PAGE_SIZE;
}

inline MemoryAddress page_up(MemoryAddress addr) {
    return page_down(addr + PAGE_SIZE - 1);
}

}  // namespace MM

// ====================== SmartPMM ========================
// a namespace about things smart physical paged memory
// ========================================================

namespace SmartPMM {

using namespace MM;

// --------- types ---------

class PageRefCount;
class PageEntry;
typedef MemoryAddress PhysicalAddress;

template <typename T>
class PhysPage;

template <typename T>
class SmartPhysPage;

// --------- globals ---------

// the reference count array
extern PageRefCount* physical_page_reference_counts;
extern SpinLock* physical_page_locks;

namespace Helper {

// --------- functions ---------

inline void lock_page(PageNum ppn);
inline void unlock_page(PageNum ppn);

inline void ref_page(PageNum ppn);

template <typename Work>
inline void unref_page(PageNum ppn, Work callback);

}  // namespace Helper

class PageRefCount {
    Atomic<uint32_t> refs;

   public:
    inline PageRefCount();

    inline uint32_t inc();
    inline uint32_t dec();

    // FIXME : for debugging
    inline uint32_t get() {
        return refs.get();
    }
};

template <typename T>
class PhysPage {
   protected:
    PageNum pn;

   public:
    inline PhysPage();
    inline PhysPage(PageNum init_pn);

    inline PageNum ppn() const;
    inline PhysicalAddress address() const;

    inline T& operator[](const int index) const;
};

template <typename T>
class SmartPhysPage : public PhysPage<T> {
    using PhysPage<T>::pn;

    inline void smart_set(PageNum rpn);

   public:
    inline SmartPhysPage(PageNum init_pn);
    inline SmartPhysPage(const SmartPhysPage& rhs);
    inline SmartPhysPage(SmartPhysPage&& rhs);
    inline virtual ~SmartPhysPage();

    inline SmartPhysPage<T>& operator=(const SmartPhysPage<T>& rhs);
    inline SmartPhysPage<T>& operator=(SmartPhysPage<T>&& rhs);
};

class PageEntry {
   protected:
    uint32_t bits;

    inline void ref_entry();

    template <typename Work>
    inline void unref_entry(Work callback);

    inline void init(PageNum ppn, Flags flags);
    inline void init(uint32_t bits);

   public:
    static const PageEntry NUL;

    inline PageEntry(PageNum ppn, Flags flags);

    inline PageNum ppn() const;
    inline PhysicalAddress address() const;
    inline Flags flags() const;

    inline void fake_set(PageEntry pe);

    template <typename Work>
    inline void smart_set(PageEntry pe, Work callback);
    inline void smart_set(PageEntry pe);
};

template <typename T>
inline SmartPhysPage<T> get_smart_page();

extern void init_smart_pmm();

// --------- definitions ---------

/**
 * locks the physical page
 */
inline void Helper::lock_page(PageNum ppn) {
    ASSERT(ppn >= 0 && ppn < page_num(kConfig.memSize));
    physical_page_locks[ppn].lock();
}

/**
 * unlocks the physical page
 */
inline void Helper::unlock_page(PageNum ppn) {
    ASSERT(ppn >= 0 && ppn < page_num(kConfig.memSize));
    physical_page_locks[ppn].unlock();
}

/**
 * marks the ppn with a reference.
 */
inline void Helper::ref_page(PageNum ppn) {
    if (ppn == PageNum::bad()) {
        return;
    }

    lock_page(ppn);
    physical_page_reference_counts[ppn].inc();
    unlock_page(ppn);
}

/**
 * marks the ppn with a reference.
 */
template <typename Work>
inline void Helper::unref_page(PageNum ppn, Work callback) {
    if (ppn == PageNum::bad()) {
        return;
    }

    lock_page(ppn);
    uint32_t ref = physical_page_reference_counts[ppn].dec();

    bool del = callback(ppn, ref);

    if (del && ref == 0) {
        PhysMem::dealloc_frame(ppn.to_address());
    }
    unlock_page(ppn);
}

// +++ PageRefCount

inline PageRefCount::PageRefCount() : refs(0) {}

inline uint32_t PageRefCount::inc() {
    uint32_t r = refs.add_fetch(1);
    ASSERT(r != 0);
    return r;
}

inline uint32_t PageRefCount::dec() {
    uint32_t r = refs.add_fetch(-1);
    ASSERT(r != (uint32_t)-1);
    return r;
}

// +++ PhysPage

template <typename T>
inline PhysPage<T>::PhysPage() : PhysPage(0) {}

template <typename T>
inline PhysPage<T>::PhysPage(PageNum init_pn) : pn(init_pn) {}

template <typename T>
inline PageNum PhysPage<T>::ppn() const {
    return pn;
}

template <typename T>
inline PhysicalAddress PhysPage<T>::address() const {
    return pn.to_address();
}

template <typename T>
inline T& PhysPage<T>::operator[](const int index) const {
    ASSERT(pn != PageNum::BAD_NUM);
    ASSERT(index >= 0 && index < (int)(PAGE_SIZE / sizeof(T)));
    T* data = (T*)address();
    return data[index];
}

// +++ SmartPhysPage

template <typename T>
inline void SmartPhysPage<T>::smart_set(PageNum rpn) {
    using namespace Helper;
    if (pn != rpn) {
        unref_page(pn, [](PageNum ppn, uint32_t refs) { return true; });
        pn = rpn;
        ref_page(pn);
    }
}

template <typename T>
inline SmartPhysPage<T>::SmartPhysPage(PageNum init_pn) : PhysPage<T>(PageNum::BAD_NUM) {
    smart_set(init_pn);
}

template <typename T>
inline SmartPhysPage<T>::SmartPhysPage(const SmartPhysPage<T>& rhs) : SmartPhysPage<T>(rhs.ppn()) {}

template <typename T>
inline SmartPhysPage<T>::SmartPhysPage(SmartPhysPage<T>&& rhs) : SmartPhysPage<T>(rhs.ppn()) {
    rhs.smart_set(PageNum::BAD_NUM);
}

template <typename T>
inline SmartPhysPage<T>::~SmartPhysPage() {
    smart_set(PageNum::BAD_NUM);
}

template <typename T>
inline SmartPhysPage<T>& SmartPhysPage<T>::operator=(const SmartPhysPage<T>& rhs) {
    smart_set(rhs.ppn());
    return *this;
}

template <typename T>
inline SmartPhysPage<T>& SmartPhysPage<T>::operator=(SmartPhysPage<T>&& rhs) {
    smart_set(rhs.ppn());
    rhs.smart_set(PageNum::BAD_NUM);
    return *this;
}

// +++ PageEntry

// only reference if its present
inline void PageEntry::ref_entry() {
    using namespace SmartPMM::Helper;
    if (flags().is(Flags::PRESENT)) {
        ref_page(ppn());
    }
}

// only unreference if its present
template <typename Work>
inline void PageEntry::unref_entry(Work callback) {
    using namespace SmartPMM::Helper;
    if (flags().is(Flags::PRESENT)) {
        unref_page(ppn(), callback);
    }
}

inline void PageEntry::init(PageNum ppn, Flags flags) {
    // if this is a bad page num, we should just 0 it
    if (ppn == PageNum::BAD_NUM) {
        ppn = 0;
        flags = 0;
    }
    ASSERT((ppn & 0xfffff) == ppn);
    bits = (ppn * PAGE_SIZE) | flags.to_bits();
}

inline void PageEntry::init(uint32_t bits) {
    init(bits >> 12, bits & 0xfff);
}

inline PageEntry::PageEntry(PageNum ppn, Flags flags) {
    init(ppn, flags);
}

inline PageNum PageEntry::ppn() const {
    return bits / PAGE_SIZE;
}

inline PhysicalAddress PageEntry::address() const {
    return ppn() * PAGE_SIZE;
}

inline Flags PageEntry::flags() const {
    return Flags(bits % PAGE_SIZE);
}

inline void PageEntry::fake_set(PageEntry pe) {
    init(pe.bits);
}

template <typename Work>
inline void PageEntry::smart_set(PageEntry pe, Work callback) {
    // if any of the essential state is different, update it
    if (ppn() != pe.ppn() || flags().is_not(Flags::PRESENT) || pe.flags().is_not(Flags::PRESENT)) {
        unref_entry(callback);
        init(pe.bits);
        ref_entry();
    }
    // otherwise update flags
    else {
        init(ppn(), pe.flags());
    }
}

inline void PageEntry::smart_set(PageEntry pe) {
    smart_set(pe, [](PageNum ppn, uint32_t ref) { return true; });
}

// --------- interface ---------

/**
 * gets a smart physical page that is auto reference counted
 */
template <typename T>
inline SmartPhysPage<T> get_smart_page() {
    PageNum ppn = page_num(PhysMem::alloc_frame());
    return SmartPhysPage<T>(ppn);
}

/**
 * clones a given smart page
 */
template <typename T>
inline SmartPhysPage<T> clone_page(SmartPhysPage<T> old_page) {
    SmartPhysPage<T> new_page = get_smart_page<T>();
    memcpy((char*)(new_page.address()),
           (char*)(old_page.address()),
           PAGE_SIZE);
    return new_page;
}

}  // namespace SmartPMM

namespace SmartVMM {

using namespace SmartPMM;

/**
 * =================================================================
 * ============================= Types =============================
 * =================================================================
 */

typedef MemoryAddress VirtualAddress;

namespace PDTypes {

// ------------------- declarations --------------------

typedef PhysPage<PageEntry> PageTable;
typedef SmartPhysPage<PageEntry> PageDir;

}  // namespace PDTypes

namespace MMAPTypes {

// ------------------- declarations --------------------

struct MMAPBlock;

// ------------------- definitions --------------------

struct MMAPBlock {
    // ====== critical data ======

    // the starting vpn of this region
    PageNum start;

    // the size of this region in pages
    uint32_t size;

    // the flags for this region
    Flags flags;

    // ====== potential data ======

    // the file that is mapped
    Shared<Node> file;

    // the offset in the file that the mapping starts
    uint32_t file_offset;

    // the size of the file mapped
    uint32_t file_size;

    inline MMAPBlock(PageNum start,
                     uint32_t size,
                     Flags flags);

    inline MMAPBlock(PageNum start,
                     uint32_t size,
                     Flags flags,
                     Shared<Node> file,
                     uint32_t file_offset,
                     uint32_t file_size);

    inline Flags compute_page_entry_flags();
};

inline MMAPBlock::MMAPBlock(PageNum start,
                            uint32_t size,
                            Flags flags) : MMAPBlock(start, size, flags, Shared<Node>(), 0, 0) {}

inline MMAPBlock::MMAPBlock(PageNum start,
                            uint32_t size,
                            Flags flags,
                            Shared<Node> file,
                            uint32_t file_offset,
                            uint32_t file_size) : start(start), size(size), flags(flags), file(file), file_offset(file_offset), file_size(file_size) {
    ASSERT(size > 0);
}

inline Flags MMAPBlock::compute_page_entry_flags() {
    Flags pe_flags = 0;
    pe_flags = pe_flags | ((flags.is(Flags::MMAP_RW)) ? Flags::READ_WRITE : 0);
    pe_flags = pe_flags | ((flags.is(Flags::MMAP_USER)) ? Flags::USER_SUPERVISOR : 0);
    return pe_flags;
}

}  // namespace MMAPTypes

using namespace PDTypes;
using namespace MMAPTypes;

/**
 * =================================================================
 * ======================= Private Func+Data =======================
 * =================================================================
 */

namespace Helper {

/**
 * sees if the fixed range is available
 */
extern PageNum is_available_fixed(RBTree<MMAPBlock*, NoLock>* mmap_tree, PageNum start, uint32_t size);

/**
 * searchees first fit in an mmap tree in a specfic vpn range for a block of a certain size
 */
extern PageNum is_available_first_fit(RBTree<MMAPBlock*, NoLock>* mmap_tree, PageNum search_start, PageNum search_end, uint32_t size);

// find the containing block
extern MMAPBlock* containing_block(RBTree<MMAPBlock*, NoLock>* mmap_tree, PageNum vpn);

/**
 * for each vpn that is allocated and has flags that are a superset of `include`,
 * and disjoint from `exclude`, calls the work like `work(PageNum vpn, MMAPBlock* containing_block)`
 */
template <typename Work>
void foreach_allocated_vpn(RBTree<MMAPBlock*, NoLock>* mmap_tree, PageNum start, uint32_t size, Flags include, Flags exclude, Work work);

/**
 * ensure that the page table is in the PD, allocating one if its not there, or handling COW
 */
extern PageTable ensure_writeable_pt(RBTree<MMAPBlock*, NoLock>* mmap_tree, PageDir pd, PageNum vpn);

/**
 * assumes that the data page is supposed to be read write and the pt is safe to change
 * ensures that the page is in the PT, allocating one if its not there, or cloning it for COW
 */
extern PhysPage<char> ensure_data(RBTree<MMAPBlock*, NoLock>* mmap_tree, PageTable pt, PageNum vpn, bool writeable);

/**
 * removes a data page from a page table. returns whether a page was removed
 */
extern bool remove_mapping(RBTree<MMAPBlock*, NoLock>* mmap_tree, PageDir pd, PageNum vpn, MMAPBlock* containing_mmap_block);

}  // namespace Helper

/**
 * =================================================================
 * ======================= Public Interface ========================
 * =================================================================
 */

extern PageDir default_page_dir;
extern RBTree<MMAPBlock*, NoLock>* default_mmap_tree;

/**
 * creates a new mmap tree like the given one (deepcopies)
 */
extern RBTree<MMAPBlock*, NoLock>* create_mmap_tree_like(RBTree<MMAPBlock*, NoLock>* mmap_tree);

/**
 * destroy a mmap tree, deallocating memory as needed
 */
extern void destroy_mmap_tree(RBTree<MMAPBlock*, NoLock>* mmap_tree);

/**
 * creates a new page dir that is like the given one, assuming COW
 * basically just shallow copies the given PD while marking both with COW Read only
 */
extern PageDir create_page_dir_like(PageDir pd_to_copy);

/**
 * destroy a page directory, deallocating memory as needed
 */
extern void destroy_page_dir(RBTree<MMAPBlock*, NoLock>* mmap_tree, PageDir pd);

/**
 * initializes the cr3 pd
 */
extern void init_cr3(PageDir pd);

/**
 * finishes the cr3, returning the pd and turning vmm off
 */
extern PageDir fini_cr3();

/**
 * get the current cr3 PD
 */
extern PageDir current_cr3();

/**
 * exchanges the cr3 PD with this one
 */
extern PageDir exchange_cr3(PageDir pd);

/**
 * tries to map a virtual address
 */
extern void* mmap(RBTree<MMAPBlock*, NoLock>* mmap_tree, VirtualAddress va, uint32_t length, Flags flags, Shared<Node> file, uint32_t file_offset, uint32_t file_size);

/**
 * munmaps a regions
 */
extern bool munmap_containing_block(RBTree<MMAPBlock*, NoLock>* mmap_tree, PageDir pd, VirtualAddress va);

/**
 * handles a pagefault, returning whether it was a success or not
 */
extern bool handle_page_fault(RBTree<MMAPBlock*, NoLock>* mmap_tree, PageDir pd, VirtualAddress va, bool write_fault);

/**
 * checks that a pagefault is safe for the user
 */
extern bool check_user_page_fault(RBTree<MMAPBlock*, NoLock>* mmap_tree, uint32_t error_code, VirtualAddress va);

/**
 * initializes smart vmm
 */
extern void init_smart_vmm();

}  // namespace SmartVMM

template <>
struct Generic::CompareFunction<MM::PageNum, SmartVMM::MMAPTypes::MMAPBlock*> {
    int32_t operator()(MM::PageNum pn, SmartVMM::MMAPTypes::MMAPBlock* block) {
        if (pn < block->start) {
            return -1;
        }
        if (pn >= block->start + block->size) {
            return 1;
        }
        return 0;
    }
};

template <>
struct Generic::CompareFunction<SmartVMM::MMAPTypes::MMAPBlock*, SmartVMM::MMAPTypes::MMAPBlock*> {
    int32_t operator()(SmartVMM::MMAPTypes::MMAPBlock* b1, SmartVMM::MMAPTypes::MMAPBlock* b2) {
        if (b1->start + b1->size <= b2->start) {
            return -1;
        }
        if (b1->start >= b2->start + b2->size) {
            return 1;
        }
        return 0;
    }
};

template <typename Work>
void SmartVMM::Helper::foreach_allocated_vpn(RBTree<MMAPBlock*, NoLock>* mmap_tree, PageNum start, uint32_t size, Flags include, Flags exclude, Work work) {
    PageNum vpn = start;

    mmap_tree->foreach_data(start, PageNum(start + size), CompareFunction<PageNum, MMAPBlock*>(), [=, &vpn](MMAPBlock* curr) {
        // check the include and exclude flags
        if (curr->flags.is_not(include) || (~exclude).is_not(curr->flags)) {
            return true;
        }

        // figure out which pages are available
        vpn = K::max(curr->start, vpn);
        PageNum to = K::min(curr->start + curr->size, start + size);

        // finally call the callback
        while (vpn < to) {
            uint32_t ret = work(vpn, curr);
            if (ret == 0) {
                return false;
            } else {
                vpn += ret;
            }
        }
        return true;
    });
}

namespace PageCache {

using namespace SmartVMM;

struct FilePage;
class BadPageCache;

struct FilePage {
    // the file - null if anon
    Shared<Node> file;

    // the page id
    PageNum file_page_offset;
};

class BadPageCache {
    static SpinLock guard;
    static FixedHashMap<FilePage, PageEntry, NoLock>* bad_pc_map;

    BadPageCache() = delete;
    ~BadPageCache() = delete;

   public:
    static SmartPhysPage<char> zero_page;

    static void init();

    /**
     * get a page that has the data of a file in it
     */
    static SmartPhysPage<char> get_ro_file_page(Shared<Node> node, PageNum off);

    /**
     * reads data from a file. assumes that mmap without MMAP_F_UNALGN does not use this
     * (relies on mmap). this uses the current process
     */
    static int64_t read_all(Shared<Node> node, uint32_t offset, uint32_t len, void* buffer);
};

}  // namespace PageCache

template <>
struct Generic::HashFunction<PageCache::FilePage> {
    uint32_t operator()(PageCache::FilePage v) {
        uint32_t val = v.file->number;
        val = val * 193 + v.file_page_offset;
        return val;
    }
};

template <>
struct Generic::EqualFunction<PageCache::FilePage> {
    bool operator()(PageCache::FilePage v1, PageCache::FilePage v2) {
        return v1.file->number == v2.file->number && v1.file_page_offset == v2.file_page_offset;
    }
};

namespace VMM {

using namespace PageCache;

constexpr VirtualAddress VA_USER_START = 0x80000000;
constexpr VirtualAddress VA_USER_PRIVATE_START = VA_USER_START;
constexpr VirtualAddress VA_USER_PRIVATE_END = 0xF0000000;
constexpr VirtualAddress VA_USER_SHARED_START = 0xF0000000;
constexpr VirtualAddress VA_USER_SHARED_FAULT_ADDR = 0xF0000800;
constexpr VirtualAddress VA_USER_SHARED_END = 0xF0001000;
constexpr VirtualAddress VA_USER_END = 0xF0001000;

constexpr VirtualAddress VA_PAGE_CACHE = 0x20000000;

extern bool is_region_in_user_mem(VirtualAddress start, VirtualAddress end);

// Called (on the initial core) to initialize data structures, etc
extern void global_init();

// Called on each core to do per-core initialization
extern void per_core_init();

}  // namespace VMM

#endif
