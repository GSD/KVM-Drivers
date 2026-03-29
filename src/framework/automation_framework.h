// automation_framework.h - KVM-Drivers C++ Automation Framework
// A comprehensive framework for local automation with plugin architecture

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <chrono>
#include <yaml-cpp/yaml.h>

#include "../core/driver_interface.h"

namespace KVMDrivers {
namespace Automation {

// Forward declarations
class AutomationContext;
class TestAction;
class ITestActionHandler;
class TestRunner;
class TestResult;

// Version info
#define AUTOMATION_FRAMEWORK_VERSION "1.0.0"
#define AUTOMATION_FRAMEWORK_VERSION_MAJOR 1
#define AUTOMATION_FRAMEWORK_VERSION_MINOR 0
#define AUTOMATION_FRAMEWORK_VERSION_PATCH 0

// Action types
enum class ActionType {
    Unknown,
    MouseMove,
    MouseClick,
    MouseScroll,
    MouseDrag,
    KeyDown,
    KeyUp,
    KeyPress,
    KeyCombo,
    KeyType,
    Wait,
    Screenshot,
    AssertImage,
    AssertPixel,
    AssertWindow,
    Custom
};

// Action result
struct ActionResult {
    bool success;
    std::string message;
    std::chrono::milliseconds duration;
    std::string screenshotPath;  // If taken
    
    ActionResult() : success(false), duration(0) {}
    
    static ActionResult Ok(const std::string& msg = "", 
        std::chrono::milliseconds dur = std::chrono::milliseconds(0)) {
        ActionResult r;
        r.success = true;
        r.message = msg;
        r.duration = dur;
        return r;
    }
    
    static ActionResult Fail(const std::string& msg,
        std::chrono::milliseconds dur = std::chrono::milliseconds(0)) {
        ActionResult r;
        r.success = false;
        r.message = msg;
        r.duration = dur;
        return r;
    }
};

// Test configuration
struct TestConfig {
    std::string name;
    std::string description;
    int timeoutSeconds = 300;  // 5 minutes default
    bool continueOnError = false;
    bool captureScreenshots = true;
    std::string screenshotDir = "./screenshots";
    std::map<std::string, std::string> variables;  // Test variables
    
    // Retry configuration
    int maxRetries = 0;
    int retryDelayMs = 1000;
};

// Test statistics
struct TestStatistics {
    int totalActions = 0;
    int passedActions = 0;
    int failedActions = 0;
    int skippedActions = 0;
    std::chrono::milliseconds totalDuration{0};
    std::chrono::milliseconds avgActionDuration{0};
    std::chrono::milliseconds minActionDuration{std::chrono::milliseconds::max()};
    std::chrono::milliseconds maxActionDuration{0};
    
    void RecordAction(const ActionResult& result, std::chrono::milliseconds duration) {
        totalActions++;
        if (result.success) {
            passedActions++;
        } else {
            failedActions++;
        }
        
        totalDuration += duration;
        if (duration < minActionDuration) minActionDuration = duration;
        if (duration > maxActionDuration) maxActionDuration = duration;
        
        avgActionDuration = std::chrono::milliseconds(totalDuration.count() / totalActions);
    }
};

// Test step definition
class TestStep {
public:
    int id;
    std::string description;
    ActionType actionType;
    std::map<std::string, std::string> parameters;
    int delayAfterMs = 100;
    bool continueOnError = false;
    int retryCount = 0;
    std::vector<std::shared_ptr<TestStep>> subSteps;  // For complex actions
    
    TestStep() : id(0), actionType(ActionType::Unknown), delayAfterMs(100), 
        continueOnError(false), retryCount(0) {}
    
    // Builder pattern for fluent API
    TestStep& WithId(int stepId) { id = stepId; return *this; }
    TestStep& WithDescription(const std::string& desc) { description = desc; return *this; }
    TestStep& WithAction(ActionType type) { actionType = type; return *this; }
    TestStep& WithParam(const std::string& key, const std::string& value) {
        parameters[key] = value;
        return *this;
    }
    TestStep& WithDelay(int ms) { delayAfterMs = ms; return *this; }
    TestStep& ContinueOnError(bool cont = true) { continueOnError = cont; return *this; }
};

// Test suite definition
class TestSuite {
public:
    TestConfig config;
    std::vector<std::shared_ptr<TestStep>> setupSteps;
    std::vector<std::shared_ptr<TestStep>> testSteps;
    std::vector<std::shared_ptr<TestStep>> teardownSteps;
    std::vector<std::shared_ptr<TestStep>> assertions;
    
