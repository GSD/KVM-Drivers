// performance_monitor.c - Performance monitoring implementation
#include "performance_monitor.h"

// Static context for driver-wide monitoring
static PERF_MONITOR_CONTEXT g_PerfMonitor = {0};

// Forward declarations
static VOID PerfMonitorLogEvent(PPERF_MONITOR_CONTEXT Context, PPERF_EVENT Event);
static VOID PerfMonitorDpcCallback(
    _In_ PKDPC Dpc,
    _In_opt_ PVOID DeferredContext,
    _In_opt_ PVOID SystemArgument1,
    _In_opt_ PVOID SystemArgument2
);

// Initialize performance monitor
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS PerfMonitorInitialize(
    _Out_ PPERF_MONITOR_CONTEXT Context,
    _In_ ULONG Categories,
    _In_ ULONGLONG WarningThresholdUs,
    _In_ ULONGLONG CriticalThresholdUs
)
{
    if (!Context) {
        return STATUS_INVALID_PARAMETER;
    }
    
    RtlZeroMemory(Context, sizeof(PERF_MONITOR_CONTEXT));
    
    Context->Enabled = TRUE;
    Context->ActiveCategories = Categories;
    Context->WarningThresholdUs = WarningThresholdUs;
    Context->CriticalThresholdUs = CriticalThresholdUs;
    
    KeInitializeSpinLock(&Context->BufferLock);
    
    // Initialize all stats to 0, set min to max
    for (int i = 0; i < 32; i++) {
        Context->Stats[i].MinDurationUs = MAXULONGLONG;
    }
    
    // Set up hitch detection timer
    KeInitializeTimer(&Context->HitchDetectionTimer);
    KeInitializeDpc(&Context->HitchDetectionDpc, PerfMonitorDpcCallback, Context);
    
    KdPrint(("[PerfMonitor] Initialized (warn=%llu us, crit=%llu us)\n",
        WarningThresholdUs, CriticalThresholdUs));
    
    return STATUS_SUCCESS;
}

// Shutdown
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID PerfMonitorShutdown(
    _In_ PPERF_MONITOR_CONTEXT Context
)
{
    if (!Context) return;
    
    PerfMonitorStopHitchDetection(Context);
    Context->Enabled = FALSE;
    
    KdPrint(("[PerfMonitor] Shutdown. Stats:\n"));
    
    // Log final statistics
    for (int i = 0; i < 32; i++) {
        if (Context->Stats[i].TotalCalls > 0) {
            KdPrint(("  Cat[%d]: calls=%llu, avg=%llu us, max=%llu us, warnings=%llu, critical=%llu\n",
                i,
                Context->Stats[i].TotalCalls,
                Context->Stats[i].TotalDurationUs / Context->Stats[i].TotalCalls,
                Context->Stats[i].MaxDurationUs,
                Context->Stats[i].WarningCount,
                Context->Stats[i].CriticalCount));
        }
    }
}

