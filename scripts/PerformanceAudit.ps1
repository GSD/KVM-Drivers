# PerformanceAudit.ps1 - Detects blocking operations and performance issues
param(
    [string]$SourcePath = "..\src",
    [string]$OutputFile = "performance_audit_report.txt"
)

$ErrorActionPreference = "Continue"

# Critical patterns that cause hangs/lag
$BlockingPatterns = @{
    # Kernel blocking operations
    KeWaitForSingleObject_Infinite = @{
        Pattern = 'KeWaitForSingleObject\s*\([^,]+,\s*Executive,\s*(?:KernelMode|UserMode),\s*(?:FALSE|0),\s*(?:NULL|nullptr)\s*\)'
        Description = "INFINITE wait in KeWaitForSingleObject - can hang forever"
        Severity = "CRITICAL"
        Fix = "Add timeout or use non-blocking variant"
    }
    
    KeDelayExecutionThread_Long = @{
        Pattern = 'KeDelayExecutionThread\s*\([^,]+,\s*(?:FALSE|0),\s*\(?\s*-\s*\d{7,}L?L?\s*\)?'
        Description = "Long delay (>= 1 second) in kernel thread"
        Severity = "HIGH"
        Fix = "Use shorter delays or work items"
    }
    
    ZwWaitForSingleObject_NoTimeout = @{
        Pattern = 'ZwWaitForSingleObject\s*\([^,]+,\s*(?:FALSE|0|NULL)\s*\)'
        Description = "ZwWaitForSingleObject without timeout"
        Severity = "HIGH"
        Fix = "Add timeout parameter"
    }
    
    # IRQL violations (can cause lag)
    HighIrql_LongOperation = @{
        Pattern = 'IRQL_(?:DISPATCH|DIRQL)_LEVEL[\s\S]{0,500}?(?:for|while|do)\s*\([\s\S]{0,200}?\{[\s\S]{0,1000}?\}'
        Description = "Potential long-running operation at high IRQL"
        Severity = "CRITICAL"
        Fix = "Defer to lower IRQL or reduce work"
    }
    
    # DPC/ISR issues
    DPC_Recursive = @{
        Pattern = 'KeInsertQueueDpc[\s\S]{0,100}?KeInsertQueueDpc'
        Description = "Potential recursive DPC insertion"
        Severity = "HIGH"
        Fix = "Add reentrancy check"
    }
    
    # User-mode blocking
    WaitForSingleObject_Infinite = @{
        Pattern = 'WaitForSingleObject\s*\([^,]+,\s*(?:INFINITE|-1)\s*\)'
        Description = "INFINITE wait in user-mode code"
        Severity = "HIGH"
        Fix = "Add reasonable timeout (e.g., 5000ms)"
    }
    
    Sleep_Long = @{
        Pattern = 'Sleep\s*\(\s*\d{4,}\s*\)'
        Description = "Long Sleep() in UI thread (>= 4 seconds)"
        Severity = "MEDIUM"
        Fix = "Use async operations or shorter sleeps"
    }
    
    SendMessage_Direct = @{
        Pattern = 'SendMessage[AW]?\s*\([^)]+\)'
        Description = "SendMessage is synchronous - can hang if target is unresponsive"
        Severity = "MEDIUM"
        Fix = "Use PostMessage for async, or SendMessageTimeout with timeout"
    }
    
    # Networking issues
    Recv_NoTimeout = @{
        Pattern = 'recv(?:from)?\s*\([^,]+,\s*[^,]+,\s*[^,]+,\s*[^,]*,\s*(?:NULL|0|nullptr)\s*\)'
        Description = "Blocking recv with no timeout"
        Severity = "HIGH"
        Fix = "Set SO_RCVTIMEO or use non-blocking socket"
    }
    
    Connect_Blocking = @{
        Pattern = 'connect\s*\([^,]+,\s*[^,]+,\s*[^)]+\)(?!.*fcntl.*O_NONBLOCK)'
        Description = "Blocking connect() without non-blocking setup"
        Severity = "HIGH"
        Fix = "Use non-blocking sockets or select/poll with timeout"
    }
    
    Select_LongTimeout = @{
        Pattern = 'select\s*\([^,]+,\s*[^,]+,\s*[^,]+,\s*[^,]+,\s*(?:NULL|nullptr|0)\s*\)'
        Description = "select() with no timeout - blocks indefinitely"
        Severity = "MEDIUM"
        Fix = "Add timeout structure"
    }
    
    # Synchronization issues
    SpinLock_LongHold = @{
        Pattern = 'KeAcquireSpinLock[^;]*;[\s\S]{0,500}?KeReleaseSpinLock'
        Description = "Potentially long critical section with spinlock"
        Severity = "HIGH"
        Fix = "Minimize work under spinlock, use mutex for long ops"
    }
    
    Mutex_NestedDeep = @{
        Pattern = 'WaitForSingleObject[\s\S]{0,200}?WaitForSingleObject[\s\S]{0,200}?WaitForSingleObject'
        Description = "Deep mutex nesting - risk of deadlock"
        Severity = "MEDIUM"
        Fix = "Reduce nesting, use consistent lock ordering"
    }
    
    # I/O issues
    ReadFile_Synchronous = @{
        Pattern = 'ReadFile\s*\([^,]+,\s*[^,]+,\s*[^,]+,\s*(?:NULL|0|nullptr),\s*(?:NULL|0|nullptr)\s*\)'
        Description = "Synchronous ReadFile without OVERLAPPED"
        Severity = "MEDIUM"
        Fix = "Use OVERLAPPED structure for async I/O"
    }
    
    WriteFile_Synchronous = @{
        Pattern = 'WriteFile\s*\([^,]+,\s*[^,]+,\s*[^,]+,\s*(?:NULL|0|nullptr),\s*(?:NULL|0|nullptr)\s*\)'
        Description = "Synchronous WriteFile without OVERLAPPED"
        Severity = "MEDIUM"
        Fix = "Use OVERLAPPED structure for async I/O"
    }
}

