#pragma once

#include "stdint.h"

class Flags {
    uint32_t bits;

   public:
    static const Flags PRESENT;
    static const Flags READ_WRITE;
    static const Flags USER_SUPERVISOR;
    static const Flags KERNEL;
    static const Flags ALL;

    static const Flags MMAP_REAL;
    static const Flags MMAP_RW;
    static const Flags MMAP_USER;
    static const Flags MMAP_SHARED;

    static const Flags MMAP_FIXED;     // do we map at the given address
    static const Flags MMAP_F_UNALGN;  // do we allow offset to be unaligned
    static const Flags MMAP_F_TRUNC;   // do we truncate the mapped file (zero fill the rest)

    static const Flags USER_FILE_READ;
    static const Flags USER_FILE_WRITE;

    inline Flags(uint32_t bits);

    inline uint32_t to_bits() const;

    // union
    inline const Flags operator|(const Flags& other) const;

    // intersection
    inline const Flags operator&(const Flags& other) const;

    // difference
    inline const Flags operator-(const Flags& other) const;

    // inverse
    inline const Flags operator~() const;

    // equals
    inline const bool operator==(const Flags& other) const;

    // is `*this` a superset of `flags`
    inline const bool is(Flags flags) const;

    // is `*this` not a superset of `flags`
    inline const bool is_not(Flags flags) const;
};

// +++ Flags

inline Flags::Flags(uint32_t bits) : bits(bits) {}

inline uint32_t Flags::to_bits() const {
    return bits;
}

inline const Flags Flags::operator|(const Flags& other) const {
    return Flags(bits | other.bits);
}

inline const Flags Flags::operator&(const Flags& other) const {
    return Flags(bits & other.bits);
}

inline const Flags Flags::operator-(const Flags& other) const {
    uint32_t mask = 0xFFF ^ other.to_bits();
    return Flags(bits & mask);
}

inline const Flags Flags::operator~() const {
    return Flags(~bits);
}

inline const bool Flags::operator==(const Flags& other) const {
    return to_bits() == other.to_bits();
}

inline const bool Flags::is(Flags flags) const {
    uint32_t fbits = flags.to_bits();
    uint32_t result = bits & fbits;
    return result == fbits;
}

inline const bool Flags::is_not(Flags flags) const {
    return !is(flags);
}