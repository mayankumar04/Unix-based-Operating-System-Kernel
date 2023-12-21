#include "vmm.h"

#include "config.h"
#include "debug.h"
#include "idt.h"
#include "libk.h"
#include "machine.h"
#include "physmem.h"
#include "process.h"
#include "sys.h"
#include "window.h"

namespace SmartPMM
{

    const PageEntry PageEntry::NUL = PageEntry(PageNum::BAD_NUM, 0);

    PageRefCount *physical_page_reference_counts;
    SpinLock *physical_page_locks;

    void init_smart_pmm()
    {
        physical_page_reference_counts = new PageRefCount[page_num(page_up(kConfig.memSize))];
        physical_page_locks = new SpinLock[page_num(page_up(kConfig.memSize))];
    }

} // namespace SmartPMM

namespace SmartVMM
{

    using namespace Helper;

    PageDir default_page_dir{PageNum::BAD_NUM};
    RBTree<MMAPBlock *, NoLock> *default_mmap_tree{nullptr};

    namespace Helper
    {

        PageNum is_available_fixed(RBTree<MMAPBlock *, NoLock> *mmap_tree, PageNum start, uint32_t size)
        {
            // if these are the same, that means there are no blocks in the middle
            bool avail = true;
            mmap_tree->foreach_data(start, PageNum(start + size - 1), CompareFunction<PageNum, MMAPBlock *>(), [&avail](MMAPBlock *block)
                                    {
        avail = false;
        return false; });
            if (avail)
            {
                return start;
            }
            return PageNum::BAD_NUM;
        }

        PageNum is_available_first_fit(RBTree<MMAPBlock *, NoLock> *mmap_tree, PageNum search_start, PageNum search_end, uint32_t size)
        {
            // otherwise first fit
            MMAPBlock *prev = nullptr;
            PageNum out = PageNum::BAD_NUM;
            mmap_tree->foreach_data(search_start, PageNum(search_end - 1), CompareFunction<PageNum, MMAPBlock *>(), [=, &prev, &out](MMAPBlock *block)
                                    {
        PageNum spn = prev == nullptr ? search_start : PageNum(prev->start + prev->size);
        PageNum epn = block->start;
        if (spn + size <= epn) {
            out = spn;
            return false;
        }
        prev = block;
        return true; });
            // edge case for the end
            if (out == PageNum::BAD_NUM && prev != nullptr && (prev->start + prev->size + size < search_end))
            {
                out = prev->start + prev->size;
            }
            return out;
        }

        MMAPBlock *containing_block(RBTree<MMAPBlock *, NoLock> *mmap_tree, PageNum vpn)
        {
            return mmap_tree->search(vpn, CompareFunction<PageNum, MMAPBlock *>());
        }

        PageTable ensure_writeable_pt(RBTree<MMAPBlock *, NoLock> *mmap_tree, PageDir pd, PageNum vpn)
        {
            PageEntry &pde = pd[vpn.pdi()];

            // if the PT was not there, just get a blank one
            if (pde.flags().is_not(Flags::PRESENT))
            {
                SmartPhysPage<PageEntry> new_pt_page = get_smart_page<PageEntry>();
                pde.smart_set(PageEntry(new_pt_page.ppn(), Flags::ALL));
            }

            // if the PT was there, but it was read only, we have a COW case
            else if (pde.flags().is_not(Flags::READ_WRITE))
            {
                // get a place holder for the og_pt
                PageTable pt = pde.ppn();

                // get a space to possible save a new page
                SmartPhysPage<PageEntry> new_pt_page = get_smart_page<PageEntry>();

                // reset the entry and copy it if we can't claim it
                pde.smart_set(PageEntry::NUL, [&pt, &new_pt_page, mmap_tree, vpn](PageNum ppn, uint32_t ref)
                              {
            if (ref > 0) {
                // we need to copy the original one and add the references
                PageTable og_pt = pt;
                pt = new_pt_page.ppn();
                foreach_allocated_vpn(mmap_tree, vpn.pdi() * ENTRIES_PER_PAGE, ENTRIES_PER_PAGE, 0, 0, [&pt, &og_pt](PageNum pn, MMAPBlock* block) {
                    if (block->flags.is(Flags::MMAP_REAL)) {
                        pt[pn.pti()].smart_set(og_pt[pn.pti()]);
                    } else {
                        pt[pn.pti()].fake_set(og_pt[pn.pti()]);
                    }
                    return 1;
                });
            }
            return false; });

                // lazily mark it read only
                // NOTE : could optimize . if its a new pt, then no need to do this
                foreach_allocated_vpn(mmap_tree, vpn.pdi() * ENTRIES_PER_PAGE, ENTRIES_PER_PAGE, Flags::MMAP_REAL | Flags::MMAP_RW, Flags::MMAP_SHARED, [&pt](PageNum pn, MMAPBlock *block)
                                      {
            pt[pn.pti()].fake_set(PageEntry(pt[pn.pti()].ppn(), pt[pn.pti()].flags() - Flags::READ_WRITE));
            return 1; });

                // save the pt into the pde
                pde.smart_set(PageEntry(pt.ppn(), Flags::ALL));
            }

            // finally return the pt, as we now have a safe copy of it
            return pde.ppn();
        }