# Async/Threading issues
$AsyncIssues = @{
    NoErrorHandling_AsyncOp = @{
        Pattern = 'CreateThread\s*\([^)]+\)(?!.*if\s*\()'
        Description = "Thread creation without immediate error check"
        Severity = "MEDIUM"
    }
    
    ThreadNoJoin = @{
        Pattern = 'CreateThread[\s\S]{0,1000}?ExitProcess|TerminateProcess'
        Description = "Process exit without waiting for threads"
        Severity = "MEDIUM"
    }
    
    DetachedThread_NoCleanup = @{
        Pattern = 'CloseHandle\s*\([^)]+\)[\s\S]{0,500}?CreateThread'
        Description = "Thread handle closed but thread may still be running"
        Severity = "LOW"
    }
}

# Memory/performance patterns
$PerformancePatterns = @{
    ExAllocatePool_Frequent = @{
        Pattern = '(ExAllocatePool(?:WithTag)?\s*\([^)]+\)[^;]*;){3,}'
        Description = "Multiple pool allocations in sequence - consider batching"
        Severity = "LOW"
    }
    
    memset_LargeBuffer = @{
        Pattern = 'memset\s*\([^,]+,\s*[^,]+,\s*(?:\d{5,}|0x[\da-fA-F]{4,})\s*\)'
        Description = "Large memset operation - could stall"
        Severity = "LOW"
    }
    
    memcpy_LargeBuffer = @{
        Pattern = 'memcpy\s*\([^,]+,\s*[^,]+,\s*(?:\d{5,}|0x[\da-fA-F]{4,})\s*\)'
        Description = "Large memcpy operation - consider async or chunked"
        Severity = "LOW"
    }
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "PERFORMANCE AUDIT - KVM-Drivers" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$Results = @()
$Stats = @{ Critical = 0; High = 0; Medium = 0; Low = 0 }

# Find all source files
$files = Get-ChildItem -Path $SourcePath -Recurse -Include @("*.c","*.cpp","*.h","*.hpp","*.cs")

Write-Host "Scanning $($files.Count) files..." -ForegroundColor Yellow

foreach ($file in $files) {
    $content = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
    if (-not $content) { continue }
    
    $isKernel = $file.FullName -match "drivers[\\/]"
    
    # Check all patterns
    $allPatterns = $BlockingPatterns + $AsyncIssues + $PerformancePatterns
    
    foreach ($name in $allPatterns.Keys) {
        $pattern = $allPatterns[$name]
        $matches = [regex]::Matches($content, $pattern.Pattern, [System.Text.RegularExpressions.RegexOptions]::Singleline)
        
        foreach ($match in $matches) {
            $lineNum = ($content.Substring(0, $match.Index) -split "`n").Count
            
            $issue = [PSCustomObject]@{
                File = $file.FullName
                Line = $lineNum
                Pattern = $name
                Description = $pattern.Description
                Severity = $pattern.Severity
                Fix = $pattern.Fix
                Code = $match.Value.Substring(0, [Math]::Min($match.Value.Length, 100))
                IsKernel = $isKernel
            }
            
            $Results += $issue
            $Stats[$pattern.Severity]++
        }
    }
}

# Generate report
$report = @"
PERFORMANCE AUDIT REPORT
Generated: $(Get-Date)
Files Scanned: $($files.Count)

SUMMARY
-------
Critical Issues (Will cause hangs): $($Stats.Critical)
High Issues (High risk): $($Stats.High)
Medium Issues (Moderate risk): $($Stats.Medium)
Low Issues (Minor optimization): $($Stats.Low)

CRITICAL ISSUES (Fix Immediately)
---------------------------------
"@

$critical = $Results | Where-Object { $_.Severity -eq "CRITICAL" } | Sort-Object File, Line
foreach ($issue in $critical) {
    $report += "[$($issue.File):$($issue.Line)] $($issue.Description)`n"
    $report += "  Code: $($issue.Code)`n"
    $report += "  Fix: $($issue.Fix)`n`n"
}

$report += @"

HIGH SEVERITY ISSUES
--------------------
"@

$high = $Results | Where-Object { $_.Severity -eq "HIGH" } | Sort-Object File, Line
foreach ($issue in $high) {
    $report += "[$($issue.File):$($issue.Line)] $($issue.Description)`n"
    $report += "  Fix: $($issue.Fix)`n`n"
}

# Check specific files for networking
$networkFiles = $files | Where-Object { $_.Name -match "websocket|socket|network|remote" }

$report += @"

NETWORKING FILES ANALYSIS
-------------------------
Found $($networkFiles.Count) networking-related files.

"@

foreach ($netFile in $networkFiles) {
    $content = Get-Content $netFile.FullName -Raw
    
    # Check for async patterns
    $hasAsync = $content -match "async|thread|queue|OVERLAPPED|WSAAsync"
    $hasBlocking = $content -match "recv\s*\([^)]*NULL|select\s*\([^)]*NULL"
    
    $status = if ($hasAsync -and -not $hasBlocking) { "✓ PROPERLY ASYNC" } 
              elseif ($hasBlocking) { "✗ BLOCKING CALLS DETECTED" }
              else { "? UNCLEAR - REVIEW NEEDED" }
    
    $report += "$($netFile.Name): $status`n"
}

$report += @"

RECOMMENDATIONS
---------------
1. Replace all INFINITE waits with reasonable timeouts (5-30 seconds max)
2. Use non-blocking sockets with select/poll/IOCP for networking
3. Minimize work under spinlocks (keep < 100 microseconds)
4. Use OVERLAPPED I/O for all file operations
5. Add performance monitoring hooks to measure actual latencies
6. Test with Driver Verifier enabled

FILES REQUIRING IMMEDIATE ATTENTION
-----------------------------------
"@

$problemFiles = $Results | Group-Object File | Sort-Object Count -Descending | Select-Object -First 10
foreach ($file in $problemFiles) {
    $report += "  $($file.Name): $($file.Count) issues`n"
}

$report | Out-File $OutputFile
Write-Host ""
Write-Host "Audit complete!" -ForegroundColor Green
Write-Host "Critical: $($Stats.Critical), High: $($Stats.High), Medium: $($Stats.Medium), Low: $($Stats.Low)" -ForegroundColor $(if ($Stats.Critical -gt 0) { "Red" } elseif ($Stats.High -gt 0) { "Yellow" } else { "Green" })
Write-Host "Report saved to: $OutputFile" -ForegroundColor Cyan

exit $(if ($Stats.Critical -gt 0) { 1 } else { 0 })
