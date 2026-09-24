#include "key_loader.h"

#include <fstream>
#include <iostream>
#include <sstream>

std::string loadTextFile(const std::string& filePath)
{
    std::ifstream file(filePath);
    if(!file.is_open())
    {
        std::cerr << "failed to open file: " << filePath << std::endl;
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}