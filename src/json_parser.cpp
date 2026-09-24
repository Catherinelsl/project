#include "json_parser.h"
#include <nlohmann/json.hpp>
#include <stdexcept>

using json = nlohmann::json;

UpdateStartRequest JsonParser::parseUpdateStartRequest(const std::string& payload)
{
    try
    {
        auto j = json::parse(payload);
        UpdateStartRequest req;
        req.requestId = j.at("requestId").get<std::string>();
        req.version = j.at("version").get<std::string>();
        req.fileSize = j.at("fileSize").get<long long>();
        return req;
    }
    catch(const json::exception& e)
    {
        throw std::runtime_error(std::string("parse UpdateStartRequest failed: ") + e.what());
    }
}

std::string JsonParser::serializeUpdateStartResponse(const UpdateStartResponse& resp)
{
    json j;
    j["requestId"] = resp.requestId;
    j["errorCode"] = resp.errorCode;
    j["msg"] = resp.msg;
    return j.dump(4);
}

UpdateFinishRequest JsonParser::parseUpdateFinishRequest(const std::string& payload)
{
    try
    {
        auto j = json::parse(payload);
        UpdateFinishRequest req;
        req.requestId = j.at("requestId").get<std::string>();
        req.version = j.at("version").get<std::string>();
        req.success = j.at("success").get<bool>();
        req.errorCode = j.at("errorCode").get<int>();
        return req;
    }
    catch(const json::exception& e)
    {
        throw std::runtime_error(std::string("parse UpdateFinishRequest failed: ") + e.what());
    }
}

std::string JsonParser::serializeUpdateFinishResponse(const UpdateFinishResponse& resp)
{
    json j;
    j["requestId"] = resp.requestId;
    j["errorCode"] = resp.errorCode;
    j["msg"] = resp.msg;
    return j.dump(4);
}
