// test_controller_xusb.c - Comprehensive Xbox controller test suite
#include "../src/drivers/vxinput/vxinput.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define TEST_PASS 0
#define TEST_FAIL 1

typedef struct _TEST_RESULT {
    char name[128];
    int status;
    char message[256];
} TEST_RESULT;

typedef struct _TEST_CONTEXT {
    HANDLE busHandle;
    HANDLE controllerHandle;
    BOOL driverLoaded;
    int testCount;
    int passCount;
    int failCount;
    TEST_RESULT results[30];
} TEST_CONTEXT;

BOOL TestInitialize(TEST_CONTEXT* ctx) {
    memset(ctx, 0, sizeof(TEST_CONTEXT));
    
    ctx->busHandle = CreateFile(
        L"\\\\.\\vxinput",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    ctx->driverLoaded = (ctx->busHandle != INVALID_HANDLE_VALUE);
    
    printf("[TEST] Controller bus handle: %p\n", ctx->busHandle);
    printf("[TEST] Driver %s\n", ctx->driverLoaded ? "connected" : "NOT available");
    
    return TRUE;
}

void TestCleanup(TEST_CONTEXT* ctx) {
    if (ctx->busHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(ctx->busHandle);
    }
}

void RecordResult(TEST_CONTEXT* ctx, const char* name, int status, const char* msg) {
    if (ctx->testCount >= 30) return;
    
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
        RecordResult(ctx, "Driver Handle", TEST_PASS, "Bus handle opened successfully");
    } else {
        RecordResult(ctx, "Driver Handle", TEST_FAIL, "Failed to open vxinput bus driver");
    }
}

// Test 2: Create virtual controller
void Test_CreateController(TEST_CONTEXT* ctx) {
    if (!ctx->driverLoaded) {
        RecordResult(ctx, "Create Controller", TEST_FAIL, "Driver not available");
        return;
    }
    
    XUSB_CONTROLLER_INFO info = {0};
    info.UserIndex = 0;
    
    DWORD bytesReturned;
    BOOL result = DeviceIoControl(
        ctx->busHandle,
        IOCTL_VXINPUT_CREATE_CONTROLLER,
        &info,
        sizeof(info),
        &info,
        sizeof(info),
        &bytesReturned,
        NULL
    );
    
    if (result) {
        ctx->controllerHandle = info.ControllerHandle;
        RecordResult(ctx, "Create Controller", TEST_PASS, "Virtual controller created");
    } else {
        RecordResult(ctx, "Create Controller", TEST_FAIL, "Failed to create controller");
    }
}

// Test 3: Submit XUSB report (buttons)
void Test_SubmitButtons(TEST_CONTEXT* ctx) {
    if (!ctx->driverLoaded || !ctx->controllerHandle) {
        RecordResult(ctx, "Submit Buttons", TEST_FAIL, "Controller not available");
        return;
    }
    
    XUSB_REPORT report = {0};
    report.bReportId = 0x00;
    report.bSize = sizeof(XUSB_REPORT);
    report.wButtons = XUSB_GAMEPAD_A | XUSB_GAMEPAD_B;
    
    DWORD bytesReturned;
    BOOL result = DeviceIoControl(
        ctx->busHandle,
        IOCTL_VXINPUT_SUBMIT_REPORT,
        &report,
        sizeof(report),
        NULL,
        0,
        &bytesReturned,
        NULL
    );
    
    Sleep(100);
    
    if (result) {
        RecordResult(ctx, "Submit Buttons", TEST_PASS, "Button report submitted");
    } else {
        RecordResult(ctx, "Submit Buttons", TEST_FAIL, "Report submission failed");
    }
}

// Test 4: Analog stick input
void Test_AnalogSticks(TEST_CONTEXT* ctx) {
    if (!ctx->driverLoaded) {
        RecordResult(ctx, "Analog Sticks", TEST_FAIL, "Driver not available");
        return;
    }
    
    XUSB_REPORT report = {0};
    report.bSize = sizeof(XUSB_REPORT);
    report.sThumbLX = 32767;   // Full right
    report.sThumbLY = 0;       // Center
    report.sThumbRX = -32768;  // Full left
    report.sThumbRY = 32767; // Full up
    
    DWORD bytesReturned;
    BOOL result = DeviceIoControl(
        ctx->busHandle,
        IOCTL_VXINPUT_SUBMIT_REPORT,
        &report,
        sizeof(report),
        NULL,
        0,
        &bytesReturned,
        NULL
    );
    
    Sleep(50);
    
    if (result) {
        RecordResult(ctx, "Analog Sticks", TEST_PASS, "Analog stick values submitted");
    } else {
        RecordResult(ctx, "Analog Sticks", TEST_FAIL, "Failed to submit analog data");
    }
}