        PhysPage<char> ensure_data(RBTree<MMAPBlock *, NoLock> *mmap_tree, PageTable pt, PageNum vpn, bool writeable)
        {
            PageEntry &pte = pt[vpn.pti()];

            // if the data page was not there, just get a blank one
            if (pte.flags().is_not(Flags::PRESENT))
            {
                // compute the page entry flags
                MMAPBlock *block = containing_block(mmap_tree, vpn);
                ASSERT(block != nullptr);

                // initlaize to zero mpaping
                SmartPhysPage<char> new_ro_data_page = BadPageCache::zero_page;
                Flags flags_to_remove = Flags::READ_WRITE;

                // check if this is a file mapping and that we need to load from it
                if (block->file != Shared<Node>::NUL)
                {
                    PageNum mapped_page_idx = vpn - block->start;
                    uint32_t mapped_bytes = (block->file->size_in_bytes() > block->file_offset)
                                                ? K::min(block->file_size, block->file->size_in_bytes() - block->file_offset)
                                                : 0;
                    if (mapped_page_idx < page_num(page_up(mapped_bytes)))
                    {
                        // if we are aligned then we can just load whatever page directly from the page cache
                        // this is the base case for the recursive read_all() implemented in the page cache
                        // NOTE: could maybe add file_size == node->size_in_bytes() ?
                        if (block->flags.is_not(Flags::MMAP_F_UNALGN) &&
                            (block->flags.is_not(Flags::MMAP_F_TRUNC) || mapped_page_idx < page_num(mapped_bytes)))
                        {
                            ASSERT(page_down(block->file_offset) == block->file_offset);
                            new_ro_data_page = BadPageCache::get_ro_file_page(block->file, page_num(block->file_offset) + mapped_page_idx);
                        }

                        // if we need a partial paghe, then everything has to be read into a new page
                        else
                        {
                            new_ro_data_page = get_smart_page<char>();
                            uint32_t len = K::min(PAGE_SIZE, mapped_bytes - mapped_page_idx.to_address());
                            BadPageCache::read_all(block->file,
                                                   block->file_offset + mapped_page_idx.to_address(), len,
                                                   (char *)new_ro_data_page.address());
                            flags_to_remove = 0;
                        }
                    }
                }

                Flags pe_flags = (block->compute_page_entry_flags() - flags_to_remove) | Flags::PRESENT;

                // save the data into the pte. this is safe because we just got a new process
                pte.smart_set(PageEntry(new_ro_data_page.ppn(), pe_flags));
            }

            Flags old_pte_flags = pte.flags();

            // if the data page was there, but it was read only, we have a COW case
            if (writeable && pte.flags().is_not(Flags::READ_WRITE))
            {
                // get a place holder for the og_data
                PhysPage<char> data = pte.ppn();

                // get a space for the likely new data page
                SmartPhysPage<char> new_data_page = get_smart_page<char>();

                // reset the entry and copy it if we can't claim it
                pte.smart_set(PageEntry::NUL, [&data, &new_data_page, vpn](PageNum ppn, uint32_t ref)
                              {
            if (ref > 0) {
                // we need to copy the original one and add the references
                PhysPage<char> og_data = data;
                data = new_data_page.ppn();
                memcpy((char*)data.address(), (char*)og_data.address(), PAGE_SIZE);
            }
            return false; });

                // save the data into the pte, adding the RW
                pte.smart_set(PageEntry(data.ppn(), old_pte_flags | Flags::READ_WRITE));
            }

            // finally return the pt, as we now have a safe copy of it
            return pte.ppn();
        }

