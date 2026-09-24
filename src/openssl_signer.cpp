#include "openssl_signer.h"

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <memory>
#include <stdexcept>

namespace
{
using BioPtr = std::unique_ptr<BIO, decltype(&BIO_free)>;
using EvpMdCtxPtr = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
using EvpPKeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;

std::string openSSLError()
{
    unsigned long code = ERR_get_error();
    if(code == 0)
    {
        return "OpenSSL operation failed";
    }

    char buffer[256] = {};
    ERR_error_string_n(code, buffer, sizeof(buffer));
    return buffer;
}

const EVP_MD* digestByName(const std::string& digestName)
{
    const EVP_MD* digest = EVP_get_digestbyname(digestName.c_str());
    if(digest == nullptr)
    {
        throw std::invalid_argument("Unsupported digest algorithm: " + digestName);
    }
    return digest;
}

BioPtr makeReadBio(const std::string& data)
{
    BIO* bio = BIO_new_mem_buf(data.data(), static_cast<int>(data.size()));
    if(bio == nullptr)
    {
        throw std::runtime_error(openSSLError());
    }
    return BioPtr(bio, BIO_free);
}

EvpPKeyPtr readPrivateKey(const std::string& privateKeyPem)
{
    BioPtr bio = makeReadBio(privateKeyPem);
    EVP_PKEY* key = PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr);
    if(key == nullptr)
    {
        throw std::runtime_error(openSSLError());
    }
    return EvpPKeyPtr(key, EVP_PKEY_free);
}

EvpPKeyPtr readPublicKey(const std::string& publicKeyPem)
{
    BioPtr bio = makeReadBio(publicKeyPem);
    EVP_PKEY* key = PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr);
    if(key == nullptr)
    {
        throw std::runtime_error(openSSLError());
    }
    return EvpPKeyPtr(key, EVP_PKEY_free);
}

std::string base64Encode(const std::vector<unsigned char>& data)
{
    BioPtr base64(BIO_new(BIO_f_base64()), BIO_free);
    BioPtr memory(BIO_new(BIO_s_mem()), BIO_free);
    if(base64 == nullptr || memory == nullptr)
    {
        throw std::runtime_error(openSSLError());
    }

    BIO_set_flags(base64.get(), BIO_FLAGS_BASE64_NO_NL);
    BIO_push(base64.get(), memory.get());
    if(BIO_write(base64.get(), data.data(), static_cast<int>(data.size())) <= 0 || BIO_flush(base64.get()) != 1)
    {
        throw std::runtime_error(openSSLError());
    }

    BUF_MEM* buffer = nullptr;
    BIO_get_mem_ptr(memory.get(), &buffer);
    return std::string(buffer->data, buffer->length);
}

std::vector<unsigned char> base64Decode(const std::string& data)
{
    BioPtr base64(BIO_new(BIO_f_base64()), BIO_free);
    BioPtr memory(BIO_new_mem_buf(data.data(), static_cast<int>(data.size())), BIO_free);
    if(base64 == nullptr || memory == nullptr)
    {
        throw std::runtime_error(openSSLError());
    }

    BIO_set_flags(base64.get(), BIO_FLAGS_BASE64_NO_NL);
    BIO_push(base64.get(), memory.get());

    std::vector<unsigned char> decoded(data.size());
    int length = BIO_read(base64.get(), decoded.data(), static_cast<int>(decoded.size()));
    if(length < 0)
    {
        throw std::runtime_error(openSSLError());
    }

    decoded.resize(static_cast<std::size_t>(length));
    return decoded;
}
}

std::vector<unsigned char> OpenSSLSigner::sign(
    const std::string& payload,
    const std::string& privateKeyPem,
    const std::string& digestName)
{
    EvpPKeyPtr privateKey = readPrivateKey(privateKeyPem);
    EvpMdCtxPtr context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if(context == nullptr)
    {
        throw std::runtime_error(openSSLError());
    }

    if(EVP_DigestSignInit(context.get(), nullptr, digestByName(digestName), nullptr, privateKey.get()) != 1 ||
       EVP_DigestSignUpdate(context.get(), payload.data(), payload.size()) != 1)
    {
        throw std::runtime_error(openSSLError());
    }

    std::size_t signatureLength = 0;
    if(EVP_DigestSignFinal(context.get(), nullptr, &signatureLength) != 1)
    {
        throw std::runtime_error(openSSLError());
    }

    std::vector<unsigned char> signature(signatureLength);
    if(EVP_DigestSignFinal(context.get(), signature.data(), &signatureLength) != 1)
    {
        throw std::runtime_error(openSSLError());
    }

    signature.resize(signatureLength);
    return signature;
}

bool OpenSSLSigner::verify(
    const std::string& payload,
    const std::vector<unsigned char>& signature,
    const std::string& publicKeyPem,
    const std::string& digestName)
{
    EvpPKeyPtr publicKey = readPublicKey(publicKeyPem);
    EvpMdCtxPtr context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if(context == nullptr)
    {
        throw std::runtime_error(openSSLError());
    }

    if(EVP_DigestVerifyInit(context.get(), nullptr, digestByName(digestName), nullptr, publicKey.get()) != 1 ||
       EVP_DigestVerifyUpdate(context.get(), payload.data(), payload.size()) != 1)
    {
        throw std::runtime_error(openSSLError());
    }

    int result = EVP_DigestVerifyFinal(context.get(), signature.data(), signature.size());
    if(result < 0)
    {
        throw std::runtime_error(openSSLError());
    }

    return result == 1;
}

std::string OpenSSLSigner::signBase64(
    const std::string& payload,
    const std::string& privateKeyPem,
    const std::string& digestName)
{
    return base64Encode(sign(payload, privateKeyPem, digestName));
}

bool OpenSSLSigner::verifyBase64(
    const std::string& payload,
    const std::string& signatureBase64,
    const std::string& publicKeyPem,
    const std::string& digestName)
{
    return verify(payload, base64Decode(signatureBase64), publicKeyPem, digestName);
}