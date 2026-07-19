#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <condition_variable>

namespace upimesh {
namespace service {

class IdempotencyService {
public:
    IdempotencyService();
    ~IdempotencyService();

    // Try to claim a hash. Returns true if this caller is the first; false if already claimed.
    bool claim(const std::string& packetHash);

    int size();
    void clear();

private:
    std::unordered_map<std::string, int64_t> seen_;
    mutable std::mutex mutex_;

    int64_t ttlSeconds_ = 86400; // 24 hours

    std::thread evictorThread_;
    std::condition_variable cv_;
    std::atomic<bool> stopFlag_{false};

    void evictExpired();
};

} // namespace service
} // namespace upimesh
