#pragma once

#include "ext2.h"
#include "shared.h"

// a class representing the File System
class FileSystem {
    static Shared<Ext2> fs;

   public:
    static void init(uint32_t drive);
    static Shared<Node> get_root();
    static void close();

    static Shared<Node> find_by_path(Shared<Node> from, const char* path);
};