        bool remove_mapping(RBTree<MMAPBlock *, NoLock> *mmap_tree, PageDir pd, PageNum vpn, MMAPBlock *containing_mmap_block)
        {
            PageEntry &pde = pd[vpn.pdi()];
            if (pde.flags().is_not(Flags::PRESENT))
            {
                return false;
            }

            PageTable pt = pde.ppn();
            if (pt[vpn.pti()].flags().is_not(Flags::PRESENT))
            {
                return false;
            }

            // remove the mapping
            pt = ensure_writeable_pt(mmap_tree, pd, vpn);
            if (containing_mmap_block->compute_page_entry_flags().is(Flags::MMAP_REAL))
            {
                pt[vpn.pti()].smart_set(PageEntry::NUL);
            }
            else
            {
                pt[vpn.pti()].fake_set(PageEntry::NUL);
            }
            return true;
        }

    } // namespace Helper

    RBTree<MMAPBlock *, NoLock> *create_mmap_tree_like(RBTree<MMAPBlock *, NoLock> *mmap_tree)
    {
        return mmap_tree->deep_copy([](MMAPBlock *block)
                                    {
        MMAPBlock* new_block = new MMAPBlock(*block);
        return new_block; });
    }

    void destroy_mmap_tree(RBTree<MMAPBlock *, NoLock> *mmap_tree)
    {
        mmap_tree->clear([](MMAPBlock *block)
                         { delete block; });
        delete mmap_tree;
    }

    PageDir create_page_dir_like(PageDir pd_to_copy)
    {
        // mark the cow flags
        for (uint32_t pdi = 0; pdi < ENTRIES_PER_PAGE; pdi++)
        {
            pd_to_copy[pdi].smart_set(PageEntry(pd_to_copy[pdi].ppn(), pd_to_copy[pdi].flags() - Flags::READ_WRITE));
        }

        // make a copy of the pd
        PageDir pd = get_smart_page<PageEntry>();
        for (uint32_t pdi = 0; pdi < ENTRIES_PER_PAGE; pdi++)
        {
            pd[pdi].smart_set(pd_to_copy[pdi]);
        }

        if (current_cr3().ppn() == pd_to_copy.ppn())
        {
            vmm_on(getCR3());
        }

        return pd;
    }

    void destroy_page_dir(RBTree<MMAPBlock *, NoLock> *mmap_tree, PageDir pd)
    {
        // lets unref all the page tables atomically
        for (uint32_t pdi = 0; pdi < ENTRIES_PER_PAGE; pdi++)
        {
            PageEntry &pde = pd[pdi];

            pde.smart_set(PageEntry::NUL, [mmap_tree, pdi](PageNum ppn, uint32_t refs)
                          {
            // unref all the real data pages ONLY IF THIS IS LAST REFERENCE
            if (refs == 0) {
                PageTable pt = ppn;
                foreach_allocated_vpn(mmap_tree, pdi * ENTRIES_PER_PAGE, ENTRIES_PER_PAGE, Flags::MMAP_REAL, 0, [&pt](PageNum pn, MMAPBlock* block) {
                    pt[pn.pti()].smart_set(PageEntry::NUL);
                    return 1;
                });
            }

            return true; });
        }

        // no need to unref the PD as it is ref counted
    }

