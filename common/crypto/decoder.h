#pragma once

#include <string>

// AES-256-GCM: расшифровывает данные только при корректном теге целостности.
class Decoder
{
  public:
    explicit Decoder(const std::string &keyHex);
    std::string decrypt(const std::string &encrypted) const;

  private:
    std::string key_;
};