    // Load from YAML
    bool LoadFromFile(const std::string& filepath);
    bool LoadFromString(const std::string& yamlContent);
    
    // Save to YAML
    bool SaveToFile(const std::string& filepath) const;
    std::string ToYamlString() const;
    
    // Builder API
    TestSuite& WithName(const std::string& name) { config.name = name; return *this; }
    TestSuite& WithDescription(const std::string& desc) { config.description = desc; return *this; }
    TestSuite& WithTimeout(int seconds) { config.timeoutSeconds = seconds; return *this; }
    
    TestSuite& AddSetupStep(std::shared_ptr<TestStep> step) {
        setupSteps.push_back(step);
        return *this;
    }
    
    TestSuite& AddTestStep(std::shared_ptr<TestStep> step) {
        testSteps.push_back(step);
        return *this;
    }
    
    TestSuite& AddTeardownStep(std::shared_ptr<TestStep> step) {
        teardownSteps.push_back(step);
        return *this;
    }
};

// Test result for a single step
struct StepResult {
    int stepId;
    std::string description;
    ActionResult actionResult;
    std::chrono::milliseconds startTime;  // Relative to test start
    std::chrono::milliseconds duration;
    int retryAttempts = 0;
    std::string screenshotPath;
};

// Complete test result
class TestResult {
public:
    bool overallSuccess = false;
    std::string testName;
    std::chrono::milliseconds totalDuration{0};
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point endTime;
    
    std::vector<StepResult> setupResults;
    std::vector<StepResult> testResults;
    std::vector<StepResult> teardownResults;
    std::vector<StepResult> assertionResults;
    
    TestStatistics statistics;
    std::string logOutput;
    std::string errorMessage;
    
    // Summary
    int TotalSteps() const {
        return (int)(setupResults.size() + testResults.size() + 
            teardownResults.size() + assertionResults.size());
    }
    
    int PassedSteps() const {
        int passed = 0;
        auto countPassed = [&passed](const std::vector<StepResult>& results) {
            for (const auto& r : results) {
                if (r.actionResult.success) passed++;
            }
        };
        countPassed(setupResults);
        countPassed(testResults);
        countPassed(teardownResults);
        countPassed(assertionResults);
        return passed;
    }
    
    int FailedSteps() const { return TotalSteps() - PassedSteps(); }
    
    std::string ToJson() const;
    std::string ToXml() const;
    std::string ToMarkdown() const;  // For GitHub/GitLab CI
    
    bool SaveToFile(const std::string& filepath) const;
};

// Action handler interface - for plugins
class ITestActionHandler {
public:
    virtual ~ITestActionHandler() = default;
    
    // Handler identification
    virtual std::string GetName() const = 0;
    virtual std::string GetDescription() const = 0;
    virtual ActionType GetActionType() const = 0;
    
    // Parameter validation
    virtual bool ValidateParameters(const std::map<std::string, std::string>& params,
        std::string& errorMessage) const = 0;
    
    // Execute the action
    virtual ActionResult Execute(const TestStep& step,
        AutomationContext& context) = 0;
    
    // Retry support
    virtual bool IsRetryable() const { return true; }
    virtual int GetMaxRetries() const { return 3; }
};

// Automation context - passed to all actions
class AutomationContext {
public:
    // Driver interface
    DriverInterface* driverInterface = nullptr;
    
    // Test configuration
    TestConfig* currentTestConfig = nullptr;
    
    // Test variables (for interpolation)
    std::map<std::string, std::string> variables;
    
    // Current state
    int currentStepNumber = 0;
    std::chrono::steady_clock::time_point testStartTime;
    
    // Screenshots
    std::string lastScreenshotPath;
    
    // Logging
    std::function<void(const std::string&)> logCallback;
    
    // Progress callback
    std::function<void(int currentStep, int totalSteps, const std::string& stepName)> 
        progressCallback;
    
    // Error recovery
    std::function<bool(const std::string& error, int retryCount)> errorRecoveryCallback;
    
    // Variable interpolation
    std::string InterpolateVariables(const std::string& input) const;
    
