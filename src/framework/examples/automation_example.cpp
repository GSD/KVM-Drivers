// automation_example.cpp - Example usage of the KVM-Drivers Automation Framework

#include "automation_framework.h"
#include <iostream>
#include <memory>

using namespace KVMDrivers::Automation;

// Custom action handler example
class CustomActionHandler : public ITestActionHandler {
public:
    std::string GetName() const override { return "CustomActionHandler"; }
    std::string GetDescription() const override { return "Example custom action"; }
    ActionType GetActionType() const override { return ActionType::Custom; }
    
    bool ValidateParameters(const std::map<std::string, std::string>& params,
        std::string& errorMessage) const override {
        if (params.find("custom_param") == params.end()) {
            errorMessage = "Missing custom_param";
            return false;
        }
        return true;
    }
    
    ActionResult Execute(const TestStep& step, AutomationContext& context) override {
        auto it = step.parameters.find("custom_param");
        std::string value = context.InterpolateVariables(it->second);
        
        context.LogInfo("Executing custom action with param: " + value);
        
        // Do custom work here
        
        return ActionResult::Ok("Custom action completed: " + value);
    }
};

// Progress callback
void OnProgress(int current, int total, const std::string& stepName) {
    std::cout << "[Progress] Step " << current << "/" << total << ": " << stepName << std::endl;
}

// Log callback
void OnLog(const std::string& message) {
    std::cout << message << std::endl;
}

// Example 1: Programmatic test creation
void Example_ProgrammaticTest() {
    std::cout << "\n========================================\n";
    std::cout << "Example 1: Programmatic Test Creation\n";
    std::cout << "========================================\n\n";
    
    // Create test runner
    TestRunner runner;
    runner.SetLogCallback(OnLog);
    runner.SetProgressCallback(OnProgress);
    
    if (!runner.Initialize()) {
        std::cerr << "Failed to initialize runner\n";
        return;
    }
    
    // Register custom handler
    runner.RegisterHandler(std::make_shared<CustomActionHandler>());
    
    // Build test suite using fluent API
    TestSuite suite;
    suite.WithName("Login Flow Test")
         .WithDescription("Test user login process")
         .WithTimeout(300)
         .WithVariable("username", "testuser")
         .WithVariable("password", "testpass123");
    
    // Add setup steps
    suite.AddSetupStep(
        std::make_shared<TestStep>()
        ->WithDescription("Navigate to login page")
        .WithAction(ActionType::MouseClick)
        .WithParam("x", "960")
        .WithParam("y", "540")
        .WithDelay(500)
    );
    
    // Add test steps
    suite.AddTestStep(
        std::make_shared<TestStep>()
        ->WithId(1)
        .WithDescription("Click username field")
        .WithAction(ActionType::MouseClick)
        .WithParam("x", "800")
        .WithParam("y", "400")
        .WithDelay(100)
    );
    
    suite.AddTestStep(
        std::make_shared<TestStep>()
        ->WithId(2)
        .WithDescription("Type username")
        .WithAction(ActionType::KeyType)
        .WithParam("text", "${username}")
        .WithParam("interval_ms", "10")
        .WithDelay(200)
    );
    
    suite.AddTestStep(
        std::make_shared<TestStep>()
        ->WithId(3)
        .WithDescription("Press Tab to password field")
        .WithAction(ActionType::KeyPress)
        .WithParam("keycode", "9")  // Tab
        .WithDelay(100)
    );
    
    suite.AddTestStep(
        std::make_shared<TestStep>()
        ->WithId(4)
        .WithDescription("Type password")
        .WithAction(ActionType::KeyType)
        .WithParam("text", "${password}")
        .WithParam("interval_ms", "10")
        .WithDelay(200)
    );
    
    suite.AddTestStep(
        std::make_shared<TestStep>()
        ->WithId(5)
        .WithDescription("Press Enter to submit")
        .WithAction(ActionType::KeyPress)
        .WithParam("keycode", "13")  // Enter
        .WithDelay(2000)  // Wait for page load
    );
    
    suite.AddTestStep(
        std::make_shared<TestStep>()
        ->WithId(6)
        .WithDescription("Take screenshot")
        .WithAction(ActionType::Screenshot)
        .WithParam("name", "login_result")
    );
    
    // Add teardown
    suite.AddTeardownStep(
        std::make_shared<TestStep>()
        ->WithDescription("Logout")
        .WithAction(ActionType::KeyCombo)
        .WithParam("keys", "ctrl+shift+q")
    );
    
    // Run test
    TestResult result = runner.RunTest(suite);
    
    // Output results
    std::cout << "\nTest Result: " << (result.overallSuccess ? "PASSED" : "FAILED") << "\n";
    std::cout << "Duration: " << result.totalDuration.count() << "ms\n";
    std::cout << "Steps: " << result.PassedSteps() << "/" << result.TotalSteps() << " passed\n";
    
    // Save results
    result.SaveToFile("test_result.json");
    result.SaveToFile("test_result.md");
    
    runner.Shutdown();
}

