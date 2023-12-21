#include <coroutine>

#include "config.h"
#include "debug.h"
#include "elf.h"
#include "ext2.h"
#include "future.h"
#include "ide.h"
#include "libk.h"
#include "machine.h"
#include "process.h"
#include "sys.h"
#include "window.h"
#include "u8250.h"
#include "debug.h"

// #include "smartvmm.h"

#define INIT_PROGRAM_PATH "/sbin/init"

// using namespace TextGUI;
// TextEditor *editor;

Future<int> kernelMain(void) {
    using namespace ProcessManagement;
    using namespace Generic;

    // editor = new TextEditor();
    // editor->fill_background(80 * 25, 0xFFFF);

    //Debug::printf("TextEditor initialized\n");
    //TextUI::init();

    Process init_process = Process::create_like(default_kernel_process);
    Process::change(init_process);

    PCB::current().working_directory = FileSystem::get_root();
    PCB::current().user_files[0] = stdin;
    PCB::current().user_files[1] = stdout;
    PCB::current().user_files[2] = stderr;

    char* program_path = new char[sizeof(INIT_PROGRAM_PATH)];
    memcpy(program_path, INIT_PROGRAM_PATH, sizeof(INIT_PROGRAM_PATH));
    char** argv = new char*[1];
    argv[0] = 0;

    SYS::Call::execl(program_path, argv);
    Debug::panic("Error while loading " INIT_PROGRAM_PATH "\n");

    co_return -1;
}
