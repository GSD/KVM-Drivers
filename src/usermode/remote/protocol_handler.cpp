// protocol_handler.cpp - JSON-RPC protocol dispatcher
#include <windows.h>
#include <string>
#include <map>
#include <functional>
#include <json/json.h> // Requires jsoncpp library

#pragma comment(lib, "jsoncpp.lib")

// Method handlers
typedef std::function<Json::Value(const Json::Value&)> RpcMethod;

class ProtocolHandler {
public:
    ProtocolHandler() {
        RegisterMethods();
    }

    std::string ProcessRequest(const std::string& jsonRequest) {
        Json::Value request;
        Json::CharReaderBuilder builder;
        std::string errors;

        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        if (!reader->parse(jsonRequest.c_str(), 
                           jsonRequest.c_str() + jsonRequest.length(), 
                           &request, &errors)) {
            return CreateErrorResponse(Json::nullValue, -32700, "Parse error: " + errors);
        }

        // Validate request
        if (!request.isObject() || !request.isMember("jsonrpc") || 
            request["jsonrpc"].asString() != "2.0") {
            return CreateErrorResponse(request.get("id", Json::nullValue), 
                                      -32600, "Invalid Request");
        }

        std::string method = request.get("method", "").asString();
        Json::Value id = request.get("id", Json::nullValue);
        Json::Value params = request.get("params", Json::Value(Json::objectValue));

        // Find and execute method
        auto it = methods.find(method);
        if (it == methods.end()) {
            return CreateErrorResponse(id, -32601, "Method not found: " + method);
        }

        try {
            Json::Value result = it->second(params);
            return CreateSuccessResponse(id, result);
        } catch (const std::exception& e) {
            return CreateErrorResponse(id, -32603, std::string("Internal error: ") + e.what());
        }
    }

private:
    std::map<std::string, RpcMethod> methods;

    void RegisterMethods() {
        // Input methods
        methods["input.keyboard.inject"] = [this](const Json::Value& p) {
            return HandleKeyboardInject(p);
        };
        methods["input.mouse.move"] = [this](const Json::Value& p) {
            return HandleMouseMove(p);
        };
        methods["input.mouse.click"] = [this](const Json::Value& p) {
            return HandleMouseClick(p);
        };
        methods["input.controller.report"] = [this](const Json::Value& p) {
            return HandleControllerReport(p);
        };

        // Display methods
        methods["display.capture"] = [this](const Json::Value& p) {
            return HandleDisplayCapture(p);
        };
        methods["display.get_info"] = [this](const Json::Value& p) {
            return HandleDisplayInfo(p);
        };

        // System methods
        methods["system.get_version"] = [this](const Json::Value& p) {
            Json::Value result;
            result["version"] = "1.0.0";
            result["protocol"] = "2.0";
            return result;
        };
        methods["system.ping"] = [this](const Json::Value& p) {
            return Json::Value("pong");
        };
    }

    Json::Value HandleKeyboardInject(const Json::Value& params) {
        // TODO: Call driver_interface to inject keys
        Json::Value result;
        result["success"] = true;
        return result;
    }

    Json::Value HandleMouseMove(const Json::Value& params) {
        int x = params.get("x", 0).asInt();
        int y = params.get("y", 0).asInt();
        bool absolute = params.get("absolute", false).asBool();
        
        // TODO: Call driver_interface
        Json::Value result;
        result["success"] = true;
        result["x"] = x;
        result["y"] = y;
        return result;
    }

    Json::Value HandleMouseClick(const Json::Value& params) {
        int button = params.get("button", 0).asInt();
        bool pressed = params.get("pressed", true).asBool();
        
        // TODO: Call driver_interface
        Json::Value result;
        result["success"] = true;
        result["button"] = button;
        result["pressed"] = pressed;
        return result;
    }

    Json::Value HandleControllerReport(const Json::Value& params) {
        // TODO: Parse XUSB report and inject
        Json::Value result;
        result["success"] = true;
        return result;
    }

    Json::Value HandleDisplayCapture(const Json::Value& params) {
        // TODO: Capture frame from display driver
        Json::Value result;
        result["format"] = "jpeg";
        result["width"] = 1920;
        result["height"] = 1080;
        // result["data"] = base64_encoded_frame;
        return result;
    }

    Json::Value HandleDisplayInfo(const Json::Value& params) {
        Json::Value result;
        result["width"] = 1920;
        result["height"] = 1080;
        result["refresh_rate"] = 60;
        result["virtual"] = true;
        return result;
    }

    std::string CreateSuccessResponse(const Json::Value& id, const Json::Value& result) {
        Json::Value response;
        response["jsonrpc"] = "2.0";
        response["id"] = id;
        response["result"] = result;
        
        Json::StreamWriterBuilder writer;
        return Json::writeString(writer, response);
    }

    std::string CreateErrorResponse(const Json::Value& id, int code, const std::string& message) {
        Json::Value response;
        response["jsonrpc"] = "2.0";
        response["id"] = id;
        
        Json::Value error;
        error["code"] = code;
        error["message"] = message;
        response["error"] = error;
        
        Json::StreamWriterBuilder writer;
        return Json::writeString(writer, response);
    }
};

// C interface
extern "C" {
    __declspec(dllexport) void* ProtocolHandler_Create() {
        return new ProtocolHandler();
    }

    __declspec(dllexport) const char* ProtocolHandler_Process(void* handler, const char* request) {
        static std::string response;
        response = ((ProtocolHandler*)handler)->ProcessRequest(request);
        return response.c_str();
    }

    __declspec(dllexport) void ProtocolHandler_Destroy(void* handler) {
        delete (ProtocolHandler*)handler;
    }
}
