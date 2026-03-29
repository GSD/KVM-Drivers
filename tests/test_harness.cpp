// test_harness.cpp - Automated driver testing framework
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>
#include <iomanip>
#include "../src/usermode/core/driver_interface.h"

// Test result structure
struct TestResult {
    std::string name;
    bool passed;
    std::string message;
    std::chrono::milliseconds duration;
};

class DriverTestHarness {
public:
    DriverTestHarness() : driverInterface(nullptr), logFile("test_results.log") {}

    bool Initialize() {
        driverInterface = new DriverInterface();
        if (!driverInterface) {
            Log("ERROR: Failed to create DriverInterface");
            return false;
        }

        Log("Initializing driver interface...");
        if (!driverInterface->Initialize()) {
            Log("WARNING: Driver initialization reported issues (may use SendInput fallback)");
        }

        bool usingDrivers = driverInterface->IsDriverInjectionAvailable();
        Log(std::string("Injection mode: ") + (usingDrivers ? "Kernel Drivers" : "SendInput Fallback"));

        return true;
    }

    void Shutdown() {
        if (driverInterface) {
            driverInterface->Disconnect();
            delete driverInterface;
            driverInterface = nullptr;
        }
    }

    void RunAllTests() {
        Log("========================================");
        Log("KVM-Drivers Test Harness");
        Log("========================================");

        RunTest("Keyboard Single Key", [this]() { return TestKeyboardSingleKey(); });
        RunTest("Keyboard With Modifiers", [this]() { return TestKeyboardWithModifiers(); });
        RunTest("Keyboard Multiple Keys", [this]() { return TestKeyboardMultipleKeys(); });
        RunTest("Mouse Relative Movement", [this]() { return TestMouseRelativeMove(); });
        RunTest("Mouse Absolute Movement", [this]() { return TestMouseAbsoluteMove(); });
        RunTest("Mouse Buttons", [this]() { return TestMouseButtons(); });
        RunTest("Mouse Scroll", [this]() { return TestMouseScroll(); });

        PrintSummary();
    }

private:
    DriverInterface* driverInterface;
    std::vector<TestResult> results;
    std::string logFile;

    void Log(const std::string& message) {
        std::cout << message << std::endl;
        
        std::ofstream out(logFile, std::ios::app);
        if (out.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::tm localTime;
            localtime_s(&localTime, &time);
            
            out << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S") << " - " << message << std::endl;
        }
    }

    void RunTest(const std::string& name, std::function<bool()> testFunc) {
        std::cout << "\n[TEST] " << name << "... " << std::flush;
        
        auto start = std::chrono::steady_clock::now();
        bool passed = false;
        std::string message;

        try {
            passed = testFunc();
            message = passed ? "PASS" : "FAIL";
        } catch (const std::exception& e) {
            passed = false;
            message = std::string("EXCEPTION: ") + e.what();
        }

        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << (passed ? "PASS" : "FAIL") << " (" << duration.count() << "ms)" << std::endl;
        
        results.push_back({name, passed, message, duration});
        Log(name + ": " + message + " (" + std::to_string(duration.count()) + "ms)");
    }

    // Keyboard Tests
    bool TestKeyboardSingleKey() {
        // Test injecting a single key (A)
        if (!driverInterface->InjectKeyDown(VKB_KEY_A, 0)) {
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        if (!driverInterface->InjectKeyUp(VKB_KEY_A, 0)) {
            return false;
        }
        
        return true;
    }

    bool TestKeyboardWithModifiers() {
        // Test Ctrl+C (copy)
        if (!driverInterface->InjectKeyDown(VKB_KEY_C, VKB_MOD_LEFT_CTRL)) {
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        if (!driverInterface->InjectKeyUp(VKB_KEY_C, VKB_MOD_LEFT_CTRL)) {
            return false;
        }
        
        return true;
    }

    bool TestKeyboardMultipleKeys() {
        // Test typing "Hi"
        // H
        if (!driverInterface->InjectKeyDown(VKB_KEY_H, 0)) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        if (!driverInterface->InjectKeyUp(VKB_KEY_H, 0)) return false;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        
        // i
        if (!driverInterface->InjectKeyDown(VKB_KEY_I, 0)) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        if (!driverInterface->InjectKeyUp(VKB_KEY_I, 0)) return false;
        
        return true;
    }

    // Mouse Tests
    bool TestMouseRelativeMove() {
        // Move mouse right 50 pixels
        if (!driverInterface->InjectMouseMove(50, 0, false)) {
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Move mouse left 50 pixels
        if (!driverInterface->InjectMouseMove(-50, 0, false)) {
            return false;
        }
        
        return true;
    }

    bool TestMouseAbsoluteMove() {
        // Get screen dimensions
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        
        // Move to center
        int centerX = screenWidth / 2;
        int centerY = screenHeight / 2;
        
        if (!driverInterface->InjectMouseMove(centerX, centerY, true)) {
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Move to top-left
        if (!driverInterface->InjectMouseMove(100, 100, true)) {
            return false;
        }
        
        return true;
    }

    bool TestMouseButtons() {
        // Left button down
        if (!driverInterface->InjectMouseButton(VMOUSE_BUTTON_LEFT, true)) {
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Left button up
        if (!driverInterface->InjectMouseButton(VMOUSE_BUTTON_LEFT, false)) {
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Right button click
        if (!driverInterface->InjectMouseButton(VMOUSE_BUTTON_RIGHT, true)) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (!driverInterface->InjectMouseButton(VMOUSE_BUTTON_RIGHT, false)) return false;
        
        return true;
    }

    bool TestMouseScroll() {
        // Scroll up 3 clicks
        if (!driverInterface->InjectMouseScroll(3, 0)) {
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Scroll down 3 clicks
        if (!driverInterface->InjectMouseScroll(-3, 0)) {
            return false;
        }
        
        return true;
    }

    void PrintSummary() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "Test Summary" << std::endl;
        std::cout << "========================================" << std::endl;
        
        int passed = 0;
        int failed = 0;
        
        for (const auto& result : results) {
            if (result.passed) {
                passed++;
            } else {
                failed++;
            }
        }
        
        std::cout << "Total:  " << results.size() << std::endl;
        std::cout << "Passed: " << passed << std::endl;
        std::cout << "Failed: " << failed << std::endl;
        
        double percentage = results.empty() ? 0 : (passed * 100.0 / results.size());
        std::cout << "Success Rate: " << std::fixed << std::setprecision(1) << percentage << "%" << std::endl;
        
        Log("========================================");
        Log("Test Summary: " + std::to_string(passed) + "/" + std::to_string(results.size()) + " passed (" + 
            std::to_string(static_cast<int>(percentage)) + "%)");
    }
};

int main(int argc, char* argv[]) {
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);
    
    std::cout << "KVM-Drivers Test Harness" << std::endl;
    std::cout << "========================" << std::endl;
    std::cout << "\nWARNING: This test will inject actual keyboard and mouse input!" << std::endl;
    std::cout << "Make sure you are in a safe environment (e.g., not typing a password)." << std::endl;
    std::cout << "\nPress Enter to continue or Ctrl+C to cancel..." << std::endl;
    std::cin.get();
    
    DriverTestHarness harness;
    
    if (!harness.Initialize()) {
        std::cerr << "Failed to initialize test harness" << std::endl;
        return 1;
    }
    
    harness.RunAllTests();
    harness.Shutdown();
    
    std::cout << "\nTest complete. Results saved to test_results.log" << std::endl;
    
    return 0;
}
