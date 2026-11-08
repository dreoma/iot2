#include "encoder.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

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
        const auto part = hex.substr(i * 2, 2);
        try
        {
            bytes[i] = static_cast<unsigned char>(std::stoul(part, nullptr, 16));
        }
        catch (...)
        {
            throw std::runtime_error("TRAFFIC_KEY_HEX is not hexadecimal");
        }
    }
    return bytes;
}

std::string toBase64(const std::vector<unsigned char> &value)
{
    std::string result(4 * ((value.size() + 2) / 3), '\0');
    const auto size = EVP_EncodeBlock(reinterpret_cast<unsigned char *>(result.data()),
                                      value.data(), value.size());
    result.resize(size);
    return result;
}
}  // namespace

Encoder::Encoder(const std::string &keyHex) : key_(keyHex)
{
    hexToBytes(key_);  // проверяем ключ при запуске, а не при первом запросе
}

std::string Encoder::encrypt(const std::string &plainText) const
{
    const auto key = hexToBytes(key_);
    std::vector<unsigned char> nonce(12);
    if (RAND_bytes(nonce.data(), nonce.size()) != 1)
        throw std::runtime_error("cannot generate encryption nonce");

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        throw std::runtime_error("cannot create encryption context");

    std::vector<unsigned char> cipherText(plainText.size() + 16);
    std::vector<unsigned char> tag(16);
    int encryptedSize = 0;
    int finalSize = 0;
    const bool ok = EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
                    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, nonce.size(), nullptr) == 1 &&
                    EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce.data()) == 1 &&
                    EVP_EncryptUpdate(ctx, cipherText.data(), &encryptedSize,
                                      reinterpret_cast<const unsigned char *>(plainText.data()),
                                      plainText.size()) == 1 &&
                    EVP_EncryptFinal_ex(ctx, cipherText.data() + encryptedSize, &finalSize) == 1 &&
                    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, tag.size(), tag.data()) == 1;
    EVP_CIPHER_CTX_free(ctx);
    if (!ok)
        throw std::runtime_error("cannot encrypt payload");

    cipherText.resize(encryptedSize + finalSize);
    std::vector<unsigned char> packet;
    packet.reserve(nonce.size() + tag.size() + cipherText.size());
    packet.insert(packet.end(), nonce.begin(), nonce.end());
    packet.insert(packet.end(), tag.begin(), tag.end());
    packet.insert(packet.end(), cipherText.begin(), cipherText.end());
    return toBase64(packet);
}