    void init_cr3(PageDir pd)
    {
        using namespace SmartPMM::Helper;
        ref_page(pd.ppn());
        vmm_on(pd.address());
    }

    PageDir fini_cr3()
    {
        using namespace SmartPMM::Helper;
        PageDir pd = current_cr3();
        vmm_off();
        unref_page(pd.ppn(), [](PageNum ppn, uint32_t refs)
                   { return true; });
        return pd;
    }

    PageDir current_cr3()
    {
        using namespace SmartPMM::Helper;
        return PageDir(page_num(getCR3()));
    }

    PageDir exchange_cr3(PageDir pd)
    {
        using namespace SmartPMM::Helper;
        PageDir old_pd = current_cr3();
        init_cr3(pd);
        unref_page(old_pd.ppn(), [](PageNum ppn, uint32_t refs)
                   { return true; });
        return old_pd;
    }

    void *mmap(RBTree<MMAPBlock *, NoLock> *mmap_tree, VirtualAddress va, uint32_t length, Flags flags, Shared<Node> file, uint32_t file_offset, uint32_t file_size)
    {
        // make sure the va is page aligned
        if (page_down(va) != va)
        {
            return (void *)0;
        }

        // if we have a file
        if (file != Shared<Node>::NUL)
        {
            // make sure the file offset is page aligned if we are not using unalign
            if (flags.is_not(Flags::MMAP_F_UNALGN) && page_down(file_offset) != file_offset)
            {
                return (void *)0;
            }

            // if its not truncated, we need to set it correctly
            if (flags.is_not(Flags::MMAP_F_TRUNC))
            {
                file_size = file->size_in_bytes();
            }
            file_size = K::min(file_size, file->size_in_bytes());
        }

        // lets require that no file has no info
        else
        {
            ASSERT(file_offset == 0);
            ASSERT(file_size == 0);
        }

        // round up the size
        uint32_t num_pages = page_num(page_up(length));

        PageNum place = PageNum::BAD_NUM;

        // fixed map
        if (flags.is(Flags::MMAP_FIXED))
        {
            if (flags.is(Flags::MMAP_USER) && !is_region_in_user_mem(va, va + page_up(length)))
            {
                return (void *)0;
            }
            place = is_available_fixed(mmap_tree, page_num(va), num_pages);
        }

        // we do a search (first fit)
        else
        {
            PageNum search_start = flags.is(Flags::MMAP_USER) ? page_num(VA_USER_START) : PageNum(0);
            PageNum search_end = flags.is(Flags::MMAP_USER) ? page_num(VA_USER_END) : PageNum(page_num(-1) + 1);
            place = is_available_first_fit(mmap_tree, search_start, search_end, num_pages);
        }

        if (place != PageNum::BAD_NUM)
        {
            MMAPBlock *block_to_add = new MMAPBlock(place, num_pages, flags, file, file_offset, file_size);
            ASSERT(mmap_tree->insert(block_to_add));
            return (void *)place.to_address();
        }

        return (void *)0;
    }

    bool munmap_containing_block(RBTree<MMAPBlock *, NoLock> *mmap_tree, PageDir pd, VirtualAddress va)
    {
        MMAPBlock *containing_allocated_block = mmap_tree->remove(page_num(va), CompareFunction<PageNum, MMAPBlock *>());
        if (containing_allocated_block == nullptr)
        {
            return false;
        }
        for (PageNum vpn = containing_allocated_block->start;
             vpn < containing_allocated_block->start + containing_allocated_block->size;)
        {
            if (pd[vpn.pdi()].flags().is_not(Flags::PRESENT))
            {
                vpn += ENTRIES_PER_PAGE;
            }
            Helper::remove_mapping(mmap_tree, pd, vpn, containing_allocated_block);
            vpn++;
        }
        if (pd.ppn() == Process::current().pd.ppn())
        {
            vmm_on(getCR3());
        }
        delete containing_allocated_block;
        return true;
    }

