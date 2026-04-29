#pragma once

#include <cstdint>
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>

struct Point {
    int64_t ts = 0;
    double val = 0.0;
};

class HeadBlock {
public:
    HeadBlock(size_t cap) : capacity(cap) {}

    bool append(int64_t ts, double v) {
        // Ensure non-decreasing timestamps
        if (!timestamps.empty()) {
            if ( ts < timestamps.back() ) {
                return false; // reject out-of-order
            }
        }
        timestamps.push_back(ts);
        values.push_back(v);
        return true;
    }

    size_t size() const { return timestamps.size(); }
    int64_t first_ts() const { return timestamps.empty() ? 0 : timestamps.front(); }
    int64_t last_ts() const { return timestamps.empty() ? 0 : timestamps.back(); }

    // Scan range [from, to) and collect points
    std::vector<Point> range(int64_t from, int64_t to) const {
        std::vector<Point> out;
        for (size_t i = 0; i < timestamps.size(); ++i) {
            int64_t t = timestamps[i];
            if (t >= from && t < to) {
                out.push_back(Point{t, values[i]});
            }
        }
        return out;
    }

    const std::vector<int64_t>& ts() const { return timestamps; }
    const std::vector<double>& vs() const { return values; }

private:
    std::vector<int64_t> timestamps;
    std::vector<double> values;
    size_t capacity;
};

class TSDB {
public:
    TSDB(size_t head_cap = 1024, bool strict_ts_ = false) : head_capacity(head_cap), strict_ts(strict_ts_) {}
    bool set_strict_ts(bool s) { strict_ts = s; return strict_ts; }

    // PUT: insert a point for a metric
    // Returns true on success, false if out-of-order
    bool put(const std::string& metric, int64_t ts, double val) {
        std::lock_guard<std::mutex> lg(mtx);
        auto &head = registry[metric];
        // If new metric, create with default capacity
        if (head == nullptr) {
            head = std::make_shared<HeadBlock>(head_capacity);
        }
        // Enforce timestamp monotonicity if strict mode is enabled
        if (head->size() > 0) {
            int64_t last = head->last_ts();
            if (ts <= last) {
                if (strict_ts) return false; // reject duplicates/out-of-order
            }
        }
        return head->append(ts, val);
    }

    // GET: return points in [from, to)
    std::vector<Point> get(const std::string& metric, int64_t from, int64_t to) {
        std::lock_guard<std::mutex> lg(mtx);
        std::vector<Point> res;
        auto it = registry.find(metric);
        if (it == registry.end() || it->second == nullptr) {
            return res;
        }
        return it->second->range(from, to);
    }

    // STATS: simple in-memory stats for a metric
    std::string stats(const std::string& metric) {
        std::lock_guard<std::mutex> lg(mtx);
        auto it = registry.find(metric);
        std::ostringstream oss;
        oss << "metric : " << metric << "\n";
        if (it == registry.end() || it->second == nullptr) {
            oss << "total points : 0\n";
            oss << "in memory : 0\n";
            oss << "on disk : 0\n";
            oss << "disk chunks : 0\n";
            oss << "first timestamp : 0\n";
            oss << "last timestamp : 0\n";
            return oss.str();
        }
        auto head = it->second;
        oss << "total points : " << head->size() << "\n";
        oss << "in memory : " << head->size() << "\n";
        oss << "on disk : 0\n";
        oss << "disk chunks : 0\n";
        if (head->size() > 0) {
            oss << "first timestamp : " << head->first_ts() << "\n";
            oss << "last timestamp : " << head->last_ts() << "\n";
        } else {
            oss << "first timestamp : 0\n";
            oss << "last timestamp : 0\n";
        }
        return oss.str();
    }

private:
    std::unordered_map<std::string, std::shared_ptr<HeadBlock>> registry;
    size_t head_capacity;
    std::mutex mtx;
    bool strict_ts = false;
};