// Start timing
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID PerfMonitorStart(
    _In_ PPERF_MONITOR_CONTEXT Context,
    _In_ ULONG Category,
    _In_z_ const CHAR* Operation,
    _In_z_ const CHAR* Function,
    _In_ ULONG Line,
    _Out_ PULONGLONG StartCycles
)
{
    UNREFERENCED_PARAMETER(Context);
    UNREFERENCED_PARAMETER(Category);
    UNREFERENCED_PARAMETER(Operation);
    UNREFERENCED_PARAMETER(Function);
    UNREFERENCED_PARAMETER(Line);
    
    if (!StartCycles) return;
    
    *StartCycles = ReadTimeStampCounter();
}

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
)
{
    if (!Context || !Context->Enabled) return;
    
    if (!(Category & Context->ActiveCategories)) return;
    
    // Calculate duration
    ULONGLONG endCycles = ReadTimeStampCounter();
    ULONGLONG cycles = endCycles - StartCycles;
    
    // Convert to microseconds (approximate: cycles / (CPU_MHz))
    // For simplicity, using fixed 3GHz approximation
    ULONGLONG durationUs = cycles / 3000;
    
    // Determine level
    UCHAR level = 0;
    if (durationUs >= PERF_THRESHOLD_HANG_US) {
        level = 3;  // Hang
    } else if (durationUs >= Context->CriticalThresholdUs) {
        level = 2;  // Critical
    } else if (durationUs >= Context->WarningThresholdUs) {
        level = 1;  // Warning
    }
    
    // Update statistics
    ULONG statIndex = 0;
    while (!(Category & (1 << statIndex)) && statIndex < 32) {
        statIndex++;
    }
    
    if (statIndex < 32) {
        PPERF_STATS stats = &Context->Stats[statIndex];
        
        InterlockedIncrement64((LONG64*)&stats->TotalCalls);
        InterlockedExchangeAdd64((LONG64*)&stats->TotalDurationUs, durationUs);
        
        // Update min (atomic compare and swap would be better, but this is close enough)
        if (durationUs < stats->MinDurationUs) {
            stats->MinDurationUs = durationUs;
        }
        
        // Update max
        if (durationUs > stats->MaxDurationUs) {
            stats->MaxDurationUs = durationUs;
        }
        
        // Count issues
        if (level == 1) {
            InterlockedIncrement64((LONG64*)&stats->WarningCount);
        } else if (level >= 2) {
            InterlockedIncrement64((LONG64*)&stats->CriticalCount);
            if (level == 3) {
                InterlockedIncrement64((LONG64*)&stats->HangCount);
            }
        }
        
        stats->LastDurationUs = durationUs;
        KeQuerySystemTime(&stats->LastTimestamp);
    }
    
    // Log event if it's notable
    if (level >= 1) {
        PERF_EVENT event = {0};
        KeQuerySystemTime(&event.Timestamp);
        event.Category = Category;
        event.Level = level;
        event.DurationUs = durationUs;
        event.DurationCycles = cycles;
        
        if (Operation) {
            strncpy(event.Operation, Operation, sizeof(event.Operation) - 1);
        }
        if (Function) {
            strncpy(event.Function, Function, sizeof(event.Function) - 1);
        }
        event.Line = Line;
        event.Context = DeviceContext;
        
        PerfMonitorLogEvent(Context, &event);
        
        // Log to debugger immediately for critical issues
        if (level >= 2) {
            KdPrint(("[PERF-%s] %s:%d %s took %llu us (%.2f ms)\n",
                level == 3 ? "HANG" : "CRITICAL",
                Function,
                Line,
                Operation,
                durationUs,
                durationUs / 1000.0));
        }
    }
}

// Log event to ring buffer
static VOID PerfMonitorLogEvent(PPERF_MONITOR_CONTEXT Context, PPERF_EVENT Event)
{
    if (!Context || !Event) return;
    
    KIRQL oldIrql;
    KeAcquireSpinLock(&Context->BufferLock, &oldIrql);
    
    ULONG index = InterlockedIncrement((LONG*)&Context->WriteIndex) % 256;
    RtlCopyMemory(&Context->Events[index], Event, sizeof(PERF_EVENT));
    
    KeReleaseSpinLock(&Context->BufferLock, oldIrql);
    
    // Call external log callback if registered
    if (Context->LogCallback) {
        Context->LogCallback(Event);
    }
}

// Get statistics
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID PerfMonitorGetStats(
    _In_ PPERF_MONITOR_CONTEXT Context,
    _In_ ULONG Category,
    _Out_ PPERF_STATS Stats
)
{
    if (!Context || !Stats) return;
    
    ULONG statIndex = 0;
    while (!(Category & (1 << statIndex)) && statIndex < 32) {
        statIndex++;
    }
    
    if (statIndex < 32) {
        RtlCopyMemory(Stats, &Context->Stats[statIndex], sizeof(PERF_STATS));
    } else {
        RtlZeroMemory(Stats, sizeof(PERF_STATS));
    }
}

// Check for performance issues
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN PerfMonitorHasIssues(
    _In_ PPERF_MONITOR_CONTEXT Context,
    _In_ ULONG Category,
    _In_ ULONGLONG WarningThresholdCount,
    _Out_opt_ PPERF_STATS WorstCategory
)
{
    if (!Context) return FALSE;
    
    BOOLEAN hasIssues = FALSE;
    ULONGLONG worstCount = 0;
    PPERF_STATS worstStats = NULL;
    
    for (int i = 0; i < 32; i++) {
        if (Category & (1 << i)) {
            PPERF_STATS stats = &Context->Stats[i];
            
            ULONGLONG issueCount = stats->WarningCount + stats->CriticalCount + stats->HangCount;
            
            if (issueCount >= WarningThresholdCount) {
                hasIssues = TRUE;
                
                if (issueCount > worstCount) {
                    worstCount = issueCount;
                    worstStats = stats;
                }
            }
        }
    }
    
    if (WorstCategory && worstStats) {
        RtlCopyMemory(WorstCategory, worstStats, sizeof(PERF_STATS));
    }
    
    return hasIssues;
}

