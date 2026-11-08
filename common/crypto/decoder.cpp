#include "decoder.h"

#include <openssl/evp.h>

#include <stdexcept>
#include <vector>

namespace
{
std::vector<unsigned char> hexToBytes(const std::string &hex)
{
    if (hex.size() != 64)
        throw std::runtime_error("TRAFFIC_KEY_HEX must contain 64 hex characters");

    std::vector<unsigned char> bytes(32);
    for (size_t i = 0; i < bytes.size(); ++i)
    {
        try
        {
            bytes[i] = static_cast<unsigned char>(std::stoul(hex.substr(i * 2, 2), nullptr, 16));
        }
        catch (...)
        {
            throw std::runtime_error("TRAFFIC_KEY_HEX is not hexadecimal");
        }
    }
    return bytes;
}

std::vector<unsigned char> fromBase64(const std::string &value)
{
    if (value.empty() || value.size() % 4 != 0)
        throw std::runtime_error("invalid encrypted payload");

    std::vector<unsigned char> result(value.size() / 4 * 3);
    const auto size = EVP_DecodeBlock(result.data(),
                                      reinterpret_cast<const unsigned char *>(value.data()),
                                      value.size());
    if (size < 0)
        throw std::runtime_error("invalid base64 payload");

    int padding = 0;
    if (value.back() == '=') ++padding;
    if (value[value.size() - 2] == '=') ++padding;
    result.resize(size - padding);
    return result;
}
}  // namespace

Decoder::Decoder(const std::string &keyHex) : key_(keyHex)
{
    hexToBytes(key_);
}

std::string Decoder::decrypt(const std::string &encrypted) const
{
    const auto key = hexToBytes(key_);
    const auto packet = fromBase64(encrypted);
    constexpr size_t nonceSize = 12;
    constexpr size_t tagSize = 16;
    if (packet.size() <= nonceSize + tagSize)
        throw std::runtime_error("encrypted payload is too short");

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        throw std::runtime_error("cannot create decryption context");

    const auto *nonce = packet.data();
    const auto *tag = packet.data() + nonceSize;
    const auto *cipherText = packet.data() + nonceSize + tagSize;
    const auto cipherTextSize = packet.size() - nonceSize - tagSize;
    std::vector<unsigned char> plainText(cipherTextSize);
    int plainSize = 0;
    int finalSize = 0;
    const bool initialized = EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
                             EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, nonceSize, nullptr) == 1 &&
                             EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce) == 1 &&
                             EVP_DecryptUpdate(ctx, plainText.data(), &plainSize, cipherText, cipherTextSize) == 1 &&
                             EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, tagSize, const_cast<unsigned char *>(tag)) == 1;
    const bool authenticated = initialized &&
                               EVP_DecryptFinal_ex(ctx, plainText.data() + plainSize, &finalSize) == 1;
    EVP_CIPHER_CTX_free(ctx);
    if (!authenticated)
        throw std::runtime_error("encrypted payload authentication failed");

    return std::string(reinterpret_cast<char *>(plainText.data()), plainSize + finalSize);
}
