#include "service/IdempotencyService.h"
#include <chrono>

namespace upimesh {
namespace service {

IdempotencyService::IdempotencyService() {
    evictorThread_ = std::thread([this]() {
        this->evictExpired();
    });
}

IdempotencyService::~IdempotencyService() {
    stopFlag_ = true;
    cv_.notify_all();
    if (evictorThread_.joinable()) {
        evictorThread_.join();
    }
}

bool IdempotencyService::claim(const std::string& packetHash) {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
        
    std::lock_guard<std::mutex> lock(mutex_);
    auto [it, inserted] = seen_.insert({packetHash, now});
    return inserted;
}

int IdempotencyService::size() {
    std::lock_guard<std::mutex> lock(mutex_);
    return seen_.size();
}

void IdempotencyService::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    seen_.clear();
}

void IdempotencyService::evictExpired() {
    while (!stopFlag_.load()) {
        std::unique_lock<std::mutex> lock(mutex_);
        // Sleep for 60 seconds or until stop is requested
        bool stopRequested = cv_.wait_for(lock, std::chrono::seconds(60), [this] {
            return stopFlag_.load();
        });
        
        if (stopRequested) {
            break;
        }

        // Perform eviction
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        int64_t cutoffMillis = now - (ttlSeconds_ * 1000);

        for (auto it = seen_.begin(); it != seen_.end(); ) {
            if (it->second < cutoffMillis) {
                it = seen_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

} // namespace service
} // namespace upimesh