    // Logging helpers
    void LogInfo(const std::string& message);
    void LogWarning(const std::string& message);
    void LogError(const std::string& message);
    void LogDebug(const std::string& message);
};

// Built-in action handlers
class MouseActionHandler : public ITestActionHandler {
public:
    std::string GetName() const override { return "MouseActionHandler"; }
    std::string GetDescription() const override { return "Handles mouse move, click, scroll, drag"; }
    ActionType GetActionType() const override { return ActionType::MouseClick; }  // Primary type
    
    bool ValidateParameters(const std::map<std::string, std::string>& params,
        std::string& errorMessage) const override;
    
    ActionResult Execute(const TestStep& step, AutomationContext& context) override;
    
private:
    ActionResult HandleMouseMove(const TestStep& step, AutomationContext& context);
    ActionResult HandleMouseClick(const TestStep& step, AutomationContext& context);
    ActionResult HandleMouseScroll(const TestStep& step, AutomationContext& context);
    ActionResult HandleMouseDrag(const TestStep& step, AutomationContext& context);
};

class KeyboardActionHandler : public ITestActionHandler {
public:
    std::string GetName() const override { return "KeyboardActionHandler"; }
    std::string GetDescription() const override { return "Handles keyboard key press, combo, type"; }
    ActionType GetActionType() const override { return ActionType::KeyPress; }
    
    bool ValidateParameters(const std::map<std::string, std::string>& params,
        std::string& errorMessage) const override;
    
    ActionResult Execute(const TestStep& step, AutomationContext& context) override;
    
private:
    int KeyNameToCode(const std::string& name) const;
    ActionResult HandleKeyDown(const TestStep& step, AutomationContext& context);
    ActionResult HandleKeyUp(const TestStep& step, AutomationContext& context);
    ActionResult HandleKeyPress(const TestStep& step, AutomationContext& context);
    ActionResult HandleKeyCombo(const TestStep& step, AutomationContext& context);
    ActionResult HandleType(const TestStep& step, AutomationContext& context);
};

class WaitActionHandler : public ITestActionHandler {
public:
    std::string GetName() const override { return "WaitActionHandler"; }
    std::string GetDescription() const override { return "Handles wait/delay actions"; }
    ActionType GetActionType() const override { return ActionType::Wait; }
    
    bool ValidateParameters(const std::map<std::string, std::string>& params,
        std::string& errorMessage) const override;
    
    ActionResult Execute(const TestStep& step, AutomationContext& context) override;
};

class ScreenshotActionHandler : public ITestActionHandler {
public:
    std::string GetName() const override { return "ScreenshotActionHandler"; }
    std::string GetDescription() const override { return "Handles screen capture"; }
    ActionType GetActionType() const override { return ActionType::Screenshot; }
    
    bool ValidateParameters(const std::map<std::string, std::string>& params,
        std::string& errorMessage) const override;
    
    ActionResult Execute(const TestStep& step, AutomationContext& context) override;
    
private:
    bool CaptureScreen(const std::string& filepath);
};

// Main test runner
class TestRunner {
public:
    TestRunner();
    ~TestRunner();
    
    // Initialize/shutdown
    bool Initialize();
    void Shutdown();
    
    // Register action handlers (for plugins)
    void RegisterHandler(std::shared_ptr<ITestActionHandler> handler);
    void UnregisterHandler(const std::string& name);
    std::vector<std::string> GetRegisteredHandlers() const;
    
    // Execute tests
    TestResult RunTest(const TestSuite& suite);
    std::vector<TestResult> RunTests(const std::vector<TestSuite>& suites);
    
    // Execute single action (for interactive mode)
    ActionResult ExecuteAction(const TestStep& step);
    
    // Set callbacks
    void SetLogCallback(std::function<void(const std::string&)> callback);
    void SetProgressCallback(std::function<void(int, int, const std::string&)> callback);
    void SetErrorRecoveryCallback(std::function<bool(const std::string&, int)> callback);
    
    // Configuration
    void SetScreenshotDirectory(const std::string& dir);
    void SetDefaultTimeout(int seconds);
    
    // Get context (for advanced usage)
    AutomationContext& GetContext() { return context_; }
    