    bool handle_page_fault(RBTree<MMAPBlock *, NoLock> *mmap_tree, PageDir pd, VirtualAddress va, bool write_fault)
    {
        PageNum vpn = page_num(va);

        PageTable pt = ensure_writeable_pt(mmap_tree, pd, vpn);

        Helper::ensure_data(mmap_tree, pt, vpn, write_fault);

        if (getCR3() == pd.address())
        {
            invlpg(va);
        }
        return true;
    }

    bool check_user_page_fault(RBTree<MMAPBlock *, NoLock> *mmap_tree, uint32_t error_code, VirtualAddress va)
    {
        // check that its in user space
        if (!is_region_in_user_mem(va, va + 1))
        {
            return false;
        }

        // check that it is mapped
        bool is_allocated = false;
        foreach_allocated_vpn(mmap_tree, page_num(va), 1, 0, 0, [&is_allocated](PageNum vpn, MMAPBlock *block)
                              {
        is_allocated = true;
        return 0; });

        return is_allocated;
    }

    void init_smart_vmm()
    {
        default_mmap_tree = new RBTree<MMAPBlock *, NoLock>(nullptr);

        // build the default mappings
        ASSERT(mmap(default_mmap_tree, 0x1000, kConfig.memSize - 0x1000, Flags::MMAP_FIXED | Flags::MMAP_RW | Flags::MMAP_SHARED, Shared<Node>::NUL, 0, 0) == (void *)0x1000);
        ASSERT(mmap(default_mmap_tree, kConfig.ioAPIC, PAGE_SIZE, Flags::MMAP_FIXED | Flags::MMAP_RW | Flags::MMAP_SHARED, Shared<Node>::NUL, 0, 0) == (void *)kConfig.ioAPIC);
        ASSERT(mmap(default_mmap_tree, kConfig.localAPIC, PAGE_SIZE, Flags::MMAP_FIXED | Flags::MMAP_RW | Flags::MMAP_SHARED, Shared<Node>::NUL, 0, 0) == (void *)kConfig.localAPIC);
        ASSERT(mmap(default_mmap_tree, vbe_mode_block.framebuffer, DISPLAY_WIDTH*DISPLAY_HEIGHT*3, Flags::MMAP_FIXED | Flags::MMAP_RW | Flags::MMAP_SHARED, Shared<Node>::NUL, 0, 0) == (void *)vbe_mode_block.framebuffer);
        ASSERT(mmap(default_mmap_tree, VA_PROCESS, PAGE_SIZE, Flags::MMAP_FIXED | Flags::MMAP_RW | Flags::MMAP_REAL, Shared<Node>::NUL, 0, 0) == (void *)VA_PROCESS);

        // build the default page directory
        default_page_dir = get_smart_page<PageEntry>();

        for (PageNum pn = 1; pn < page_num(page_up(kConfig.memSize)); pn++)
        {
            PageTable pt = ensure_writeable_pt(default_mmap_tree, default_page_dir, pn);
            pt[pn.pti()].fake_set(PageEntry(pn, Flags::KERNEL));
        }

        PageNum ioapic_pn = page_num(kConfig.ioAPIC);
        PageTable ioapic_pt = ensure_writeable_pt(default_mmap_tree, default_page_dir, ioapic_pn);
        ioapic_pt[ioapic_pn.pti()].fake_set(PageEntry(ioapic_pn, Flags::KERNEL));

        PageNum lapic_pn = page_num(kConfig.localAPIC);
        PageTable lapic_pt = ensure_writeable_pt(default_mmap_tree, default_page_dir, lapic_pn);
        lapic_pt[lapic_pn.pti()].fake_set(PageEntry(lapic_pn, Flags::KERNEL));

        for( uint32_t i = vbe_mode_block.framebuffer; i < vbe_mode_block.framebuffer + DISPLAY_WIDTH*DISPLAY_HEIGHT*3; i += PAGE_SIZE) {
            PageNum buff_pn = page_num(i);
            PageTable buff_pt = ensure_writeable_pt(default_mmap_tree, default_page_dir, buff_pn);
            buff_pt[buff_pn.pti()].fake_set(PageEntry(buff_pn, Flags::KERNEL));
        }

        handle_page_fault(default_mmap_tree, default_page_dir, VA_PROCESS, true);
    }

} // namespace SmartVMM

