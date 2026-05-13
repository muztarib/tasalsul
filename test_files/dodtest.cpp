#include <iostream>
#include <vector>
#include "bit_man.hpp"
#include "encryption.hpp"

int main() {
    std::vector<uint64_t> original;
    uint64_t base = 1728000000;
    for (int i = 0; i < 1000; ++i) {
        original.push_back(base + i * 10);
    }
    // random deviations in data
    original[200] += 3;
    original[400] += 63;
    original[600] += 255;
    original[800] += 2047;


    BitWriter bw;
    DodEncoder enc;
    for (uint64_t ts : original) {
        enc.writeTimestamp(ts, &bw);
    }
    bw.flush();

    auto buf = bw.getBuffer();
    BitReader br(buf.data(), buf.size());
    DodDecoder dec;
    bool chk = true;
    for (uint64_t expected : original) {
        uint64_t got = dec.readTimestamp(&br);
        if (got != expected) {
            std::cerr << "mismatch: expected = " << expected << ", got = " << got << "./n";
            chk = false;
            break;
        }
    }
    if (chk) {
        std::cout << "Test completed successfully.\n";
    }
    else {
        std::cout << "Test failed.\n";
    }
    return 0;
}