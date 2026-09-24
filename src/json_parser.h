#pragma once

#include "message_schema.h"

#include <string>

class JsonParser
{
public:
    static UpdateStartRequest parseUpdateStartRequest(const std::string& payload);
    static std::string serializeUpdateStartResponse(const UpdateStartResponse& resp);

    static UpdateFinishRequest parseUpdateFinishRequest(const std::string& payload);
    static std::string serializeUpdateFinishResponse(const UpdateFinishResponse& resp);
};