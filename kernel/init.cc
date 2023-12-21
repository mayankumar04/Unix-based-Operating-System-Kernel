#include "init.h"

#include "config.h"
#include "crt.h"
#include "debug.h"
#include "events.h"
#include "filesystem.h"
#include "heap.h"
#include "idt.h"
#include "kernel.h"
#include "machine.h"
#include "physmem.h"
#include "pit.h"
#include "process.h"
#include "smp.h"
#include "stdint.h"
#include "sys.h"
#include "tss.h"
#include "u8250.h"
#include "vmm.h"
#include "window.h"
#include "keyboard.h"

struct KernelRuntimeStack {
    static constexpr int BYTES = 8192;
    uint8_t bytes[BYTES] __attribute__((aligned(16)));
};

PerCPU<KernelRuntimeStack> stacks;

static bool smpInitDone = false;

extern "C" uint32_t pickKernelStack(void) {
    return (uint32_t)&stacks.forCPU(smpInitDone ? SMP::me() : 0).bytes[KernelRuntimeStack::BYTES];
}

// using namespace WindowManagement;
// extern "C" void getVBEInfo(VbeInfoBlock *vbeInfoBlock);

static Atomic<uint32_t> howManyAreHere(0);

bool onHypervisor = true;

static constexpr uint32_t HEAP_START = 1 * 1024 * 1024;
static constexpr uint32_t HEAP_SIZE = 5 * 1024 * 1024;
static constexpr uint32_t VMM_FRAMES = HEAP_START + HEAP_SIZE;


extern "C" void kernelInit(void) {
    Debug::printf("This makes it hhere\n");
    U8250 uart;
    //Debug::printf("THe current key: %c\n", uart.get());

    if (!smpInitDone) {
        Debug::init(&uart);
        Debug::debugAll = false;
        Debug::printf("\n| What just happened? Why am I here?\n");

        {
            Debug::printf("| Discovering my identity and features\n");
            cpuid_out out;
            cpuid(0, &out);

            Debug::printf("|     CPUID: ");
            auto one = [](uint32_t q) {
                for (int i = 0; i < 4; i++) {
                    Debug::printf("%c", (char)q);
                    q = q >> 8;
                }
            };
            one(out.b);
            one(out.d);
            one(out.c);
            Debug::printf("\n");

            cpuid(1, &out);
            if (out.c & 0x1) {
                Debug::printf("|     has SSE3\n");
            }
            if (out.c & 0x8) {
                Debug::printf("|     has MONITOR/MWAIT\n");
            }
            if (out.c & 0x80000000) {
                onHypervisor = true;
                Debug::printf("|     running on hypervisor\n");
            } else {
                onHypervisor = false;
                Debug::printf("|     running on physical hardware\n");
            }
        }

        /* discover configuration */
        configInit(&kConfig);
        Debug::printf("| totalProcs %d\n", kConfig.totalProcs);
        Debug::printf("| memSize 0x%x %dMB\n",
                      kConfig.memSize,
                      kConfig.memSize / (1024 * 1024));
        Debug::printf("| localAPIC %x\n", kConfig.localAPIC);
        Debug::printf("| ioAPIC %x\n", kConfig.ioAPIC);

        /* initialize the heap */
        heapInit((void*)HEAP_START, HEAP_SIZE);

        /* switch to dynamically allocated UART */
        U8250* uart = new U8250; 
        Debug::init(uart);
        Debug::printf("| switched to new UART\n");

        /* running global constructors */
        CRT::init();

        /* initialize the filesystem*/
        FileSystem::init(1);

        /* initialize physmem */
        PhysMem::init(VMM_FRAMES, kConfig.memSize - VMM_FRAMES);

        /* global VMM initialization */
        VMM::global_init();

        /* initlaize the kernel process */
        ProcessManagement::global_init();

        /* initialize the text ui renderer */
        TextUI::init();

        UserFileIO::init_user_file_io();

        /* initialize LAPIC */
        SMP::init(true);
        smpInitDone = true;

        /* initialize system calls */
        SYS::init();

        /* initialize IDT */
        IDT::init();
        Pit::calibrate(1000);

        SMP::running.fetch_add(1);

        // The reset EIP has to be
        //     - divisible by 4K (required by LAPIC)
        //     - PPN must fit in 8 bits (required by LAPIC)
        //     - consistent with mbr.S
        for (uint32_t id = 1; id < kConfig.totalProcs; id++) {
            Debug::printf("| initialize %d\n", id);
            SMP::ipi(id, 0x4500);
            Debug::printf("| reset %d\n", id);
            Debug::printf("|      eip:0x%x\n", resetEIP);
            SMP::ipi(id, 0x4600 | (((uintptr_t)resetEIP) >> 12));
            while (SMP::running <= id)
                ;
        }
    } else {
        SMP::running.fetch_add(1);
        SMP::init(false);
    }

    // Per-core virtual memory initialization
    VMM::per_core_init();

    // per core process init
    ProcessManagement::per_core_init();

    // Initialize the PIT
    Pit::init();
    keyboard_init();


    auto id = SMP::me();

    Debug::printf("| initializing TSS for %d\n", id);
    tss[id].ss0 = kernelSS;
    ltr(tssDescriptorBase + id * 8);
    tss[id].esp0 = pickKernelStack();

    Debug::printf("| %d enabling interrupts, I'm scared\n", id);
    sti();

    auto myOrder = howManyAreHere.add_fetch(1);
    if (myOrder == kConfig.totalProcs) {
        auto f = kernelMain();
        f.get([](int v) {
            Debug::shutdown();
        });
    }
    event_loop();
}

extern "C" void graphicsInit(void) {
    // TODO: implement me!
    MISSING();
}

extern "C" void windowInit(void) {
    // TODO: implement me!
    MISSING();
}
