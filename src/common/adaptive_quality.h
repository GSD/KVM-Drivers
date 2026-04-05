// adaptive_quality.h - Adaptive quality and graceful degradation under load
// Monitors system load and automatically adjusts framerate, encoding quality,
// and connection limits to maintain responsiveness under pressure.

#pragma once
#include <windows.h>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <mutex>

// Quality tiers - system degrades/recovers through these levels
enum class QualityTier {
    ULTRA  = 0,  // 60 fps, full quality
    HIGH   = 1,  // 30 fps, high quality
    MEDIUM = 2,  // 20 fps, medium quality
    LOW    = 3,  // 10 fps, reduced quality
    MINIMAL = 4, // 5 fps, minimal - avoid crash
};

struct QualitySettings {
    int targetFps;
    int maxConnectionsVnc;
    int maxConnectionsWs;
    int jpegQuality;        // 0-100, for future JPEG encoding
    bool sendFullFrames;    // false = send dirty rects only
    int maxFrameSizeBytes;  // cap to prevent network saturation
};

static constexpr QualitySettings QUALITY_PRESETS[] = {
    // ULTRA
    { 60, 10, 20, 95, false, 1920 * 1080 * 4 },
    // HIGH
    { 30, 8,  15, 80, false, 1920 * 1080 * 4 },
    // MEDIUM
    { 20, 6,  10, 65, false, 1280 * 720  * 4 },
    // LOW
    { 10, 4,  6,  50, true,  1280 * 720  * 4 },
    // MINIMAL
    {  5, 2,  3,  30, true,  640  * 480  * 4 },
};

// AdaptiveQuality: monitors CPU, latency, and queue depth to
// automatically degrade or recover quality without manual intervention.
class AdaptiveQuality {
public:
    explicit AdaptiveQuality(int degradeThresholdMs = 150, int recoverThresholdMs = 50)
        : currentTier_(QualityTier::ULTRA)
        , degradeThresholdMs_(degradeThresholdMs)
        , recoverThresholdMs_(recoverThresholdMs)
        , consecutiveSlow_(0)
        , consecutiveFast_(0)
        , droppedFrames_(0)
        , totalFrames_(0) {
        lastCheck_ = std::chrono::steady_clock::now();
    }

    // Call at the start of each frame send. Returns the interval to wait
    // before the next frame (based on current FPS target).
    int GetFrameIntervalMs() const {
        return 1000 / GetSettings().targetFps;
    }

    // Report the time taken to send a frame (ms). Adjusts quality tier.
    void ReportFrameLatency(int latencyMs) {
        totalFrames_.fetch_add(1, std::memory_order_relaxed);

        std::lock_guard<std::mutex> lk(mutex_);
        if (latencyMs > degradeThresholdMs_) {
            consecutiveSlow_++;
            consecutiveFast_ = 0;

            // Degrade after 3 consecutive slow frames
            if (consecutiveSlow_ >= 3) {
                Degrade("frame latency " + std::to_string(latencyMs) + "ms > " +
                        std::to_string(degradeThresholdMs_) + "ms threshold");
                consecutiveSlow_ = 0;
            }
        } else if (latencyMs < recoverThresholdMs_) {
            consecutiveFast_++;
            consecutiveSlow_ = 0;

            // Recover after 10 consecutive fast frames (conservative)
            if (consecutiveFast_ >= 10) {
                Recover();
                consecutiveFast_ = 0;
            }
        } else {
            // In the middle zone - reset both counters
            consecutiveSlow_ = 0;
            consecutiveFast_ = 0;
        }
    }

    // Report a dropped frame (queue full, client too slow)
    void ReportDroppedFrame() {
        droppedFrames_.fetch_add(1, std::memory_order_relaxed);
        std::lock_guard<std::mutex> lk(mutex_);
        consecutiveSlow_ += 2;  // Weigh drops more heavily
        if (consecutiveSlow_ >= 3) {
            Degrade("dropped frame");
            consecutiveSlow_ = 0;
        }
    }

