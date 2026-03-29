// performance_monitor.h - Performance monitoring and hitch detection for drivers
#pragma once

#include <ntddk.h>
#include <wdf.h>

// Performance thresholds (in microseconds)
#define PERF_THRESHOLD_WARNING_US   1000    // 1ms - warning
#define PERF_THRESHOLD_CRITICAL_US  5000    // 5ms - critical hitch
#define PERF_THRESHOLD_HANG_US      50000   // 50ms - potential hang

// Categories to monitor
#define PERF_CATEGORY_IOCTL         0x00000001
#define PERF_CATEGORY_INPUT_INJECT 0x00000002
#define PERF_CATEGORY_HID_REPORT    0x00000004
#define PERF_CATEGORY_DPC           0x00000008
#define PERF_CATEGORY_ISR           0x00000010
#define PERF_CATEGORY_PNP           0x00000020
#define PERF_CATEGORY_POWER         0x00000040
#define PERF_CATEGORY_ALL           0xFFFFFFFF

// Performance event entry
typedef struct _PERF_EVENT {
    LARGE_INTEGER Timestamp;
    ULONG Category;
    UCHAR Level;  // 0=OK, 1=Warning, 2=Critical, 3=Hang
    ULONGLONG DurationUs;
    ULONGLONG DurationCycles;  // For precise measurement
    CHAR Operation[64];
    CHAR Function[64];
    ULONG Line;
    PVOID Context;  // Device object or context
} PERF_EVENT, *PPERF_EVENT;

// Performance statistics per category
typedef struct _PERF_STATS {
    ULONGLONG TotalCalls;
    ULONGLONG TotalDurationUs;
    ULONGLONG MinDurationUs;
    ULONGLONG MaxDurationUs;
    ULONGLONG WarningCount;
    ULONGLONG CriticalCount;
    ULONGLONG HangCount;
    ULONGLONG LastDurationUs;
    LARGE_INTEGER LastTimestamp;
} PERF_STATS, *PPERF_STATS;

// Monitor context
typedef struct _PERF_MONITOR_CONTEXT {
    // Ring buffer for events
    PERF_EVENT Events[256];
    ULONG WriteIndex;
    KSPIN_LOCK BufferLock;
    
    // Statistics per category
    PERF_STATS Stats[32];  // Indexed by bit position
    
    // Active monitoring
    BOOLEAN Enabled;
    ULONG ActiveCategories;
    ULONGLong WarningThresholdUs;
    ULONGLong CriticalThresholdUs;
    
    // Hitch detection
    KTIMER HitchDetectionTimer;
    KDPC HitchDetectionDpc;
    BOOLEAN HitchTimerRunning;
    
    // Logging callback
    VOID (*LogCallback)(PPERF_EVENT Event);
} PERF_MONITOR_CONTEXT, *PPERF_MONITOR_CONTEXT;

// Function prototypes
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS PerfMonitorInitialize(
    _Out_ PPERF_MONITOR_CONTEXT Context,
    _In_ ULONG Categories,
    _In_ ULONGLONG WarningThresholdUs,
    _In_ ULONGLONG CriticalThresholdUs
);

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID PerfMonitorShutdown(
    _In_ PPERF_MONITOR_CONTEXT Context
);

// Start timing an operation
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID PerfMonitorStart(
    _In_ PPERF_MONITOR_CONTEXT Context,
    _In_ ULONG Category,
    _In_z_ const CHAR* Operation,
    _In_z_ const CHAR* Function,
    _In_ ULONG Line,
    _Out_ PULONGLONG StartCycles
);

// End timing and record
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID PerfMonitorEnd(
    _In_ PPERF_MONITOR_CONTEXT Context,
    _In_ ULONG Category,
    _In_z_ const CHAR* Operation,
    _In_z_ const CHAR* Function,
    _In_ ULONG Line,
    _In_ ULONGLONG StartCycles,
    _In_opt_ PVOID DeviceContext
);

// Quick macro for scoped timing
#define PERF_MONITOR_SCOPE(ctx, cat, op) \
    ULONGLONG _perfStart; \
    PerfMonitorStart(ctx, cat, op, __FUNCTION__, __LINE__, &_perfStart); \
    defer { PerfMonitorEnd(ctx, cat, op, __FUNCTION__, __LINE__, _perfStart, NULL); }

// Get statistics
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID PerfMonitorGetStats(
    _In_ PPERF_MONITOR_CONTEXT Context,
    _In_ ULONG Category,
    _Out_ PPERF_STATS Stats
);

// Check for performance issues
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN PerfMonitorHasIssues(
    _In_ PPERF_MONITOR_CONTEXT Context,
    _In_ ULONG Category,
    _In_ ULONGLONG WarningThresholdCount,
    _Out_opt_ PPERF_STATS WorstCategory
);

// Telemetry and reporting
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID PerfMonitorGenerateReport(
    _In_ PPERF_MONITOR_CONTEXT Context,
    _Out_writes_(MaxEntries) PPERF_EVENT Events,
    _In_ ULONG MaxEntries,
    _Out_ PULONG EventCount
);

// Real-time hitch detection
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID PerfMonitorStartHitchDetection(
    _In_ PPERF_MONITOR_CONTEXT Context,
    _In_ ULONG IntervalMs
);

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID PerfMonitorStopHitchDetection(
    _In_ PPERF_MONITOR_CONTEXT Context
);

// Check if system is responsive
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN PerfMonitorIsResponsive(
    _In_ PPERF_MONITOR_CONTEXT Context,
    _In_ ULONGLONG MaxExpectedLatencyUs
);