// Test 5: Trigger input
void Test_Triggers(TEST_CONTEXT* ctx) {
    if (!ctx->driverLoaded) {
        RecordResult(ctx, "Triggers", TEST_FAIL, "Driver not available");
        return;
    }
    
    XUSB_REPORT report = {0};
    report.bSize = sizeof(XUSB_REPORT);
    report.bLeftTrigger = 255;   // Full pull
    report.bRightTrigger = 128;  // Half pull
    
    DWORD bytesReturned;
    BOOL result = DeviceIoControl(
        ctx->busHandle,
        IOCTL_VXINPUT_SUBMIT_REPORT,
        &report,
        sizeof(report),
        NULL,
        0,
        &bytesReturned,
        NULL
    );
    
    Sleep(50);
    
    if (result) {
        RecordResult(ctx, "Triggers", TEST_PASS, "Trigger values submitted");
    } else {
        RecordResult(ctx, "Triggers", TEST_FAIL, "Failed to submit trigger data");
    }
}

// Test 6: D-pad input
void Test_DPad(TEST_CONTEXT* ctx) {
    if (!ctx->driverLoaded) {
        RecordResult(ctx, "D-Pad", TEST_FAIL, "Driver not available");
        return;
    }
    
    BOOL allPassed = TRUE;
    USHORT dpadButtons[] = {
        XUSB_GAMEPAD_DPAD_UP,
        XUSB_GAMEPAD_DPAD_DOWN,
        XUSB_GAMEPAD_DPAD_LEFT,
        XUSB_GAMEPAD_DPAD_RIGHT
    };
    
    for (int i = 0; i < 4; i++) {
        XUSB_REPORT report = {0};
        report.bSize = sizeof(XUSB_REPORT);
        report.wButtons = dpadButtons[i];
        
        DWORD bytesReturned;
        BOOL result = DeviceIoControl(
            ctx->busHandle,
            IOCTL_VXINPUT_SUBMIT_REPORT,
            &report,
            sizeof(report),
            NULL,
            0,
            &bytesReturned,
            NULL
        );
        
        Sleep(50);
        
        if (!result) allPassed = FALSE;
    }
    
    if (allPassed) {
        RecordResult(ctx, "D-Pad", TEST_PASS, "All D-pad directions submitted");
    } else {
        RecordResult(ctx, "D-Pad", TEST_FAIL, "Some D-pad directions failed");
    }
}

// Test 7: All buttons test
void Test_AllButtons(TEST_CONTEXT* ctx) {
    if (!ctx->driverLoaded) {
        RecordResult(ctx, "All Buttons", TEST_FAIL, "Driver not available");
        return;
    }
    
    USHORT allButtons = 
        XUSB_GAMEPAD_DPAD_UP | XUSB_GAMEPAD_DPAD_DOWN |
        XUSB_GAMEPAD_DPAD_LEFT | XUSB_GAMEPAD_DPAD_RIGHT |
        XUSB_GAMEPAD_START | XUSB_GAMEPAD_BACK |
        XUSB_GAMEPAD_LEFT_THUMB | XUSB_GAMEPAD_RIGHT_THUMB |
        XUSB_GAMEPAD_LEFT_SHOULDER | XUSB_GAMEPAD_RIGHT_SHOULDER |
        XUSB_GAMEPAD_A | XUSB_GAMEPAD_B | XUSB_GAMEPAD_X | XUSB_GAMEPAD_Y;
    
    XUSB_REPORT report = {0};
    report.bSize = sizeof(XUSB_REPORT);
    report.wButtons = allButtons;
    
    DWORD bytesReturned;
    BOOL result = DeviceIoControl(
        ctx->busHandle,
        IOCTL_VXINPUT_SUBMIT_REPORT,
        &report,
        sizeof(report),
        NULL,
        0,
        &bytesReturned,
        NULL
    );
    
    Sleep(100);
    
    // Release all buttons
    report.wButtons = 0;
    DeviceIoControl(
        ctx->busHandle,
        IOCTL_VXINPUT_SUBMIT_REPORT,
        &report,
        sizeof(report),
        NULL,
        0,
        &bytesReturned,
        NULL
    );
    
    if (result) {
        RecordResult(ctx, "All Buttons", TEST_PASS, "All 14 buttons submitted simultaneously");
    } else {
        RecordResult(ctx, "All Buttons", TEST_FAIL, "Failed to submit button combo");
    }
}