// Example 2: Load and run YAML test
void Example_YamlTest() {
    std::cout << "\n========================================\n";
    std::cout << "Example 2: YAML Test Loading\n";
    std::cout << "========================================\n\n";
    
    TestRunner runner;
    runner.SetLogCallback(OnLog);
    
    if (!runner.Initialize()) {
        std::cerr << "Failed to initialize runner\n";
        return;
    }
    
    // Load test from YAML file
    TestSuite suite;
    if (!suite.LoadFromFile("tests/smoke_test.yaml")) {
        std::cerr << "Failed to load test file\n";
        runner.Shutdown();
        return;
    }
    
    // Run test
    TestResult result = runner.RunTest(suite);
    
    // Output JSON result
    std::cout << "\nJSON Output:\n";
    std::cout << result.ToJson() << "\n";
    
    runner.Shutdown();
}

// Example 3: Interactive mode
void Example_Interactive() {
    std::cout << "\n========================================\n";
    std::cout << "Example 3: Interactive Mode\n";
    std::cout << "========================================\n\n";
    
    TestRunner runner;
    if (!runner.Initialize()) {
        std::cerr << "Failed to initialize runner\n";
        return;
    }
    
    InteractiveSession session(runner);
    session.Run();  // Runs until user types 'exit'
    
    runner.Shutdown();
}

// Example 4: Smoke test
void Example_SmokeTest() {
    std::cout << "\n========================================\n";
    std::cout << "Example 4: Built-in Smoke Test\n";
    std::cout << "========================================\n\n";
    
    TestRunner runner;
    runner.SetLogCallback(OnLog);
    
    if (!runner.Initialize()) {
        std::cerr << "Failed to initialize runner\n";
        return;
    }
    
    // Run built-in smoke test
    bool passed = SmokeTestSuite::Run(runner);
    
    std::cout << "\nSmoke Test: " << (passed ? "PASSED" : "FAILED") << "\n";
    
    runner.Shutdown();
}

// Example 5: Batch test execution
void Example_BatchExecution() {
    std::cout << "\n========================================\n";
    std::cout << "Example 5: Batch Test Execution\n";
    std::cout << "========================================\n\n";
    
    std::vector<TestSuite> suites;
    
    // Load multiple test files
    std::vector<std::string> testFiles = {
        "tests/login_test.yaml",
        "tests/navigation_test.yaml",
        "tests/form_test.yaml"
    };
    
    for (const auto& file : testFiles) {
        TestSuite suite;
        if (suite.LoadFromFile(file)) {
            suites.push_back(suite);
        }
    }
    
    TestRunner runner;
    runner.SetLogCallback(OnLog);
    runner.SetProgressCallback(OnProgress);
    
    if (!runner.Initialize()) {
        std::cerr << "Failed to initialize runner\n";
        return;
    }
    
    // Run all tests
    std::vector<TestResult> results = runner.RunTests(suites);
    
    // Summary
    int passed = 0;
    for (const auto& result : results) {
        if (result.overallSuccess) passed++;
        std::cout << result.testName << ": " 
                  << (result.overallSuccess ? "PASSED" : "FAILED") << "\n";
    }
    
    std::cout << "\nSummary: " << passed << "/" << results.size() << " tests passed\n";
    
    runner.Shutdown();
}

// Example 6: Single action execution
void Example_SingleAction() {
    std::cout << "\n========================================\n";
    std::cout << "Example 6: Single Action Execution\n";
    std::cout << "========================================\n\n";
    
    TestRunner runner;
    if (!runner.Initialize()) {
        std::cerr << "Failed to initialize runner\n";
        return;
    }
    
    // Create a single action
    TestStep step;
    step.description = "Click at coordinates";
    step.actionType = ActionType::MouseClick;
    step.parameters["x"] = "100";
    step.parameters["y"] = "200";
    step.parameters["button"] = "left";
    
    // Execute
    ActionResult result = runner.ExecuteAction(step);
    
    std::cout << "Action " << (result.success ? "succeeded" : "failed") 
              << ": " << result.message << "\n";
    std::cout << "Duration: " << result.duration.count() << "ms\n";
    
    runner.Shutdown();
}

// Main entry point
int main(int argc, char* argv[]) {
    std::cout << "KVM-Drivers Automation Framework Examples\n";
    std::cout << "Version: " << TestRunner::GetVersion() << "\n";
    std::cout << "Build: " << TestRunner::GetBuildInfo() << "\n";
    
    if (argc > 1) {
        std::string arg = argv[1];
        
        if (arg == "--programmatic") {
            Example_ProgrammaticTest();
        } else if (arg == "--yaml") {
            Example_YamlTest();
        } else if (arg == "--interactive") {
            Example_Interactive();
        } else if (arg == "--smoke") {
            Example_SmokeTest();
        } else if (arg == "--batch") {
            Example_BatchExecution();
        } else if (arg == "--single") {
            Example_SingleAction();
        } else if (arg == "--help") {
            std::cout << "\nUsage: automation_example [option]\n\n";
            std::cout << "Options:\n";
            std::cout << "  --programmatic  Run programmatic test example\n";
            std::cout << "  --yaml          Run YAML loading example\n";
            std::cout << "  --interactive   Run interactive mode example\n";
            std::cout << "  --smoke         Run built-in smoke test\n";
            std::cout << "  --batch         Run batch test execution example\n";
            std::cout << "  --single        Run single action execution example\n";
            std::cout << "  --help          Show this help\n";
        }
    } else {
        // Run all examples
        Example_ProgrammaticTest();
        Example_YamlTest();
        Example_SingleAction();
        Example_SmokeTest();
    }
    
    return 0;
}
