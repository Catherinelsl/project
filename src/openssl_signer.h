#pragma once

#include <string>
#include <vector>

class OpenSSLSigner
{
public:
    static std::vector<unsigned char> sign(
        const std::string& payload,
        const std::string& privateKeyPem,
        const std::string& digestName = "SHA256");

    static bool verify(
        const std::string& payload,
        const std::vector<unsigned char>& signature,
        const std::string& publicKeyPem,
        const std::string& digestName = "SHA256");

    static std::string signBase64(
        const std::string& payload,
        const std::string& privateKeyPem,
        const std::string& digestName = "SHA256");

    static bool verifyBase64(
        const std::string& payload,
        const std::string& signatureBase64,
        const std::string& publicKeyPem,
        const std::string& digestName = "SHA256");
};