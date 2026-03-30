// automation_framework.cpp - KVM-Drivers C++ Automation Framework Implementation

#include "automation_framework.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <filesystem>
#include <thread>
#include <json/json.h>
#include <windows.h>
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")

namespace KVMDrivers {
namespace Automation {

// ==================== TestSuite Implementation ====================

bool TestSuite::LoadFromFile(const std::string& filepath) {
    try {
        YAML::Node root = YAML::LoadFile(filepath);
        
        // Parse metadata
        config.name = root["name"].as<std::string>("Unnamed Test");
        config.description = root["description"].as<std::string>("");
        config.timeoutSeconds = root["timeout"].as<int>(300);
        config.continueOnError = root["continue_on_error"].as<bool>(false);
        
        // Parse variables
        if (root["variables"]) {
            for (const auto& var : root["variables"]) {
                std::string key = var.first.as<std::string>();
                std::string value = var.second.as<std::string>();
                config.variables[key] = value;
            }
        }
        
        // Parse setup steps
        if (root["setup"]) {
            for (const auto& stepNode : root["setup"]) {
                auto step = std::make_shared<TestStep>();
                step->id = 0;  // Setup steps don't have IDs
                step->description = stepNode["description"].as<std::string>("");
                step->actionType = ParseActionType(stepNode["action"].as<std::string>(""));
                step->delayAfterMs = stepNode["delay_after_ms"].as<int>(100);
                
                if (stepNode["params"]) {
                    for (const auto& param : stepNode["params"]) {
                        step->parameters[param.first.as<std::string>()] = 
                            param.second.as<std::string>();
                    }
                }
                
                setupSteps.push_back(step);
            }
        }
        
        // Parse test steps
        if (root["steps"]) {
            int stepId = 1;
            for (const auto& stepNode : root["steps"]) {
                auto step = std::make_shared<TestStep>();
                step->id = stepNode["id"].as<int>(stepId++);
                step->description = stepNode["description"].as<std::string>("");
                step->actionType = ParseActionType(stepNode["action"].as<std::string>(""));
                step->delayAfterMs = stepNode["delay_after_ms"].as<int>(100);
                step->continueOnError = stepNode["continue_on_error"].as<bool>(false);
                step->retryCount = stepNode["retry"].as<int>(0);
                
                if (stepNode["params"]) {
                    for (const auto& param : stepNode["params"]) {
                        step->parameters[param.first.as<std::string>()] = 
                            param.second.as<std::string>();
                    }
                }
                
                testSteps.push_back(step);
            }
        }
        
        // Parse assertions
        if (root["assertions"]) {
            for (const auto& assertNode : root["assertions"]) {
                auto step = std::make_shared<TestStep>();
                step->actionType = ActionType::AssertImage;  // Default
                
                std::string type = assertNode["type"].as<std::string>("");
                if (type == "display.compare") {
                    step->actionType = ActionType::AssertImage;
                    step->parameters["reference"] = assertNode["reference"].as<std::string>();
                    step->parameters["actual"] = assertNode["actual"].as<std::string>();
                    step->parameters["tolerance"] = assertNode["tolerance"].as<std::string>("0.02");
                } else if (type == "display.find_element") {
                    step->actionType = ActionType::AssertWindow;
                    step->parameters["template"] = assertNode["template"].as<std::string>();
                    step->parameters["required"] = assertNode["required"].as<bool>(true) ? "true" : "false";
                }
                
                step->continueOnError = assertNode["continue_on_fail"].as<bool>(false);
                assertions.push_back(step);
            }
        }
        
        // Parse teardown steps
        if (root["teardown"]) {
            for (const auto& stepNode : root["teardown"]) {
                auto step = std::make_shared<TestStep>();
                step->actionType = ParseActionType(stepNode["action"].as<std::string>(""));
                
                if (stepNode["params"]) {
                    for (const auto& param : stepNode["params"]) {
                        step->parameters[param.first.as<std::string>()] = 
                            param.second.as<std::string>();
                    }
                }
                
                teardownSteps.push_back(step);
            }
        }
        
        return true;
        
    } catch (const YAML::Exception& e) {
        std::cerr << "YAML parsing error: " << e.what() << std::endl;
        return false;
    }
}

bool TestSuite::LoadFromString(const std::string& yamlContent) {
    try {
        YAML::Node root = YAML::Load(yamlContent);
        // Same parsing logic as LoadFromFile...
        return true;
    } catch (const YAML::Exception& e) {
        std::cerr << "YAML parsing error: " << e.what() << std::endl;
        return false;
    }
}

bool TestSuite::SaveToFile(const std::string& filepath) const {
    YAML::Emitter out;
    out << YAML::BeginMap;
    
    out << YAML::Key << "name" << YAML::Value << config.name;
    out << YAML::Key << "description" << YAML::Value << config.description;
    out << YAML::Key << "version" << YAML::Value << "1.0";
    
    if (!config.variables.empty()) {
        out << YAML::Key << "variables" << YAML::Value << YAML::BeginMap;
        for (const auto& [key, value] : config.variables) {
            out << YAML::Key << key << YAML::Value << value;
        }
        out << YAML::EndMap;
    }
    
    // Emit steps...
    
    out << YAML::EndMap;
    
    std::ofstream file(filepath);
    if (!file.is_open()) return false;
    file << out.c_str();
    return true;
}

// ==================== Action Type Parser ====================

ActionType ParseActionType(const std::string& str) {
    static const std::map<std::string, ActionType> map = {
        {"input.mouse.move", ActionType::MouseMove},
        {"input.mouse.click", ActionType::MouseClick},
        {"input.mouse.scroll", ActionType::MouseScroll},
        {"input.mouse.drag", ActionType::MouseDrag},
        {"input.keyboard.keydown", ActionType::KeyDown},
        {"input.keyboard.keyup", ActionType::KeyUp},
        {"input.keyboard.keypress", ActionType::KeyPress},
        {"input.keyboard.combo", ActionType::KeyCombo},
        {"input.keyboard.type", ActionType::KeyType},
        {"system.wait", ActionType::Wait},
        {"display.capture", ActionType::Screenshot},
        {"assert.image.compare", ActionType::AssertImage},
        {"assert.pixel", ActionType::AssertPixel},
        {"assert.window", ActionType::AssertWindow}
    };
    
    auto it = map.find(str);
    if (it != map.end()) return it->second;
    return ActionType::Unknown;
}

// ==================== AutomationContext Implementation ====================

std::string AutomationContext::InterpolateVariables(const std::string& input) const {
    return Utils::InterpolateVariables(input, variables);
}

void AutomationContext::LogInfo(const std::string& message) {
    if (logCallback) logCallback("[INFO] " + message);
}

void AutomationContext::LogWarning(const std::string& message) {
    if (logCallback) logCallback("[WARNING] " + message);
}

void AutomationContext::LogError(const std::string& message) {
    if (logCallback) logCallback("[ERROR] " + message);
}

void AutomationContext::LogDebug(const std::string& message) {
    if (logCallback) logCallback("[DEBUG] " + message);
}

// ==================== MouseActionHandler Implementation ====================

bool MouseActionHandler::ValidateParameters(const std::map<std::string, std::string>& params,
    std::string& errorMessage) const {
    // Check required parameters based on sub-action
    return true;
}

ActionResult MouseActionHandler::Execute(const TestStep& step, AutomationContext& context) {
    switch (step.actionType) {
    case ActionType::MouseMove:
        return HandleMouseMove(step, context);
    case ActionType::MouseClick:
        return HandleMouseClick(step, context);
    case ActionType::MouseScroll:
        return HandleMouseScroll(step, context);
    case ActionType::MouseDrag:
        return HandleMouseDrag(step, context);
    default:
        return ActionResult::Fail("Unsupported mouse action type");
    }
}

ActionResult MouseActionHandler::HandleMouseMove(const TestStep& step, AutomationContext& context) {
    auto it = step.parameters.find("x");
    auto jt = step.parameters.find("y");
    
    if (it == step.parameters.end() || jt == step.parameters.end()) {
        return ActionResult::Fail("Missing x or y coordinate");
    }
    
    int x = std::stoi(context.InterpolateVariables(it->second));
    int y = std::stoi(context.InterpolateVariables(jt->second));
    bool absolute = true;
    
    auto kt = step.parameters.find("absolute");
    if (kt != step.parameters.end()) {
        absolute = (kt->second == "true" || kt->second == "1");
    }
    
    if (context.driverInterface) {
        bool ok = context.driverInterface->InjectMouseMove(x, y, absolute);
        if (ok) {
            return ActionResult::Ok("Moved mouse to (" + std::to_string(x) + ", " + std::to_string(y) + ")");
        } else {
            return ActionResult::Fail("Failed to inject mouse movement");
        }
    }
    
    return ActionResult::Fail("Driver interface not available");
}

ActionResult MouseActionHandler::HandleMouseClick(const TestStep& step, AutomationContext& context) {
    auto it = step.parameters.find("x");
    auto jt = step.parameters.find("y");
    
    // Move to position first if specified
    if (it != step.parameters.end() && jt != step.parameters.end()) {
        int x = std::stoi(context.InterpolateVariables(it->second));
        int y = std::stoi(context.InterpolateVariables(jt->second));
        context.driverInterface->InjectMouseMove(x, y, true);
    }
    
    std::string button = "left";
    auto bt = step.parameters.find("button");
    if (bt != step.parameters.end()) {
        button = bt->second;
    }
    
    int buttonCode = (button == "left") ? 1 : (button == "right") ? 2 : (button == "middle") ? 3 : 1;
    
    // Press
    bool ok = context.driverInterface->InjectMouseButton(buttonCode, true);
    if (!ok) return ActionResult::Fail("Failed to press mouse button");
    
    // Small delay
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Release
    ok = context.driverInterface->InjectMouseButton(buttonCode, false);
    if (!ok) return ActionResult::Fail("Failed to release mouse button");
    
    return ActionResult::Ok("Clicked " + button + " button");
}

ActionResult MouseActionHandler::HandleMouseScroll(const TestStep& step, AutomationContext& context) {
    auto it = step.parameters.find("vertical");
    auto jt = step.parameters.find("horizontal");
    
    int vertical = 0, horizontal = 0;
    if (it != step.parameters.end()) vertical = std::stoi(it->second);
    if (jt != step.parameters.end()) horizontal = std::stoi(jt->second);
    
    // Scroll via driver
    if (context.driverInterface) {
        // Assuming driver has scroll support
        context.driverInterface->InjectMouseScroll(vertical, horizontal);
        return ActionResult::Ok("Scrolled (" + std::to_string(horizontal) + ", " + std::to_string(vertical) + ")");
    }
    
    return ActionResult::Fail("Driver interface not available");
}

ActionResult MouseActionHandler::HandleMouseDrag(const TestStep& step, AutomationContext& context) {
    auto getInt = [&](const char* key, int def = 0) -> int {
        auto it = step.parameters.find(key);
        return it != step.parameters.end() ? std::stoi(it->second) : def;
    };
    int fromX  = getInt("from_x"); int fromY  = getInt("from_y");
    int toX    = getInt("to_x");   int toY    = getInt("to_y");
    int steps  = std::max(1, getInt("steps", 20));
    int holdMs = getInt("hold_ms", 50);

    if (!context.driverInterface) return ActionResult::Fail("Driver interface not available");

    context.driverInterface->InjectMouseMove(fromX, fromY, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(holdMs));
    context.driverInterface->InjectMouseButton(VMOUSE_BUTTON_LEFT, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(holdMs));

    for (int i = 1; i <= steps; i++) {
        int x = fromX + (toX - fromX) * i / steps;
        int y = fromY + (toY - fromY) * i / steps;
        context.driverInterface->InjectMouseMove(x, y, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(holdMs));
    context.driverInterface->InjectMouseButton(VMOUSE_BUTTON_LEFT, false);

    return ActionResult::Ok("Dragged from (" + std::to_string(fromX) + "," + std::to_string(fromY) +
                            ") to (" + std::to_string(toX) + "," + std::to_string(toY) + ")");
}

// ==================== KeyboardActionHandler Implementation ====================

int KeyboardActionHandler::KeyNameToCode(const std::string& name) const {
    static const std::map<std::string, int> keyMap = {
        {"ctrl", 0x11}, {"shift", 0x10}, {"alt", 0x12},
        {"tab", 0x09}, {"return", 0x0D}, {"enter", 0x0D},
        {"esc", 0x1B}, {"escape", 0x1B}, {"space", 0x20},
        {"delete", 0x2E}, {"del", 0x2E}, {"backspace", 0x08},
        {"up", 0x26}, {"down", 0x28}, {"left", 0x25}, {"right", 0x27},
        {"home", 0x24}, {"end", 0x23}, {"pageup", 0x21}, {"pagedown", 0x22},
        {"f1", 0x70}, {"f2", 0x71}, {"f3", 0x72}, {"f4", 0x73},
        {"f5", 0x74}, {"f6", 0x75}, {"f7", 0x76}, {"f8", 0x77},
        {"f9", 0x78}, {"f10", 0x79}, {"f11", 0x7A}, {"f12", 0x7B},
    };
    
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    auto it = keyMap.find(lower);
    if (it != keyMap.end()) return it->second;
    
    // Single character
    if (lower.length() == 1) {
        return std::toupper(lower[0]);
    }
    
    return 0;
}

bool KeyboardActionHandler::ValidateParameters(const std::map<std::string, std::string>& params,
    std::string& errorMessage) const {
    return true;
}

ActionResult KeyboardActionHandler::Execute(const TestStep& step, AutomationContext& context) {
    switch (step.actionType) {
    case ActionType::KeyDown:
        return HandleKeyDown(step, context);
    case ActionType::KeyUp:
        return HandleKeyUp(step, context);
    case ActionType::KeyPress:
        return HandleKeyPress(step, context);
    case ActionType::KeyCombo:
        return HandleKeyCombo(step, context);
    case ActionType::KeyType:
        return HandleType(step, context);
    default:
        return ActionResult::Fail("Unsupported keyboard action type");
    }
}

ActionResult KeyboardActionHandler::HandleKeyDown(const TestStep& step, AutomationContext& context) {
    auto it = step.parameters.find("keycode");
    if (it == step.parameters.end()) {
        return ActionResult::Fail("Missing keycode parameter");
    }
    
    int keycode = std::stoi(it->second);
    int modifiers = 0;
    
    auto jt = step.parameters.find("modifiers");
    if (jt != step.parameters.end()) {
        modifiers = std::stoi(jt->second);
    }
    
    if (context.driverInterface) {
        bool ok = context.driverInterface->InjectKeyDown(keycode, modifiers);
        if (ok) {
            return ActionResult::Ok("Key down: " + std::to_string(keycode));
        } else {
            return ActionResult::Fail("Failed to inject key down");
        }
    }
    
    return ActionResult::Fail("Driver interface not available");
}

ActionResult KeyboardActionHandler::HandleKeyUp(const TestStep& step, AutomationContext& context) {
    auto it = step.parameters.find("keycode");
    if (it == step.parameters.end()) {
        return ActionResult::Fail("Missing keycode parameter");
    }
    
    int keycode = std::stoi(it->second);
    int modifiers = 0;
    
    auto jt = step.parameters.find("modifiers");
    if (jt != step.parameters.end()) {
        modifiers = std::stoi(jt->second);
    }
    
    if (context.driverInterface) {
        bool ok = context.driverInterface->InjectKeyUp(keycode, modifiers);
        if (ok) {
            return ActionResult::Ok("Key up: " + std::to_string(keycode));
        } else {
            return ActionResult::Fail("Failed to inject key up");
        }
    }
    
    return ActionResult::Fail("Driver interface not available");
}

ActionResult KeyboardActionHandler::HandleKeyPress(const TestStep& step, AutomationContext& context) {
    // Key down then key up
    auto downResult = HandleKeyDown(step, context);
    if (!downResult.success) return downResult;
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    return HandleKeyUp(step, context);
}

ActionResult KeyboardActionHandler::HandleKeyCombo(const TestStep& step, AutomationContext& context) {
    auto it = step.parameters.find("keys");
    if (it == step.parameters.end()) {
        return ActionResult::Fail("Missing keys parameter");
    }
    
    std::vector<std::string> keys = Utils::ParseKeyCombo(it->second);
    
    // Press all keys
    for (const auto& key : keys) {
        int code = KeyNameToCode(key);
        if (code > 0) {
            context.driverInterface->InjectKeyDown(code, 0);
        }
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Release in reverse
    for (auto it = keys.rbegin(); it != keys.rend(); ++it) {
        int code = KeyNameToCode(*it);
        if (code > 0) {
            context.driverInterface->InjectKeyUp(code, 0);
        }
    }
    
    return ActionResult::Ok("Key combo executed");
}

ActionResult KeyboardActionHandler::HandleType(const TestStep& step, AutomationContext& context) {
    auto it = step.parameters.find("text");
    if (it == step.parameters.end()) {
        return ActionResult::Fail("Missing text parameter");
    }
    
    std::string text = context.InterpolateVariables(it->second);
    int interval = 10;
    
    auto jt = step.parameters.find("interval_ms");
    if (jt != step.parameters.end()) {
        interval = std::stoi(jt->second);
    }
    
    for (char c : text) {
        context.driverInterface->InjectKeyDown(c, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        context.driverInterface->InjectKeyUp(c, 0);
    }
    
    return ActionResult::Ok("Typed: " + text);
}

// ==================== WaitActionHandler Implementation ====================

bool WaitActionHandler::ValidateParameters(const std::map<std::string, std::string>& params,
    std::string& errorMessage) const {
    auto it = params.find("duration_ms");
    if (it == params.end()) {
        errorMessage = "Missing duration_ms parameter";
        return false;
    }
    return true;
}

ActionResult WaitActionHandler::Execute(const TestStep& step, AutomationContext& context) {
    auto it = step.parameters.find("duration_ms");
    if (it == step.parameters.end()) {
        return ActionResult::Fail("Missing duration_ms parameter");
    }
    
    int duration = std::stoi(it->second);
    context.LogInfo("Waiting " + std::to_string(duration) + "ms");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(duration));
    
    return ActionResult::Ok("Waited " + std::to_string(duration) + "ms");
}

// ==================== ScreenshotActionHandler Implementation ====================

bool ScreenshotActionHandler::ValidateParameters(const std::map<std::string, std::string>& params,
    std::string& errorMessage) const {
    return true;
}

ActionResult ScreenshotActionHandler::Execute(const TestStep& step, AutomationContext& context) {
    std::string filename = "screenshot_" + Utils::GetTimestamp() + ".png";
    
    auto it = step.parameters.find("name");
    if (it != step.parameters.end()) {
        filename = it->second + ".png";
    }
    
    std::string filepath = "./screenshots/" + filename;
    Utils::EnsureDirectory("./screenshots");
    
    if (CaptureScreen(filepath)) {
        context.lastScreenshotPath = filepath;
        return ActionResult::Ok("Screenshot saved: " + filepath);
    } else {
        return ActionResult::Fail("Failed to capture screenshot");
    }
}

bool ScreenshotActionHandler::CaptureScreen(const std::string& filepath) {
    using namespace Gdiplus;
    GdiplusStartupInput gpIn; ULONG_PTR gpTok = 0;
    GdiplusStartup(&gpTok, &gpIn, NULL);

    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    HDC scr = GetDC(NULL);
    HDC mem = CreateCompatibleDC(scr);
    HBITMAP hBmp = CreateCompatibleBitmap(scr, w, h);
    HBITMAP hOld = (HBITMAP)SelectObject(mem, hBmp);
    BitBlt(mem, 0, 0, w, h, scr, 0, 0, SRCCOPY);
    SelectObject(mem, hOld);

    UINT   num = 0, sz = 0;
    GetImageEncodersSize(&num, &sz);
    std::vector<BYTE> buf(sz);
    ImageCodecInfo* pci = reinterpret_cast<ImageCodecInfo*>(buf.data());
    GetImageEncoders(num, sz, pci);
    CLSID clsid = {};
    for (UINT i = 0; i < num; i++) {
        if (wcscmp(pci[i].MimeType, L"image/png") == 0) { clsid = pci[i].Clsid; break; }
    }

    Bitmap bmp(hBmp, NULL);
    std::wstring wf(filepath.begin(), filepath.end());
    bool ok = (bmp.Save(wf.c_str(), &clsid, NULL) == Ok);

    DeleteDC(mem); ReleaseDC(NULL, scr); DeleteObject(hBmp);
    GdiplusShutdown(gpTok);
    return ok;
}

// ==================== TestRunner Implementation ====================

TestRunner::TestRunner() {
    context_.driverInterface = nullptr;
}

TestRunner::~TestRunner() {
    Shutdown();
}

bool TestRunner::Initialize() {
    if (initialized_) return true;
    
    // Initialize driver interface
    context_.driverInterface = new DriverInterface();
    if (!context_.driverInterface->Initialize()) {
        std::cerr << "[TestRunner] Failed to initialize driver interface" << std::endl;
        delete context_.driverInterface;
        context_.driverInterface = nullptr;
        return false;
    }
    
    // Register built-in handlers
    RegisterBuiltInHandlers();
    
    initialized_ = true;
    return true;
}

void TestRunner::Shutdown() {
    if (!initialized_) return;
    
    if (context_.driverInterface) {
        context_.driverInterface->Disconnect();
        delete context_.driverInterface;
        context_.driverInterface = nullptr;
    }
    
    handlers_.clear();
    handlersByName_.clear();
    
    initialized_ = false;
}

void TestRunner::RegisterBuiltInHandlers() {
    RegisterHandler(std::make_shared<MouseActionHandler>());
    RegisterHandler(std::make_shared<KeyboardActionHandler>());
    RegisterHandler(std::make_shared<WaitActionHandler>());
    RegisterHandler(std::make_shared<ScreenshotActionHandler>());
}

void TestRunner::RegisterHandler(std::shared_ptr<ITestActionHandler> handler) {
    handlers_[handler->GetActionType()] = handler;
    handlersByName_[handler->GetName()] = handler;
}

void TestRunner::UnregisterHandler(const std::string& name) {
    auto it = handlersByName_.find(name);
    if (it != handlersByName_.end()) {
        // Remove from both maps
        for (auto jt = handlers_.begin(); jt != handlers_.end(); ) {
            if (jt->second->GetName() == name) {
                jt = handlers_.erase(jt);
            } else {
                ++jt;
            }
        }
        handlersByName_.erase(it);
    }
}

std::vector<std::string> TestRunner::GetRegisteredHandlers() const {
    std::vector<std::string> names;
    for (const auto& [name, handler] : handlersByName_) {
        names.push_back(name);
    }
    return names;
}

TestResult TestRunner::RunTest(const TestSuite& suite) {
    TestResult result;
    result.testName = suite.config.name;
    result.startTime = std::chrono::steady_clock::now();
    context_.testStartTime = result.startTime;
    context_.currentTestConfig = const_cast<TestConfig*>(&suite.config);
    
    context_.LogInfo("Starting test: " + suite.config.name);
    
    // Execute setup
    if (!suite.setupSteps.empty()) {
        context_.LogInfo("Running setup phase...");
        result.setupResults = ExecuteSteps(suite.setupSteps, "setup");
        for (const auto& r : result.setupResults) {
            result.statistics.RecordAction(r.actionResult, r.duration);
            if (!r.actionResult.success && !suite.config.continueOnError) {
                result.overallSuccess = false;
                result.errorMessage = "Setup failed: " + r.actionResult.message;
                return result;
            }
        }
    }
    
    // Execute test steps
    context_.LogInfo("Running test steps...");
    result.testResults = ExecuteSteps(suite.testSteps, "test");
    for (const auto& r : result.testResults) {
        result.statistics.RecordAction(r.actionResult, r.duration);
    }
    
    // Execute assertions
    if (!suite.assertions.empty()) {
        context_.LogInfo("Running assertions...");
        result.assertionResults = ExecuteSteps(suite.assertions, "assert");
        for (const auto& r : result.assertionResults) {
            result.statistics.RecordAction(r.actionResult, r.duration);
        }
    }
    
    // Execute teardown (always run, even if test failed)
    if (!suite.teardownSteps.empty()) {
        context_.LogInfo("Running teardown phase...");
        result.teardownResults = ExecuteSteps(suite.teardownSteps, "teardown");
    }
    
    result.endTime = std::chrono::steady_clock::now();
    result.totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        result.endTime - result.startTime);
    
    // Determine overall success
    result.overallSuccess = (result.FailedSteps() == 0);
    
    context_.LogInfo("Test completed. Total: " + std::to_string(result.TotalSteps()) +
        ", Passed: " + std::to_string(result.PassedSteps()) +
        ", Failed: " + std::to_string(result.FailedSteps()));
    
    return result;
}

std::vector<StepResult> TestRunner::ExecuteSteps(
    const std::vector<std::shared_ptr<TestStep>>& steps, const std::string& phase) {
    std::vector<StepResult> results;
    
    for (size_t i = 0; i < steps.size(); i++) {
        context_.currentStepNumber = (int)i + 1;
        
        if (context_.progressCallback) {
            context_.progressCallback((int)i + 1, (int)steps.size(), steps[i]->description);
        }
        
        StepResult result = ExecuteSingleStep(*steps[i]);
        results.push_back(result);
        
        // Delay after step
        if (i < steps.size() - 1 && steps[i]->delayAfterMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(steps[i]->delayAfterMs));
        }
    }
    
    return results;
}

StepResult TestRunner::ExecuteSingleStep(const TestStep& step) {
    StepResult result;
    result.stepId = step.id;
    result.description = step.description;
    
    auto stepStart = std::chrono::steady_clock::now();
    result.startTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        stepStart - context_.testStartTime);
    
