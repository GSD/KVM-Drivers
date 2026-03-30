// rate_limiter.h - Input rate limiting to prevent DoS attacks
#pragma once

#include <chrono>
#include <atomic>
#include <mutex>

namespace KVMDrivers {

// Rate limiter for input injection
class RateLimiter {
public:
    RateLimiter(int maxPerSecond = 120) 
        : maxPerSecond_(maxPerSecond)
        , inputCount_(0)
        , lastReset_(std::chrono::steady_clock::now()) {
    }
    
    // Returns true if input is allowed, false if rate exceeded
    bool AllowInput() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastReset_);
        
        if (elapsed.count() >= 1) {
            // Reset counter every second
            inputCount_ = 0;
            lastReset_ = now;
        }
        
        if (++inputCount_ > maxPerSecond_) {
            return false;
        }
        return true;
    }
    
    // Get current rate
    int GetCurrentRate() const {
        return inputCount_.load();
    }
    
    // Set max rate
    void SetMaxRate(int maxPerSecond) {
        maxPerSecond_ = maxPerSecond;
    }
    
    // Reset counter
    void Reset() {
        inputCount_ = 0;
        lastReset_ = std::chrono::steady_clock::now();
    }

private:
    int maxPerSecond_;
    std::atomic<int> inputCount_;
    std::chrono::steady_clock::time_point lastReset_;
};

// Thread-safe connection tracker
class ConnectionTracker {
public:
    ConnectionTracker(int maxConnections = 20)
        : maxConnections_(maxConnections)
        , currentConnections_(0) {
    }
    
    // Try to add a connection, returns false if limit reached
    bool TryAddConnection() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (currentConnections_ >= maxConnections_) {
            return false;
        }
        currentConnections_++;
        return true;
    }
    
    // Remove a connection
    void RemoveConnection() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (currentConnections_ > 0) {
            currentConnections_--;
        }
    }
    
    // Get current count
    int GetConnectionCount() const {
        return currentConnections_.load();
    }
    
    // Set max connections
    void SetMaxConnections(int max) {
        maxConnections_ = max;
    }

private:
    int maxConnections_;
    std::atomic<int> currentConnections_;
    std::mutex mutex_;
};

// Latency tracker for performance monitoring
class LatencyTracker {
public:
    LatencyTracker(int warningThresholdMs = 16, int criticalThresholdMs = 100)
        : warningThresholdMs_(warningThresholdMs)
        , criticalThresholdMs_(criticalThresholdMs)
        , totalLatencyUs_(0)
        , sampleCount_(0)
        , maxLatencyUs_(0)
        , hitchCount_(0) {
    }
    
    // Record a latency sample
    void RecordLatency(int64_t microseconds) {
        totalLatencyUs_ += microseconds;
        sampleCount_++;
        
        if (microseconds > maxLatencyUs_) {
            maxLatencyUs_ = microseconds;
        }
        
        // Check for hitch
        if (microseconds > criticalThresholdMs_ * 1000) {
            hitchCount_++;
        }
    }
    
    // Start timing
    std::chrono::steady_clock::time_point StartTiming() {
        return std::chrono::steady_clock::now();
    }
    
    // End timing and record
    int64_t EndTiming(std::chrono::steady_clock::time_point start) {
        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        RecordLatency(elapsed);
        return elapsed;
    }
    
    // Get average latency in microseconds
    double GetAverageLatencyUs() const {
        if (sampleCount_ == 0) return 0;
        return (double)totalLatencyUs_ / sampleCount_;
    }
    
    // Get max latency
    int64_t GetMaxLatencyUs() const {
        return maxLatencyUs_;
    }
    
    // Get hitch count
    int GetHitchCount() const {
        return hitchCount_;
    }
    
    // Reset stats
    void Reset() {
        totalLatencyUs_ = 0;
        sampleCount_ = 0;
        maxLatencyUs_ = 0;
        hitchCount_ = 0;
    }

private:
    int warningThresholdMs_;
    int criticalThresholdMs_;
    std::atomic<int64_t> totalLatencyUs_;
    std::atomic<int> sampleCount_;
    std::atomic<int64_t> maxLatencyUs_;
    std::atomic<int> hitchCount_;
};

} // namespace KVMDrivers
