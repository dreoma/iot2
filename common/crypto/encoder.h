#pragma once

#include <string>

// AES-256-GCM: шифрует данные и добавляет тег, защищающий их от подмены.
class Encoder
{
  public:
    explicit Encoder(const std::string &keyHex);
    std::string encrypt(const std::string &plainText) const;

  private:
    std::string key_;
};
