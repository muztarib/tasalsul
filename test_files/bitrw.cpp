#include <iostream>
#include <random>
#include <cassert>
#include "../src/bitman.cpp"

int main() {
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<int> bits_dist(1, 64);
    std::uniform_int_distribution<uint64_t> value_dist;

    std::vector<std::pair<uint64_t, int>> test_data;
    for (int i = 0; i < 100; ++i) {
        int nbits = bits_dist(rng);
        uint64_t val = value_dist(rng) & ((1ULL << nbits) - 1); // keep only low nbits
        test_data.emplace_back(val, nbits);
    }

    // Write 
    BitWriter writer;
    for (auto& p : test_data) {
        writer.write(p.first, p.second);
    }
    writer.flush();

    // Read and verify
    auto buffer = writer.getBuffer();
    BitReader reader(buffer.data(), buffer.size());
    bool chk = true;
    for (auto& p : test_data) {
        uint64_t got = reader.read(p.second);
        if (got != p.first) {
            std::cerr << "Mismatch: expected " << p.first << " (" << p.second << " bits), got " << got << "\n";
            chk = false;
            break;
        }
    }
    if (chk) {
        std::cout << "Round-trip test passed. Total bytes written: " << buffer.size() << "\n";
    }
    else {
        std::cout << "Test failed.\n";
    }
    return 0;
}