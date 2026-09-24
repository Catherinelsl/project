#include "application_paths.h"

#include <filesystem>

namespace fs = std::filesystem;

#ifdef _WIN32
#include <windows.h>

std::string getExecutableDirectory()
{
    wchar_t buffer[MAX_PATH]{};
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    fs::path path(buffer);
    return path.parent_path().string();
}
#elif __linux__
#include <unistd.h>

std::string getExecutableDirectory()
{
    char buffer[1024]{};
    auto length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if(length <= 0)
    {
        return "";
    }

    fs::path path(buffer);
    return path.parent_path().string();
}
#elif __APPLE__
#include <mach-o/dyld.h>

std::string getExecutableDirectory()
{
    char buffer[1024]{};
    uint32_t size = sizeof(buffer);
    _NSGetExecutablePath(buffer, &size);
    fs::path path(buffer);
    return path.parent_path().string();
}
#else
std::string getExecutableDirectory()
{
    return "";
}
#endif