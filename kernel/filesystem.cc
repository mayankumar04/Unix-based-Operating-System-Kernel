#include "filesystem.h"

Shared<Ext2> FileSystem::fs {};

void FileSystem::init(uint32_t drive) {
    auto d = Shared<Ide>::make(drive);
    Debug::printf("mounting drive %d\n", drive);
    fs = Shared<Ext2>::make(d);
}

Shared<Node> FileSystem::get_root() {
    return fs->root;
}

void FileSystem::close() {
    fs = Shared<Ext2>();
}

Shared<Node> FileSystem::find_by_path(Shared<Node> from, const char* path) {
    return fs->find_by_path(from, path);
}