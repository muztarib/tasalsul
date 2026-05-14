#include <iostream>
#include <vector>
#include <random>
#include <cassert>
#include "bit_man.hpp"
#include "encryption.hpp"

int main() {
    std::vector<double> original;
    // Generate slowly drifting random walk
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist(-0.1, 0.1);
    double val = 50.0;
    for (int i = 0; i < 1000; ++i) {
        val += dist(rng);
        original.push_back(val);
    }

    BitWriter bw;
    XorEncoder enc;
    for (double d : original) {
        enc.writeValue(d, &bw);
    }
    bw.flush();

    auto buf = bw.getBuffer();
    BitReader br(buf.data(), buf.size());
    XorDecoder dec;
    bool chk = true;
    for (double expected : original) {
        double got = dec.readValue(&br);
        if (expected != got){
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