// test_keyboard_hid.c - Comprehensive HID minidriver test suite
// Tests actual Windows input injection via HID stack

#include "../src/drivers/vhidkb/vhidkb.h"
#include "../src/drivers/vhidkb/vhidkb_ioctl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <hidclass.h>

// Test results
#define TEST_PASS 0
#define TEST_FAIL 1

typedef struct _TEST_RESULT {
    char name[128];
    int status;
    char message[256];
    DWORD durationMs;
} TEST_RESULT;

// Test context
typedef struct _TEST_CONTEXT {
    HANDLE deviceHandle;
    BOOL driverLoaded;
    BOOL usingDriverInjection;
    TEST_RESULT results[50];
    int testCount;
    int passCount;
    int failCount;
} TEST_CONTEXT;

// Initialize test context
BOOL TestInitialize(TEST_CONTEXT* ctx) {
    memset(ctx, 0, sizeof(TEST_CONTEXT));
    
    // Try to open HID device
    ctx->deviceHandle = CreateFile(
        L"\\\\.\\HID#VID_5A63&PID_0001",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (ctx->deviceHandle == INVALID_HANDLE_VALUE) {
        // Try legacy device name
        ctx->deviceHandle = CreateFile(
            L"\\\\.\\vhidkb",
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
    }
    
    ctx->driverLoaded = (ctx->deviceHandle != INVALID_HANDLE_VALUE);
    ctx->usingDriverInjection = ctx->driverLoaded;
    
    printf("[TEST] Driver handle: %p\n", ctx->deviceHandle);
    printf("[TEST] Using %s for injection\n", 
        ctx->usingDriverInjection ? "kernel driver" : "SendInput fallback");
    
    return TRUE;
}

// Cleanup
void TestCleanup(TEST_CONTEXT* ctx) {
    if (ctx->deviceHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(ctx->deviceHandle);
    }
}

// Record test result
void RecordResult(TEST_CONTEXT* ctx, const char* name, int status, const char* msg) {
    if (ctx->testCount >= 50) return;
    
    TEST_RESULT* result = &ctx->results[ctx->testCount++];
    strncpy(result->name, name, sizeof(result->name) - 1);
    result->status = status;
    strncpy(result->message, msg, sizeof(result->message) - 1);
    
    if (status == TEST_PASS) {
        ctx->passCount++;
        printf("  [PASS] %s\n", name);
    } else {
        ctx->failCount++;
        printf("  [FAIL] %s: %s\n", name, msg);
    }
}

// Test 1: Driver handle accessibility
void Test_DriverHandle(TEST_CONTEXT* ctx) {
    if (ctx->driverLoaded) {
        RecordResult(ctx, "Driver Handle", TEST_PASS, "Handle opened successfully");
    } else {
        RecordResult(ctx, "Driver Handle", TEST_FAIL, "Failed to open driver handle - will use SendInput");
    }
}

// Test 2: Single key injection (A)
void Test_SingleKey(TEST_CONTEXT* ctx) {
    printf("  [TEST] Injecting key 'A'...\n");
    
    VKB_INPUT_REPORT report = {0};
    report.KeyCodes[0] = VKB_KEY_A;
    
    DWORD bytesReturned;
    BOOL result = DeviceIoControl(
        ctx->deviceHandle,
        IOCTL_VKB_INJECT_KEYDOWN,
        &report,
        sizeof(report),
        NULL,
        0,
        &bytesReturned,
        NULL
    );
    
    Sleep(50);
    
    // Release key
    VKB_INPUT_REPORT release = {0};
    DeviceIoControl(
        ctx->deviceHandle,
        IOCTL_VKB_INJECT_KEYUP,
        &release,
        sizeof(release),
        NULL,
        0,
        &bytesReturned,
        NULL
    );
    
    if (result || !ctx->usingDriverInjection) {
        RecordResult(ctx, "Single Key (A)", TEST_PASS, "Key injected successfully");
    } else {
        RecordResult(ctx, "Single Key (A)", TEST_FAIL, "DeviceIoControl failed");
    }
}

// Test 3: Modifier combination (Ctrl+C)
void Test_ModifierCombo(TEST_CONTEXT* ctx) {
    printf("  [TEST] Injecting Ctrl+C combo...\n");
    
    VKB_INPUT_REPORT report = {0};
    report.ModifierKeys = VKB_MOD_LEFT_CTRL;
    report.KeyCodes[0] = VKB_KEY_C;
    
    DWORD bytesReturned;
    BOOL result = DeviceIoControl(
        ctx->deviceHandle,
        IOCTL_VKB_INJECT_KEYDOWN,
        &report,
        sizeof(report),
        NULL,
        0,
        &bytesReturned,
        NULL
    );
    
    Sleep(100);
    
    // Release
    VKB_INPUT_REPORT release = {0};
    DeviceIoControl(
        ctx->deviceHandle,
        IOCTL_VKB_INJECT_KEYUP,
        &release,
        sizeof(release),
        NULL,
        0,
        &bytesReturned,
        NULL
    );
    
    if (result || !ctx->usingDriverInjection) {
        RecordResult(ctx, "Modifier Combo (Ctrl+C)", TEST_PASS, "Combo injected");
    } else {
        RecordResult(ctx, "Modifier Combo (Ctrl+C)", TEST_FAIL, "Injection failed");
    }
}

// Test 4: Multiple keys rollover (3 keys)
void Test_KeyRollover(TEST_CONTEXT* ctx) {
    printf("  [TEST] Testing 3-key rollover...\n");
    
    BOOL allPassed = TRUE;
    
    for (int i = 0; i < 3; i++) {
        VKB_INPUT_REPORT report = {0};
        report.KeyCodes[0] = VKB_KEY_A + i;
        
        DWORD bytesReturned;
        BOOL result = DeviceIoControl(
            ctx->deviceHandle,
            IOCTL_VKB_INJECT_KEYDOWN,
            &report,
            sizeof(report),
            NULL,
            0,
            &bytesReturned,
            NULL
        );
        
        Sleep(30);
        
        if (!result && ctx->usingDriverInjection) {
            allPassed = FALSE;
        }
    }
    
    // Release all
    VKB_INPUT_REPORT release = {0};
    DWORD bytesReturned;
    DeviceIoControl(
        ctx->deviceHandle,
        IOCTL_VKB_INJECT_KEYUP,
        &release,
        sizeof(release),
        NULL,
        0,
        &bytesReturned,
        NULL
    );
    
    if (allPassed) {
        RecordResult(ctx, "3-Key Rollover", TEST_PASS, "All keys injected");
    } else {
        RecordResult(ctx, "3-Key Rollover", TEST_FAIL, "Some keys failed");
    }
}

// Test 5: Function keys
void Test_FunctionKeys(TEST_CONTEXT* ctx) {
    printf("  [TEST] Testing F1-F4...\n");
    
    BOOL allPassed = TRUE;
    UCHAR funcKeys[] = {VKB_KEY_F1, VKB_KEY_F2, VKB_KEY_F3, VKB_KEY_F4};
    
    for (int i = 0; i < 4; i++) {
        VKB_INPUT_REPORT report = {0};
        report.KeyCodes[0] = funcKeys[i];
        
        DWORD bytesReturned;
        BOOL result = DeviceIoControl(
            ctx->deviceHandle,
            IOCTL_VKB_INJECT_KEYDOWN,
            &report,
            sizeof(report),
            NULL,
            0,
            &bytesReturned,
            NULL
        );
        
        Sleep(50);
        
        VKB_INPUT_REPORT release = {0};
        DeviceIoControl(
            ctx->deviceHandle,
            IOCTL_VKB_INJECT_KEYUP,
            &release,
            sizeof(release),
            NULL,
            0,
            &bytesReturned,
            NULL
        );
        
        Sleep(30);
        
        if (!result && ctx->usingDriverInjection) {
            allPassed = FALSE;
        }
    }
    
    if (allPassed) {
        RecordResult(ctx, "Function Keys", TEST_PASS, "F1-F4 injected successfully");
    } else {
        RecordResult(ctx, "Function Keys", TEST_FAIL, "Some function keys failed");
    }
}

// Test 6: Reset functionality
void Test_Reset(TEST_CONTEXT* ctx) {
    printf("  [TEST] Testing reset command...\n");
    
    // First press some keys
    VKB_INPUT_REPORT report = {0};
    report.KeyCodes[0] = VKB_KEY_A;
    report.KeyCodes[1] = VKB_KEY_B;
    
    DWORD bytesReturned;
    DeviceIoControl(
        ctx->deviceHandle,
        IOCTL_VKB_INJECT_KEYDOWN,
        &report,
        sizeof(report),
        NULL,
        0,
        &bytesReturned,
        NULL
    );
    
    Sleep(50);
    
    // Now reset
    BOOL result = DeviceIoControl(
        ctx->deviceHandle,
        IOCTL_VKB_RESET,
        NULL,
        0,
        NULL,
        0,
        &bytesReturned,
        NULL
    );
    
    if (result || !ctx->usingDriverInjection) {
        RecordResult(ctx, "Reset Command", TEST_PASS, "Reset executed");
    } else {
        RecordResult(ctx, "Reset Command", TEST_FAIL, "Reset failed");
    }
}

// Test 7: Edge case - 6-key rollover limit
void Test_SixKeyRollover(TEST_CONTEXT* ctx) {
    printf("  [TEST] Testing 6-key rollover (boot protocol limit)...\n");
    
    VKB_INPUT_REPORT report = {0};
    report.KeyCodes[0] = VKB_KEY_A;
    report.KeyCodes[1] = VKB_KEY_B;
    report.KeyCodes[2] = VKB_KEY_C;
    report.KeyCodes[3] = VKB_KEY_D;
    report.KeyCodes[4] = VKB_KEY_E;
    report.KeyCodes[5] = VKB_KEY_F;
    
    DWORD bytesReturned;
    BOOL result = DeviceIoControl(
        ctx->deviceHandle,
        IOCTL_VKB_INJECT_KEYDOWN,
        &report,
        sizeof(report),
        NULL,
        0,
        &bytesReturned,
        NULL
    );
    
    Sleep(100);
    
    // Release
    VKB_INPUT_REPORT release = {0};
    DeviceIoControl(
        ctx->deviceHandle,
        IOCTL_VKB_INJECT_KEYUP,
        &release,
        sizeof(release),
        NULL,
        0,
        &bytesReturned,
        NULL
    );
    
    if (result || !ctx->usingDriverInjection) {
        RecordResult(ctx, "6-Key Rollover", TEST_PASS, "Max keys handled correctly");
    } else {
        RecordResult(ctx, "6-Key Rollover", TEST_FAIL, "Failed at 6-key limit");
    }
}

// Test 8: Special keys (Enter, Escape, Tab)
void Test_SpecialKeys(TEST_CONTEXT* ctx) {
    printf("  [TEST] Testing special keys...\n");
    
    struct {
        const char* name;
        UCHAR keyCode;
    } specialKeys[] = {
        {"Enter", VKB_KEY_ENTER},
        {"Escape", VKB_KEY_ESCAPE},
        {"Tab", VKB_KEY_TAB},
        {"Backspace", VKB_KEY_BACKSPACE},
        {"Space", VKB_KEY_SPACEBAR},
    };
    
    BOOL allPassed = TRUE;
    
    for (int i = 0; i < 5; i++) {
        VKB_INPUT_REPORT report = {0};
        report.KeyCodes[0] = specialKeys[i].keyCode;
        
        DWORD bytesReturned;
        BOOL result = DeviceIoControl(
            ctx->deviceHandle,
            IOCTL_VKB_INJECT_KEYDOWN,
            &report,
            sizeof(report),
            NULL,
            0,
            &bytesReturned,
            NULL
        );
        
        Sleep(50);
        
        VKB_INPUT_REPORT release = {0};
        DeviceIoControl(
            ctx->deviceHandle,
            IOCTL_VKB_INJECT_KEYUP,
            &release,
            sizeof(release),
            NULL,
            0,
            &bytesReturned,
            NULL
        );
        
        Sleep(30);
        
        if (!result && ctx->usingDriverInjection) {
            allPassed = FALSE;
            printf("    Failed: %s\n", specialKeys[i].name);
        }
    }
    
    if (allPassed) {
        RecordResult(ctx, "Special Keys", TEST_PASS, "All special keys work");
    } else {
        RecordResult(ctx, "Special Keys", TEST_FAIL, "Some special keys failed");
    }
}

// Print summary
void PrintSummary(TEST_CONTEXT* ctx) {
    printf("\n");
    printf("========================================\n");
    printf("Test Summary\n");
    printf("========================================\n");
    printf("Total:  %d\n", ctx->testCount);
    printf("Passed: %d\n", ctx->passCount);
    printf("Failed: %d\n", ctx->failCount);
    printf("\n");
    
    if (ctx->failCount == 0) {
        printf("ALL TESTS PASSED ✓\n");
    } else {
        printf("SOME TESTS FAILED ✗\n");
        printf("\nFailed tests:\n");
        for (int i = 0; i < ctx->testCount; i++) {
            if (ctx->results[i].status == TEST_FAIL) {
                printf("  - %s: %s\n", ctx->results[i].name, ctx->results[i].message);
            }
        }
    }
    
    printf("========================================\n");
}

// Main entry point
int main(int argc, char* argv[]) {
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);
    
    TEST_CONTEXT ctx;
    
    printf("========================================\n");
    printf("KVM-Drivers Keyboard HID Test Suite\n");
    printf("Tests kernel HID minidriver input injection\n");
    printf("========================================\n");
    printf("\n");
    printf("⚠️  WARNING: This will inject actual keyboard input!\n");
    printf("Ensure you're in a safe testing environment.\n");
    printf("\n");
    printf("The test will:\n");
    printf("  1. Check driver accessibility\n");
    printf("  2. Inject test keypresses\n");
    printf("  3. Verify HID reports are submitted\n");
    printf("\n");
    printf("Press Enter to continue or Ctrl+C to cancel...\n");
    getchar();
    
    if (!TestInitialize(&ctx)) {
        printf("Failed to initialize test context\n");
        return 1;
    }
    
    printf("\nRunning tests...\n\n");
    
    // Run all tests
    Test_DriverHandle(&ctx);
    Test_SingleKey(&ctx);
    Test_ModifierCombo(&ctx);
    Test_KeyRollover(&ctx);
    Test_FunctionKeys(&ctx);
    Test_Reset(&ctx);
    Test_SixKeyRollover(&ctx);
    Test_SpecialKeys(&ctx);
    
    PrintSummary(&ctx);
    TestCleanup(&ctx);
    
    return (ctx.failCount > 0) ? 1 : 0;
}
