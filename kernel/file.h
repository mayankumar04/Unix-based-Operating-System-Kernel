#pragma once

#include "atomic.h"
#include "barrier.h"
#include "bb.h"
#include "filesystem.h"
#include "flags.h"
#include "shared.h"
#include "stdint.h"
#include "texteditor.h"

/**
 * a namespace inteded for user by user processes as they can block
 * the ker`nel should use the other stronger interfaces that this is built on
 * none of these functions are allowed to block, but they may return a sync handle
 * that is used to signify that the tasks are ready to complete
 */
namespace UserFileIO {

enum UserFileType {
    NODE = 0,
    TERMINAL = 1,
    PIPE = 2,
    TUI = 3
};

/**
 * the interface provided by a userfile
 */
struct UserFile {
    virtual ~UserFile();

    /**
     * reads len bytes into the buffer from this file at offset
     */
    virtual int64_t do_read(uint32_t len, void* buffer) = 0;

    /**
     * writes len bytes from the buffer to this file at offset
     */
    virtual int64_t do_write(uint32_t len, void* buffer) = 0;

    /**
     * returns the type of this file
     */
    virtual UserFileType type() = 0;

    /**
     * returns the length of this file
     */
    virtual int64_t len();
};

struct UserFileContainer {
    UserFile* ptr;

    /**
     * the provided user file is intended to be consumed by this file
     * this means that you should not delete the uf after creating an OpenFile with it
     */
    UserFileContainer(UserFile* ptr);
    ~UserFileContainer();
};

/**
 * an open file for users, with automatic shared uses handled
 */
struct OpenFile {
    Shared<UserFileContainer> uf;
    Flags perms;

    OpenFile();
    OpenFile(Shared<UserFileContainer> uf, Flags perms);

    /**
     * returns the type of this file
     */
    UserFileType type();

    /**
     * returns the length of this file
     */
    int64_t len();

    /**
     * checks permissions and reads len bytes into the buffer from this file at offset
     */
    int64_t read(uint32_t len, void* buffer);

    /**
     * checks permissions and writes len bytes from the buffer to this file at offset
     */
    int64_t write(uint32_t len, void* buffer);
};

class NodeFile : public UserFile {
    SpinLock guard;
    uint32_t offset;

   public:
    Shared<Node> node;

    NodeFile(Shared<Node> node);
    virtual ~NodeFile();

    virtual UserFileType type() override;
    virtual int64_t do_read(uint32_t len, void* buffer) override;
    virtual int64_t do_write(uint32_t len, void* buffer) override;
    virtual int64_t len() override;
};

struct TerminalFile : public UserFile {
    BoundedBuffer<char> bb;

    TerminalFile();
    virtual ~TerminalFile();

    virtual UserFileType type() override;
    virtual int64_t do_read(uint32_t len, void* buffer) override;
    virtual int64_t do_write(uint32_t len, void* buffer) override;
};

class PipeFile : public UserFile {
    BoundedBuffer<char> bb;

   public:
    PipeFile();
    virtual ~PipeFile();

    virtual UserFileType type() override;
    virtual int64_t do_read(uint32_t len, void* buffer) override;
    virtual int64_t do_write(uint32_t len, void* buffer) override;
};

    class TUIFile : public UserFile {
    public:
        BoundedBuffer<char> data_bb;
        BoundedBuffer<char> display_bb;
        uint32_t write_offset;
        uint32_t read_offset;

        TUIFile();
        virtual ~TUIFile();

        virtual UserFileType type() override;
        virtual int64_t do_read(uint32_t len, void* buffer) override;
        virtual int64_t do_write(uint32_t len, void* buffer) override;
    };

    extern Shared<UserFileContainer> terminal;
    extern OpenFile stdin;
    extern OpenFile stdout;
    extern OpenFile stderr;

    extern void init_user_file_io();
    extern OpenFile get_active_tui();

}  // namespace UserFileIO