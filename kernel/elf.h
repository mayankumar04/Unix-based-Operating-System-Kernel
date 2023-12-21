#ifndef _ELF_H_
#define _ELF_H_

#include "ext2.h"
#include "stdint.h"
#include "shared.h"

#define ELF_PH_TYPE_NULL 0
#define ELF_PH_TYPE_LOAD 1

class ELF {
   public:
    struct ElfHeader {
        unsigned char magic[4];  // should be 0x7f, E, L, F
        uint8_t cls;             // 1 -> 32 bit, 2 -> 64 bit
        uint8_t encoding;        // 1 -> LE, 2 -> BE
        uint8_t header_version;  // 1
        uint8_t abi;             // 0 -> Unix System V, 1 -> HP-UX
        uint8_t abi_version;     //
        uint8_t padding[7];

        uint16_t type;                       // 1 -> relocatable, 2 -> executable
        uint16_t machine;                    // 3 -> intel i386
        uint32_t version;                    // 1 -> current
        uint32_t entry;                      // program entry point
        uint32_t program_header_off;         // offset in file for program headers
        uint32_t section_header_off;         // offset in file for section headers
        uint32_t flags;                      //
        uint16_t elf_header_size;            // how many bytes in this header
        uint16_t program_header_entry_size;  // bytes per program header entry
        uint16_t num_program_headers;        // number of program header entries
        uint16_t section_header_entry_size;
        uint16_t num_section_headers;
        uint16_t shstrndx;

    } __attribute__((packed));

    struct ProgramHeader {
        uint32_t type;   /* 1 -> load, ... */
        uint32_t offset; /* data offset in the file */
        uint32_t vaddr;  /* Where should it be loaded in virtual memory */
        uint32_t paddr;  /* ignore */
        uint32_t filesz; /* how many bytes in the file */
        uint32_t memsz;  /* how many bytes in memory, result should be 0 */
        uint32_t flags;  /* 1 -> exec, 2 -> write, 4 -> read */
        uint32_t align;  /* alignment */
    } __attribute__((packed));

   private:
    static bool is_region_in_user_mem(uint32_t start, uint32_t end);

    static bool check_program_headers(Shared<Node> file, uint32_t num_program_headers, ProgramHeader* phs);
    static void handle_program_headers(Shared<Node> file, uint32_t num_program_headers, ProgramHeader* phs);

    static void load_program_header(Shared<Node> file, ProgramHeader& ph);

   public:
    // loads the file or returns 0 if it fails. (0 is fine, since user shouldn't be entering at address 0)
    static uint32_t load(Shared<Node> file);
};

#endif
