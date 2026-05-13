#pragma once

#include <cstdint>
#include <vector>
#include <stdexcept>
#include <cstring>

class BitWriter {
    std::vector<uint8_t> buffer;
    uint8_t curbyte;
    int curbits;

public:
    BitWriter() : buffer(), curbyte(0), curbits(0) {}

    // Write the lower n_bits of 'value' to the stream, MSB first.
    void write(uint64_t value, int n_bits) {
         if (n_bits < 0 || n_bits > 64) {
            throw std::invalid_argument("n_bits must be 0..64");
        }
        while (n_bits > 0) {
            int free = 8 - curbits;
            int take = std::min(free, n_bits);
            int shift = n_bits - take;
            uint64_t chunk = (value >> shift) & ((1ULL << take) - 1);
            curbyte |= (chunk << (free - take));
            curbits += take;
            n_bits -= take;
            
            if (curbits == 8){
                buffer.push_back(curbyte);
                curbyte = 0;
                curbits = 0;
            }
        }
    }
    // Align the stream to the next byte boundary by writing zero bits.
    void alignToByte() {
        if (curbits > 0) {
            write(0, 8 - curbits);
        }
    }
    // Flush any partially filled byte to the buffer (pads with zero bits).
    void flush() {
        if (curbits > 0) {
            buffer.push_back(curbyte);
            curbyte = 0;
            curbits = 0;
        }
    }
    // Return the final byte buffer (call flush() first if needed).
    std::vector<uint8_t> getBuffer() const {
        return buffer;
    }
    // Clear the writer for reuse.
    void clear() {
        buffer.clear();
        curbyte = 0;
        curbits = 0;
    }
};

class BitReader {
    const uint8_t* data;
    size_t len;
    size_t pos;
    uint8_t curbyte;
    int curbits;

public:
    BitReader(const uint8_t* _data, size_t _len) : data(_data), len(_len), pos(0), curbyte(_len > 0 ? _data[0] : 0), curbits(0) {}
    // Read exactly 'n_bits' from the stream and return them as a uint64_t.
    uint64_t read(int n_bits) {
        if (n_bits < 0 || n_bits > 64) {
            throw std::invalid_argument("n_bits must be 0..64");
        }
        uint64_t result = 0;

        while (n_bits > 0) {
            if (curbits == 8) {
                pos++;
                if (pos > len) {
                    throw std::out_of_range("No more bytes available");
                }
                curbyte = data[pos];
                curbits = 0;
            }
            int available = 8 - curbits;
            int take = std::min(available, n_bits);
            int shift = available - take;
            uint64_t chunk = (curbyte >> shift) & ((1ULL << take) - 1);
            result = result << take;
            result |= chunk;
            curbits += take;
            n_bits -= take;
        }
        return result;
    }
    // Return the number of bytes remaining in the buffer (for debugging).
    size_t bytesLeft() const {
        if (pos >= len) return 0;
        return (len - pos - 1) + (curbits < 8 ? 1 : 0);
    }
};