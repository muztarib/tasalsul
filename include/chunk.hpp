#pragma once

#include "tsdb.hpp"                 
#include "bit_writer.hpp"           
#include "bit_reader.hpp"           
#include "delta_of_delta_encoder.hpp"
#include "delta_of_delta_decoder.hpp"
#include "xor_encoder.hpp"
#include "xor_decoder.hpp"

#include <fstream>
#include <vector>
#include <cstring>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

static inline uint64_t crc64_chunk(const uint8_t* data, size_t len) {
    constexpr uint64_t POLY = 0x42F0E1EBA9EA3693ULL;
    uint64_t crc = 0ULL;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint64_t)data[i] << 56;
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x8000000000000000ULL)
                crc = (crc << 1) ^ POLY;
            else
                crc <<= 1;
        }
    }
    return crc;
}

class ChunkWriter {
public:
    static bool write(const std::string& base_path, const HeadBlock& head) {
        const auto& ts_vec = head.ts();
        const auto& val_vec = head.vs();
        size_t n = ts_vec.size();
        if (n == 0) return true;  

        BitWriter ts_writer;
        DeltaOfDeltaEncoder ts_enc;
        for (uint64_t ts : ts_vec) {
            ts_enc.writeTimestamp(ts, &ts_writer);
        }
        ts_writer.alignToByte();
        ts_writer.flush();
        auto ts_buf = ts_writer.getBuffer();
        uint32_t ts_len = static_cast<uint32_t>(ts_buf.size());

        BitWriter val_writer;
        XOREncoder val_enc;
        for (double v : val_vec) {
            val_enc.writeValue(v, &val_writer);
        }
        val_writer.alignToByte();
        val_writer.flush();
        auto val_buf = val_writer.getBuffer();
        uint32_t val_len = static_cast<uint32_t>(val_buf.size());

        std::vector<uint8_t> file_data;
        auto write_u32 = [&](uint32_t v) {
            for (int i = 0; i < 4; ++i) file_data.push_back(static_cast<uint8_t>(v >> (i*8)));
        };
        auto write_u64 = [&](uint64_t v) {
            for (int i = 0; i < 8; ++i) file_data.push_back(static_cast<uint8_t>(v >> (i*8)));
        };

        file_data.push_back('T'); file_data.push_back('S');
        file_data.push_back('D'); file_data.push_back('B');
        write_u32(2);                      // version
        write_u32(static_cast<uint32_t>(n));
        write_u64(static_cast<uint64_t>(ts_vec.front()));
        write_u64(static_cast<uint64_t>(ts_vec.back()));
        write_u32(ts_len);
        write_u32(val_len);
        file_data.insert(file_data.end(), ts_buf.begin(), ts_buf.end());
        file_data.insert(file_data.end(), val_buf.begin(), val_buf.end());

        uint64_t crc = crc64_chunk(file_data.data(), file_data.size());
        write_u64(crc);                    // append CRC

        // ----- 4. Write to temporary file and rename atomically -----
        std::string filename = std::to_string(ts_vec.front()) + ".chunk";
        fs::path dir_path(base_path);
        if (!fs::exists(dir_path)) {
            fs::create_directories(dir_path);
        }
        fs::path tmp_path = dir_path / (filename + ".tmp");
        fs::path final_path = dir_path / filename;

        std::ofstream ofs(tmp_path, std::ios::binary);
        if (!ofs) return false;
        ofs.write(reinterpret_cast<const char*>(file_data.data()), file_data.size());
        ofs.close();
        if (!ofs) return false;

        fs::rename(tmp_path, final_path);
        return true;
    }
};

class ChunkReader {
public:
    // Read a chunk file and return all points.
    static std::vector<Point> read(const std::string& file_path) {
        std::ifstream ifs(file_path, std::ios::binary);
        if (!ifs) return {};

        // Read entire file
        ifs.seekg(0, std::ios::end);
        size_t file_size = static_cast<size_t>(ifs.tellg());
        ifs.seekg(0, std::ios::beg);
        std::vector<uint8_t> data(file_size);
        ifs.read(reinterpret_cast<char*>(data.data()), file_size);
        if (!ifs) return {};

        // Verify CRC64
        if (file_size < 8) return {};
        uint64_t stored_crc = 0;
        for (int i = 0; i < 8; ++i)
            stored_crc |= static_cast<uint64_t>(data[file_size - 8 + i]) << (i*8);
        uint64_t computed_crc = crc64_chunk(data.data(), file_size - 8);
        if (stored_crc != computed_crc) return {};

        // Parse header
        const uint8_t* ptr = data.data();
        if (std::memcmp(ptr, "TSDB", 4) != 0) return {};
        ptr += 4;
        uint32_t version = 0;
        for (int i = 0; i < 4; ++i) version |= static_cast<uint32_t>(ptr[i]) << (i*8);
        ptr += 4;
        if (version != 2) return {};

        uint32_t point_count = 0;
        for (int i = 0; i < 4; ++i) point_count |= static_cast<uint32_t>(ptr[i]) << (i*8);
        ptr += 4;

        uint64_t first_ts = 0, last_ts = 0;
        for (int i = 0; i < 8; ++i) first_ts |= static_cast<uint64_t>(ptr[i]) << (i*8);
        ptr += 8;
        for (int i = 0; i < 8; ++i) last_ts |= static_cast<uint64_t>(ptr[i]) << (i*8);
        ptr += 8;

        uint32_t ts_len = 0, val_len = 0;
        for (int i = 0; i < 4; ++i) ts_len |= static_cast<uint32_t>(ptr[i]) << (i*8);
        ptr += 4;
        for (int i = 0; i < 4; ++i) val_len |= static_cast<uint32_t>(ptr[i]) << (i*8);
        ptr += 4;

        const uint8_t* ts_start = ptr;
        const uint8_t* val_start = ts_start + ts_len;

        // Decode timestamps
        BitReader ts_reader(ts_start, ts_len);
        DeltaOfDeltaDecoder ts_dec;
        std::vector<int64_t> timestamps;
        timestamps.reserve(point_count);
        for (uint32_t i = 0; i < point_count; ++i) {
            timestamps.push_back(ts_dec.readTimestamp(&ts_reader));
        }

        // Decode values
        BitReader val_reader(val_start, val_len);
        XORDecoder val_dec;
        std::vector<double> values;
        values.reserve(point_count);
        for (uint32_t i = 0; i < point_count; ++i) {
            values.push_back(val_dec.readValue(&val_reader));
        }

        // Merge into points
        std::vector<Point> points;
        points.reserve(point_count);
        for (size_t i = 0; i < point_count; ++i) {
            points.push_back({timestamps[i], values[i]});
        }
        return points;
    }
};