    // Version info
    static std::string GetVersion();
    static std::string GetBuildInfo();

private:
    AutomationContext context_;
    std::map<ActionType, std::shared_ptr<ITestActionHandler>> handlers_;
    std::map<std::string, std::shared_ptr<ITestActionHandler>> handlersByName_;
    bool initialized_ = false;
    
    // Built-in handlers
    void RegisterBuiltInHandlers();
    
    // Execution helpers
    std::vector<StepResult> ExecuteSteps(const std::vector<std::shared_ptr<TestStep>>& steps,
        const std::string& phase);
    StepResult ExecuteSingleStep(const TestStep& step);
    
    // Find handler
    std::shared_ptr<ITestActionHandler> FindHandler(ActionType type) const;
    std::shared_ptr<ITestActionHandler> FindHandler(const std::string& name) const;
};

// Interactive session (REPL)
class InteractiveSession {
public:
    InteractiveSession(TestRunner& runner);
    ~InteractiveSession();
    
    // Run interactive loop
    void Run();
    void RunCommand(const std::string& command);
    
    // Command handlers
    void HandleHelp();
    void HandleClick(const std::string& args);
    void HandleMove(const std::string& args);
    void HandleKey(const std::string& args);
    void HandleType(const std::string& args);
    void HandleCombo(const std::string& args);
    void HandleWait(const std::string& args);
    void HandleScreenshot(const std::string& args);
    void HandleStatus();
    void HandleVariable(const std::string& args);
    void HandleExecute(const std::string& args);
    
private:
    TestRunner& runner_;
    bool running_ = false;
    std::map<std::string, std::function<void(const std::string&)>> commands_;
    
    void RegisterCommands();
};

// Smoke test suite
class SmokeTestSuite {
public:
    static TestSuite Create();
    static bool Run(TestRunner& runner);
};

// Utility functions
namespace Utils {
    // Parse coordinate string "100,200" -> pair(100, 200)
    bool ParseCoordinates(const std::string& str, int& x, int& y);
    
    // Parse key combination "ctrl+shift+a"
    std::vector<std::string> ParseKeyCombo(const std::string& str);
    
    // Generate timestamp string
    std::string GetTimestamp();
    
    // Ensure directory exists
    bool EnsureDirectory(const std::string& path);
    
    // Variable interpolation "Hello ${name}" -> "Hello World"
    std::string InterpolateVariables(const std::string& input,
        const std::map<std::string, std::string>& vars);
}

} // namespace Automation
} // namespace KVMDrivers

// C API for external language bindings
extern "C" {
    // Framework handle
    typedef void* KvmAutomationHandle;
    typedef void* KvmTestSuiteHandle;
    typedef void* KvmTestResultHandle;
    
    // Lifecycle
    __declspec(dllexport) KvmAutomationHandle KvmAutomation_Create(void);
    __declspec(dllexport) void KvmAutomation_Destroy(KvmAutomationHandle handle);
    __declspec(dllexport) int KvmAutomation_Initialize(KvmAutomationHandle handle);
    __declspec(dllexport) void KvmAutomation_Shutdown(KvmAutomationHandle handle);
    
    // Test execution
    __declspec(dllexport) KvmTestSuiteHandle KvmAutomation_LoadTestSuite(
        KvmAutomationHandle handle, const char* filepath);
    __declspec(dllexport) KvmTestResultHandle KvmAutomation_RunTest(
        KvmAutomationHandle handle, KvmTestSuiteHandle suite);
    __declspec(dllexport) int KvmAutomation_RunSmokeTest(KvmAutomationHandle handle);
    
    // Result access
    __declspec(dllexport) int KvmResult_GetSuccess(KvmTestResultHandle result);
    __declspec(dllexport) int KvmResult_GetTotalSteps(KvmTestResultHandle result);
    __declspec(dllexport) int KvmResult_GetPassedSteps(KvmTestResultHandle result);
    __declspec(dllexport) int KvmResult_GetFailedSteps(KvmTestResultHandle result);
    __declspec(dllexport) void KvmResult_SaveToFile(KvmTestResultHandle result, 
        const char* filepath, const char* format);  // "json", "xml", "md"
    __declspec(dllexport) void KvmResult_Destroy(KvmTestResultHandle result);
    
    // Cleanup
    __declspec(dllexport) void KvmTestSuite_Destroy(KvmTestSuiteHandle suite);
    
    // Version
    __declspec(dllexport) const char* KvmAutomation_GetVersion(void);
}