    // Poll CPU usage and degrade if system is overloaded
    void CheckSystemLoad() {
        std::lock_guard<std::mutex> lk(mutex_);
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastCheck_).count();
        if (elapsed < 5) return;  // Only check every 5s
        lastCheck_ = now;

        FILETIME idleTime, kernelTime, userTime;
        if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) return;

        ULARGE_INTEGER idle, kernel, user;
        idle.LowPart   = idleTime.dwLowDateTime;
        idle.HighPart  = idleTime.dwHighDateTime;
        kernel.LowPart = kernelTime.dwLowDateTime;
        kernel.HighPart = kernelTime.dwHighDateTime;
        user.LowPart   = userTime.dwLowDateTime;
        user.HighPart  = userTime.dwHighDateTime;

        ULONGLONG total = (kernel.QuadPart - prevKernel_) + (user.QuadPart - prevUser_);
        ULONGLONG idleDelta = idle.QuadPart - prevIdle_;

        if (total > 0) {
            int cpuPct = (int)(100 - (idleDelta * 100 / total));

            if (cpuPct > 90 && currentTier_ < QualityTier::MINIMAL) {
                Degrade("CPU " + std::to_string(cpuPct) + "% > 90% threshold");
            } else if (cpuPct < 50 && currentTier_ > QualityTier::ULTRA) {
                Recover();
            }
        }

        prevKernel_ = kernel.QuadPart;
        prevUser_   = user.QuadPart;
        prevIdle_   = idle.QuadPart;
    }

    QualityTier GetTier() const { return currentTier_.load(); }

    const QualitySettings& GetSettings() const {
        return QUALITY_PRESETS[(int)currentTier_.load()];
    }

    void GetStats(uint64_t* total, uint64_t* dropped, int* tier) const {
        if (total)   *total   = totalFrames_.load(std::memory_order_relaxed);
        if (dropped) *dropped = droppedFrames_.load(std::memory_order_relaxed);
        if (tier)    *tier    = (int)currentTier_.load();
    }

    static const char* TierName(QualityTier t) {
        switch (t) {
        case QualityTier::ULTRA:   return "ULTRA";
        case QualityTier::HIGH:    return "HIGH";
        case QualityTier::MEDIUM:  return "MEDIUM";
        case QualityTier::LOW:     return "LOW";
        case QualityTier::MINIMAL: return "MINIMAL";
        default:                   return "UNKNOWN";
        }
    }

private:
    mutable std::mutex mutex_;          // guards all non-atomic members below
    std::atomic<QualityTier> currentTier_;
    int degradeThresholdMs_;
    int recoverThresholdMs_;
    int consecutiveSlow_;
    int consecutiveFast_;
    std::atomic<uint64_t> droppedFrames_;
    std::atomic<uint64_t> totalFrames_;

    std::chrono::steady_clock::time_point lastCheck_;
    ULONGLONG prevKernel_ = 0;
    ULONGLONG prevUser_   = 0;
    ULONGLONG prevIdle_   = 0;

    // NOTE: Degrade/Recover must only be called with mutex_ already held.
    void Degrade(const std::string& reason) {
        QualityTier current = currentTier_.load();
        if (current < QualityTier::MINIMAL) {
            QualityTier next = (QualityTier)((int)current + 1);
            currentTier_.store(next);
            std::cerr << "[AdaptiveQuality] Degraded to " << TierName(next)
                      << " (" << reason << ")"
                      << " fps=" << GetSettings().targetFps << std::endl;
        }
    }

    void Recover() {
        QualityTier current = currentTier_.load();
        if (current > QualityTier::ULTRA) {
            QualityTier next = (QualityTier)((int)current - 1);
            currentTier_.store(next);
            std::cout << "[AdaptiveQuality] Recovered to " << TierName(next)
                      << " fps=" << QUALITY_PRESETS[(int)next].targetFps << std::endl;
        }
    }
};
