#include "kernel.h"

Node* checkFile(const char* name, Node* node) {
    CHECK(node != nullptr);
    CHECK(node->is_file());
    Debug::printf("file %s is ok\n",name);
    return node;
}

Node* getFile(Ext2* fs, Node* node, const char* name) {
    return checkFile(name,fs->find(node,name));
}

Node* checkDir(const char* name, Node* node) {
    Debug::printf("checking %s\n",name);
    CHECK (node != nullptr);
    CHECK (node->is_dir());
    Debug::printf("directory %s is ok\n",name);
    return node;
}

Node* getDir(Ext2* fs, Node* node, const char* name) {
    return checkDir(name,fs->find(node,name));
}

Ext2* fs;
extern PerCPU<pcb*> pcbs;

Future<int> kernelMain(void) {
    auto d = new Ide(1);
    Debug::printf("mounting drive 1\n");
    fs = new Ext2(d);
    auto root = checkDir("/",fs->root);
    auto sbin = getDir(fs,root,"sbin");
    auto init = getFile(fs,sbin,"init");

    Debug::printf("loading init\n");
    pcbs.mine() = new pcb();
    uint32_t e = ELF::load(init);
    Debug::printf("entry %x\n",e);
    uint32_t userEsp = 0xEFFFFFE0;
    Debug::printf("user esp %x\n",userEsp);
    // Current state:
    //     - %eip points somewhere in the middle of kernelMain
    //     - %cs contains kernelCS (CPL = 0)
    //     - %esp points in the middle of the thread's kernel stack
    //     - %ss contains kernelSS
    //     - %eflags has IF=1
    // Desired state:
    //     - %eip points at e
    //     - %cs contains userCS (CPL = 3)
    //     - %eflags continues to have IF=1
    //     - %esp points to the bottom of the user stack
    //     - %ss contain userSS
    // User mode will never "return" from switchToUser. It will
    // enter the kernel through interrupts, exceptions, and system
    // calls.
    ((uint32_t*)userEsp)[0] = 1;
    ((uint32_t*)userEsp)[1] = userEsp + 8;
    memcpy((char*)(userEsp + 8), "/sbin/init", 11);
    switchToUser(e,userEsp,0);
    Debug::panic("*** implement switchToUser in machine.S\n");
    co_return -1;
}