// Generate report
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID PerfMonitorGenerateReport(
    _In_ PPERF_MONITOR_CONTEXT Context,
    _Out_writes_(MaxEntries) PPERF_EVENT Events,
    _In_ ULONG MaxEntries,
    _Out_ PULONG EventCount
)
{
    if (!Context || !Events || !EventCount) return;
    
    *EventCount = 0;
    
    KIRQL oldIrql;
    KeAcquireSpinLock(&Context->BufferLock, &oldIrql);
    
    ULONG count = 0;
    ULONG current = Context->WriteIndex;
    
    while (count < MaxEntries && count < 256) {
        ULONG index = (current + 256 - count - 1) % 256;
        
        if (Context->Events[index].Timestamp.QuadPart != 0) {
            RtlCopyMemory(&Events[count], &Context->Events[index], sizeof(PERF_EVENT));
            count++;
        } else {
            break;
        }
    }
    
    KeReleaseSpinLock(&Context->BufferLock, oldIrql);
    
    *EventCount = count;
}

// Hitch detection DPC callback
static VOID PerfMonitorDpcCallback(
    _In_ PKDPC Dpc,
    _In_opt_ PVOID DeferredContext,
    _In_opt_ PVOID SystemArgument1,
    _In_opt_ PVOID SystemArgument2
)
{
    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);
    
    PPERF_MONITOR_CONTEXT context = (PPERF_MONITOR_CONTEXT)DeferredContext;
    if (!context) return;
    
    // Check overall system responsiveness
    LARGE_INTEGER startTime, endTime;
    KeQuerySystemTime(&startTime);
    
    // Try to acquire a spinlock quickly - if this takes too long, system is under load
    KIRQL oldIrql;
    KeAcquireSpinLock(&context->BufferLock, &oldIrql);
    KeReleaseSpinLock(&context->BufferLock, oldIrql);
    
    KeQuerySystemTime(&endTime);
    
    ULONGLONG durationUs = (endTime.QuadPart - startTime.QuadPart) / 10;
    
    if (durationUs > PERF_THRESHOLD_WARNING_US) {
        KdPrint(("[PerfMonitor] System responsiveness check took %llu us - possible load issue\n", durationUs));
    }
    
    // Reschedule timer if still running
    if (context->HitchTimerRunning) {
        LARGE_INTEGER dueTime;
        dueTime.QuadPart = -10000LL * 1000;  // 1 second
        KeSetTimer(&context->HitchDetectionTimer, dueTime, &context->HitchDetectionDpc);
    }
}

// Start hitch detection
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID PerfMonitorStartHitchDetection(
    _In_ PPERF_MONITOR_CONTEXT Context,
    _In_ ULONG IntervalMs
)
{
    if (!Context || Context->HitchTimerRunning) return;
    
    Context->HitchTimerRunning = TRUE;
    
    LARGE_INTEGER dueTime;
    dueTime.QuadPart = -10000LL * (LONGLONG)IntervalMs;
    
    KeSetTimer(&Context->HitchDetectionTimer, dueTime, &Context->HitchDetectionDpc);
    
    KdPrint(("[PerfMonitor] Hitch detection started (%d ms interval)\n", IntervalMs));
}

// Stop hitch detection
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID PerfMonitorStopHitchDetection(
    _In_ PPERF_MONITOR_CONTEXT Context
)
{
    if (!Context) return;
    
    Context->HitchTimerRunning = FALSE;
    KeCancelTimer(&Context->HitchDetectionTimer);
    
    KdPrint(("[PerfMonitor] Hitch detection stopped\n"));
}

// Check responsiveness
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN PerfMonitorIsResponsive(
    _In_ PPERF_MONITOR_CONTEXT Context,
    _In_ ULONGLONG MaxExpectedLatencyUs
)
{
    if (!Context) return TRUE;
    
    LARGE_INTEGER startTime, endTime;
    KIRQL oldIrql;
    
    KeQuerySystemTime(&startTime);
    
    // Quick spinlock test
    KeAcquireSpinLock(&Context->BufferLock, &oldIrql);
    KeReleaseSpinLock(&Context->BufferLock, oldIrql);
    
    KeQuerySystemTime(&endTime);
    
    ULONGLONG durationUs = (endTime.QuadPart - startTime.QuadPart) / 10;
    
    return durationUs <= MaxExpectedLatencyUs;
}