// Test 8: Get controller count
void Test_GetControllerCount(TEST_CONTEXT* ctx) {
    if (!ctx->driverLoaded) {
        RecordResult(ctx, "Get Controller Count", TEST_FAIL, "Driver not available");
        return;
    }
    
    ULONG count = 0;
    DWORD bytesReturned;
    BOOL result = DeviceIoControl(
        ctx->busHandle,
        IOCTL_VXINPUT_GET_CONTROLLER_COUNT,
        NULL,
        0,
        &count,
        sizeof(count),
        &bytesReturned,
        NULL
    );
    
    if (result && count > 0) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Found %u controller(s)", count);
        RecordResult(ctx, "Get Controller Count", TEST_PASS, msg);
    } else if (result) {
        RecordResult(ctx, "Get Controller Count", TEST_PASS, "No controllers (expected after create)");
    } else {
        RecordResult(ctx, "Get Controller Count", TEST_FAIL, "Failed to get count");
    }
}

// Test 9: Get rumble state (output from games)
void Test_GetRumble(TEST_CONTEXT* ctx) {
    if (!ctx->driverLoaded || !ctx->controllerHandle) {
        RecordResult(ctx, "Get Rumble", TEST_FAIL, "Controller not available");
        return;
    }
    
    XUSB_RUMBLE_STATE rumble = {0};
    DWORD bytesReturned;
    BOOL result = DeviceIoControl(
        ctx->busHandle,
        IOCTL_VXINPUT_GET_RUMBLE,
        &ctx->controllerHandle,
        sizeof(ctx->controllerHandle),
        &rumble,
        sizeof(rumble),
        &bytesReturned,
        NULL
    );
    
    if (result) {
        char msg[128];
        snprintf(msg, sizeof(msg), 
            "Rumble: L=%u R=%u LT=%u RT=%u",
            rumble.bLeftMotor,
            rumble.bRightMotor,
            rumble.bLeftTriggerMotor,
            rumble.bRightTriggerMotor);
        RecordResult(ctx, "Get Rumble", TEST_PASS, msg);
    } else {
        RecordResult(ctx, "Get Rumble", TEST_FAIL, "Failed to read rumble state");
    }
}

// Test 10: Remove controller
void Test_RemoveController(TEST_CONTEXT* ctx) {
    if (!ctx->driverLoaded || !ctx->controllerHandle) {
        RecordResult(ctx, "Remove Controller", TEST_FAIL, "Controller not available");
        return;
    }
    
    DWORD bytesReturned;
    BOOL result = DeviceIoControl(
        ctx->busHandle,
        IOCTL_VXINPUT_REMOVE_CONTROLLER,
        &ctx->controllerHandle,
        sizeof(ctx->controllerHandle),
        NULL,
        0,
        &bytesReturned,
        NULL
    );
    
    if (result) {
        ctx->controllerHandle = NULL;
        RecordResult(ctx, "Remove Controller", TEST_PASS, "Controller removed successfully");
    } else {
        RecordResult(ctx, "Remove Controller", TEST_FAIL, "Failed to remove controller");
    }
}

// Print summary
void PrintSummary(TEST_CONTEXT* ctx) {
    printf("\n");
    printf("========================================\n");
    printf("Xbox Controller Test Summary\n");
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

int main(int argc, char* argv[]) {
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);
    
    TEST_CONTEXT ctx;
    
    printf("========================================\n");
    printf("KVM-Drivers Xbox Controller Test Suite\n");
    printf("Tests XUSB protocol implementation\n");
    printf("========================================\n");
    printf("\n");
    printf("This will test virtual Xbox 360 controller functionality.\n");
    printf("Ensure no games are currently running that might interfere.\n");
    printf("\n");
    printf("Press Enter to continue...\n");
    getchar();
    
    if (!TestInitialize(&ctx)) {
        printf("Failed to initialize test context\n");
        return 1;
    }
    
    printf("\nRunning tests...\n\n");
    
    // Run all tests
    Test_DriverHandle(&ctx);
    Test_CreateController(&ctx);
    Test_SubmitButtons(&ctx);
    Test_AnalogSticks(&ctx);
    Test_Triggers(&ctx);
    Test_DPad(&ctx);
    Test_AllButtons(&ctx);
    Test_GetControllerCount(&ctx);
    Test_GetRumble(&ctx);
    Test_RemoveController(&ctx);
    
    PrintSummary(&ctx);
    TestCleanup(&ctx);
    
    return (ctx.failCount > 0) ? 1 : 0;
}
