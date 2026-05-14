#pragma once

#include <cstdint>
#include <vector>
#include <stdexcept>
#include <cstring>
#include "bit_man.cpp"

class DodEncoder {
    bool first;
    bool second;
    uint64_t prevT;
    int64_t prevD;

public:
    DodEncoder() : first(0), second(0), prevT(0), prevD(0) {}

    // convert signed to unsigned 
    uint64_t encodeSigned(int64_t value, int bits) {
        return static_cast<uint64_t>(value) & ((1ULL << bits) - 1);
    }
    // encode and write timestamp
    void writeTimestamp(uint64_t ts, BitWriter* bw) {
        if (!first) {
            bw->write(ts, 64);
            prevT = ts;
            first = true;
            return;
        }
        else if (!second) {
            int64_t delta = static_cast<int64_t>(ts - prevT);
            bw->write(encodeSigned(delta, 14), 14);
            prevD = delta;
            prevT = ts;
            second = true;
            return;
        }

        int64_t delta = static_cast<int64_t>(ts - prevT);
        int64_t dod = delta - prevD;

        if (dod == 0) {
            bw->write(0, 1);
        }
        else if (dod >= -63 && dod <= 64) {
            bw->write(0b10, 2);
            bw->write(encodeSigned(dod, 7), 7);
        }
        else if (dod >= -255 && dod <= 256) {
            bw->write(0b110, 3);
            bw->write(encodeSigned(dod, 9), 9);
        }
        else if (dod >= -2047 && dod <= 2048) {
            bw->write(0b1110, 4);
            bw->write(encodeSigned(dod, 12), 12);
        }
        else {
            bw->write(0b1111, 4);
            bw->write(encodeSigned(dod, 32), 32);
        }
        prevD = delta;
        prevT = ts;
    }
};

class DodDecoder {
    bool first;
    bool second;
    uint64_t prevT;
    int64_t prevD;

public:
    DodDecoder() : first(0), second(0), prevT(0), prevD(0) {}

    // convert unsigned to signed 
    int64_t decodeSigned(uint64_t raw, int bits) {
        if ((raw >> (bits - 1)) & 1) {
            uint64_t mask = (1ULL << bits) - 1;
            return static_cast<int64_t>(raw | ~mask);
        }
        else {
            return static_cast<int64_t>(raw);
        }
    }
    // read and decode timestamp
    uint64_t readTimestamp(BitReader* br) {
        if (!first) {
            prevT = br->read(64);
            first = true;
            return prevT;
        }
        else if (!second) {
            uint64_t raw = br->read(14);
            int64_t delta = decodeSigned(raw, 14);
            prevT = prevT + delta;
            prevD = delta;
            second = true;
            return prevT;
        }
        int64_t dod = 0;

        if (br->read(1) == 0) {
            dod = 0;
        }
        else if (br->read(1) == 0) {
            uint64_t raw = br->read(7);
            dod = decodeSigned(raw, 7);
        }
        else if (br->read(1) == 0) {
            uint64_t raw = br->read(9);
            dod = decodeSigned(raw, 9);
        }
        else if (br->read(1) == 0) {
            uint64_t raw = br->read(12);
            dod = decodeSigned(raw, 12);
        }
        else {
            uint64_t raw = br->read(32);
            dod = decodeSigned(raw, 32);
        }
        
        int64_t delta = prevD + dod;
        prevT = prevT + delta;
        prevD = delta;
        return prevT;
    }
};

class XorEncoder {
    bool first;
    uint64_t prevBits;
    int prevLead;
    int prevTrail;

public:
    XorEncoder() : first(false), prevBits(0), prevLead(0), prevTrail(0) {}

    uint64_t doubleToBits(double d) {
        uint64_t bits;
        std::memcpy(&bits, &d, sizeof(double));
        return bits;
    }

    int leadingZeros(uint64_t x) {
        if (x == 0) return 64;
        return __builtin_clzll(x);
    }

    int trailingZeros(uint64_t x) {
        if (x == 0) return 64;
        return __builtin_ctzll(x);
    }

    void writeValue(double value, BitWriter* bw) {
        uint64_t currBits = doubleToBits(value);
        if (!first) {
            bw->write(currBits, 64);
            prevBits = currBits;
            first = true;
            return;
        }
        uint64_t xorBits = currBits ^ prevBits;

        if (xorBits == 0) {
            bw->write(0, 1);
        } else {
            int leading = leadingZeros(xorBits);
            int trailing = trailingZeros(xorBits);
            int meaningful = 64 - leading - trailing;

            if (leading >= prevLead && trailing >= prevTrail &&
                (prevLead + prevTrail <= 64 - meaningful)) {
                // Prefix '10'
                bw->write(0b10, 2);
                uint64_t middle = (xorBits >> trailing) & ((1ULL << meaningful) - 1);
                bw->write(middle, meaningful);
            } else {
                // Prefix '11'
                bw->write(0b11, 2);
                // Write leading 
                uint64_t leadBits = (leading > 31) ? 31 : leading;
                bw->write(leadBits, 5);
                // Write (meaningful - 1) 
                bw->write(meaningful - 1, 6);
                uint64_t middle = (xorBits >> trailing) & ((1ULL << meaningful) - 1);
                bw->write(middle, meaningful);

                prevLead = leading;
                prevTrail = trailing;
            }
        }
        prevBits = currBits;
    }
};


class XorDecoder {
    bool first;
    uint64_t prevBits;
    int prevLead;
    int prevTrail;

public:
    XorDecoder() : first(false), prevBits(0), prevLead(0), prevTrail(0) {}

    double bitsToDouble(uint64_t bits) {
        double d;
        std::memcpy(&d, &bits, sizeof(double));
        return d;
    }

    double readValue(BitReader* br) {
        if (!first) {
            prevBits = br->read(64);
            first = true;
            return bitsToDouble(prevBits);
        }
        if (br->read(1) == 0) {
            return bitsToDouble(prevBits);
        }
        if (br->read(1) == 0) {
            // Prefix '10'
            int meaningful = 64 - prevLead - prevTrail;
            uint64_t middle = br->read(meaningful);
            uint64_t xorBits = middle << prevTrail;
            uint64_t currBits = prevBits ^ xorBits;
            prevBits = currBits;
            return bitsToDouble(currBits);
        } else {
            // Prefix '11'
            uint64_t leadBits = br->read(5);
            int leading = static_cast<int>(leadBits);
            uint64_t meaningful_minus_1 = br->read(6);
            int meaningful = static_cast<int>(meaningful_minus_1) + 1;
            uint64_t middle = br->read(meaningful);
            int trailing = 64 - leading - meaningful;
            uint64_t xorBits = middle << trailing;
            uint64_t currBits = prevBits ^ xorBits;

            prevLead = leading;
            prevTrail = trailing;
            prevBits = currBits;
            return bitsToDouble(currBits);
        }
    }

};