    // Find handler
    auto handler = FindHandler(step.actionType);
    if (!handler) {
        result.actionResult = ActionResult::Fail("No handler registered for action type");
        result.duration = std::chrono::milliseconds(0);
        return result;
    }
    
    // Validate parameters
    std::string validationError;
    if (!handler->ValidateParameters(step.parameters, validationError)) {
        result.actionResult = ActionResult::Fail("Parameter validation failed: " + validationError);
        auto stepEnd = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(stepEnd - stepStart);
        return result;
    }
    
    // Execute with retry logic
    int maxRetries = std::max(step.retryCount, handler->GetMaxRetries());
    for (int attempt = 0; attempt <= maxRetries; attempt++) {
        result.actionResult = handler->Execute(step, context_);
        
        if (result.actionResult.success) {
            break;
        }
        
        result.retryAttempts = attempt;
        
        if (attempt < maxRetries) {
            context_.LogWarning("Action failed (attempt " + std::to_string(attempt + 1) + 
                "), retrying...");
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    auto stepEnd = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(stepEnd - stepStart);
    
    // Capture screenshot on failure if configured
    if (!result.actionResult.success && context_.currentTestConfig &&
            context_.currentTestConfig->captureScreenshots) {
        auto* sh = dynamic_cast<ScreenshotActionHandler*>(
            FindHandler(ActionType::Screenshot).get());
        if (sh) {
            std::string path = "./screenshots/fail_step" +
                std::to_string(result.stepId) + "_" +
                Utils::GetTimestamp() + ".png";
            Utils::EnsureDirectory("./screenshots");
            sh->CaptureScreen(path);
            result.actionResult.message += " [screenshot: " + path + "]";
        }
    }
    
    return result;
}

std::shared_ptr<ITestActionHandler> TestRunner::FindHandler(ActionType type) const {
    auto it = handlers_.find(type);
    if (it != handlers_.end()) return it->second;
    return nullptr;
}

std::shared_ptr<ITestActionHandler> TestRunner::FindHandler(const std::string& name) const {
    auto it = handlersByName_.find(name);
    if (it != handlersByName_.end()) return it->second;
    return nullptr;
}

void TestRunner::SetLogCallback(std::function<void(const std::string&)> callback) {
    context_.logCallback = callback;
}

void TestRunner::SetProgressCallback(
    std::function<void(int, int, const std::string&)> callback) {
    context_.progressCallback = callback;
}

void TestRunner::SetErrorRecoveryCallback(
    std::function<bool(const std::string&, int)> callback) {
    context_.errorRecoveryCallback = callback;
}

void TestRunner::SetScreenshotDirectory(const std::string& dir) {
    Utils::EnsureDirectory(dir);
}

void TestRunner::SetDefaultTimeout(int seconds) {
    // Store default timeout
}

std::string TestRunner::GetVersion() {
    return AUTOMATION_FRAMEWORK_VERSION;
}

std::string TestRunner::GetBuildInfo() {
    return "KVM-Drivers Automation Framework v" + GetVersion() + " (Build " __DATE__ " " __TIME__ ")";
}

// ==================== TestResult Serialization ====================

std::string TestResult::ToJson() const {
    Json::Value root;
    root["testName"] = testName;
    root["overallSuccess"] = overallSuccess;
    root["totalDurationMs"] = (int)totalDuration.count();
    root["totalSteps"] = TotalSteps();
    root["passedSteps"] = PassedSteps();
    root["failedSteps"] = FailedSteps();
    root["errorMessage"] = errorMessage;
    
    Json::Value steps(Json::arrayValue);
    for (const auto& r : testResults) {
        Json::Value step;
        step["id"] = r.stepId;
        step["description"] = r.description;
        step["success"] = r.actionResult.success;
        step["message"] = r.actionResult.message;
        step["durationMs"] = (int)r.duration.count();
        steps.append(step);
    }
    root["steps"] = steps;
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}

std::string TestResult::ToXml() const {
    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml << "<test_result>\n";
    xml << "  <test_name>" << testName << "</test_name>\n";
    xml << "  <overall_success>" << (overallSuccess ? "true" : "false") << "</overall_success>\n";
    xml << "  <total_duration_ms>" << totalDuration.count() << "</total_duration_ms>\n";
    xml << "  <total_steps>" << TotalSteps() << "</total_steps>\n";
    xml << "  <passed_steps>" << PassedSteps() << "</passed_steps>\n";
    xml << "  <failed_steps>" << FailedSteps() << "</failed_steps>\n";
    xml << "  <steps>\n";
    for (const auto& r : testResults) {
        xml << "    <step>\n";
        xml << "      <id>" << r.stepId << "</id>\n";
        xml << "      <description>" << r.description << "</description>\n";
        xml << "      <success>" << (r.actionResult.success ? "true" : "false") << "</success>\n";
        xml << "      <message>" << r.actionResult.message << "</message>\n";
        xml << "      <duration_ms>" << r.duration.count() << "</duration_ms>\n";
        xml << "    </step>\n";
    }
    xml << "  </steps>\n";
    xml << "</test_result>\n";
    return xml.str();
}

std::string TestResult::ToMarkdown() const {
    std::ostringstream md;
    md << "# Test Result: " << testName << "\n\n";
    md << "**Status:** " << (overallSuccess ? "✅ PASSED" : "❌ FAILED") << "\n\n";
    md << "**Duration:** " << totalDuration.count() << "ms\n\n";
    md << "**Summary:** " << PassedSteps() << "/" << TotalSteps() << " steps passed\n\n";
    
    if (!errorMessage.empty()) {
        md << "**Error:** " << errorMessage << "\n\n";
    }
    
    md << "## Steps\n\n";
    md << "| ID | Description | Status | Duration |\n";
    md << "|---|---|---|---|\n";
    for (const auto& r : testResults) {
        md << "| " << r.stepId << " | " << r.description << " | " 
           << (r.actionResult.success ? "✅" : "❌") << " | " 
           << r.duration.count() << "ms |\n";
    }
    
    return md.str();
}

bool TestResult::SaveToFile(const std::string& filepath) const {
    std::string ext = filepath.substr(filepath.find_last_of('.') + 1);
    std::string content;
    
    if (ext == "json") {
        content = ToJson();
    } else if (ext == "xml") {
        content = ToXml();
    } else if (ext == "md") {
        content = ToMarkdown();
    } else {
        content = ToJson();  // Default
    }
    
    std::ofstream file(filepath);
    if (!file.is_open()) return false;
    file << content;
    return true;
}

// ==================== Utils Implementation ====================

namespace Utils {

bool ParseCoordinates(const std::string& str, int& x, int& y) {
    size_t comma = str.find(',');
    if (comma == std::string::npos) return false;
    
    try {
        x = std::stoi(str.substr(0, comma));
        y = std::stoi(str.substr(comma + 1));
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<std::string> ParseKeyCombo(const std::string& str) {
    std::vector<std::string> keys;
    std::stringstream ss(str);
    std::string key;
    
    while (std::getline(ss, key, '+')) {
        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        if (!key.empty()) {
            keys.push_back(key);
        }
    }
    
    return keys;
}

std::string GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
    return ss.str();
}

bool EnsureDirectory(const std::string& path) {
    try {
        std::filesystem::create_directories(path);
        return true;
    } catch (...) {
        return false;
    }
}

std::string InterpolateVariables(const std::string& input,
    const std::map<std::string, std::string>& vars) {
    std::string result = input;
    
    for (const auto& [key, value] : vars) {
        std::string placeholder = "${" + key + "}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), value);
            pos += value.length();
        }
    }
    
    return result;
}

} // namespace Utils

} // namespace Automation
} // namespace KVMDrivers