namespace PageCache
{

    SpinLock BadPageCache::guard{};
    FixedHashMap<FilePage, PageEntry, NoLock> *BadPageCache::bad_pc_map{nullptr};
    SmartPhysPage<char> BadPageCache::zero_page{PageNum::BAD_NUM};

    void BadPageCache::init()
    {
        bad_pc_map = new FixedHashMap<FilePage, PageEntry, NoLock>(32027, PageEntry::NUL);
        zero_page = get_smart_page<char>();
    }

    SmartPhysPage<char> BadPageCache::get_ro_file_page(Shared<Node> node, PageNum off)
    {
        LockGuard{guard};
        FilePage fp = FilePage(node, off);
        PageEntry pe = bad_pc_map->get(fp);
        if (pe.flags().is_not(Flags::PRESENT))
        {
            SmartPhysPage<char> data_page = get_smart_page<char>();
            node->read_all(off.to_address(), PAGE_SIZE, (char *)data_page.address());
            pe.smart_set(PageEntry(data_page.ppn(), Flags::PRESENT));
            bad_pc_map->put(fp, pe);
        }
        return pe.ppn();
    }

    int64_t BadPageCache::read_all(Shared<Node> node, uint32_t offset, uint32_t len, void *buffer)
    {
        if (offset > node->size_in_bytes())
        {
            return -1;
        }
        if (offset == node->size_in_bytes())
        {
            return 0;
        }

        len = K::min(len, node->size_in_bytes() - offset);

        uint32_t aligned_offset = page_down(offset);
        uint32_t aligned_length = page_up(offset + len) - aligned_offset;
        char *file_location = (char *)mmap(PCB::current().mmap_tree, VA_PAGE_CACHE, aligned_length,
                                           Flags::MMAP_FIXED | Flags::MMAP_REAL | Flags::MMAP_RW, node, aligned_offset, -1);
        if (file_location == (char *)0)
        {
            return -1;
        }

        memcpy(buffer, file_location + (offset - aligned_offset), len);

        munmap_containing_block(PCB::current().mmap_tree, Process::current().pd, (VirtualAddress)file_location);

        return len;
    }

} // namespace PageCache

namespace VMM
{

    bool is_region_in_user_mem(VirtualAddress start, VirtualAddress end)
    {
        return start >= VA_USER_START && start <= end && end <= VA_USER_END;
    }

    // Called (on the initial core) to initialize data structures, etc
    void global_init()
    {
        init_smart_pmm();
        BadPageCache::init();
        init_smart_vmm();
    }

    // Called on each core to do per-core initialization
    void per_core_init()
    {
        init_cr3(default_page_dir);
    }

    extern "C" void vmm_pageFault(VirtualAddress va, RegisterState *regs)
    {
        // we need to look at the current mappings
        // Debug::printf("*** page fault at %x\n", va);
        RBTree<MMAPBlock *, NoLock> *current_mmap_tree = PCB::current().mmap_tree;

        // check if this is not a safe pagefault if we are coming from user mode
        if (regs->cs == userCS && !check_user_page_fault(current_mmap_tree, PCB::current().regs.error_code, va))
        {
            // check if this is implicit sigreturn
            if (va == SYS::Helper::VA_IMPLICIT_SIGRET)
            {
                SYS::Call::sigreturn();
            }

            // if the sigreturn failed, then we should continue the normal steps

            // try to call the user handler
            PCB::current().save_state(regs);
            SYS::Helper::try_user_exception_handler(1, va);

            // if this fails, then write to the faulting address
            *(VirtualAddress *)(VA_USER_SHARED_FAULT_ADDR) = va;
            SYS::Call::exit(139);
        }

        bool write_fault = Flags(regs->error_code).is(Flags::READ_WRITE);

        SmartVMM::handle_page_fault(current_mmap_tree, Process::current().pd, va, write_fault);
        // Debug::shutdown();
    }

} // namespace VMM