#pragma once
#include <string>

struct UpdateStartRequest
{
    std::string requestId;
    std::string version;
    long long fileSize;
};

struct UpdateStartResponse
{
    std::string requestId;
    int errorCode;
    std::string msg;
};

struct UpdateFinishRequest
{
    std::string requestId;
    std::string version;
    bool success;
    int errorCode;
};

struct UpdateFinishResponse
{
    std::string requestId;
    int errorCode;
    std::string msg;
};