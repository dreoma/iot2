
#pragma once
#include <atomic>
#include <mutex>
#include <thread>
#include <tgbot/CurlHttpClient.h>
#include <tgbot/tgbot.h>
#include "INotifier.h"

class UserRepository;

class TelegramNotifier : public INotifier {
public:
    TelegramNotifier(std::string token, std::string apiUrl);
    ~TelegramNotifier() override;

    void start(std::shared_ptr<UserRepository> users);
    void stop();
    std::string username() const;
    bool send(const Notification& n) override;
    const char* name() const override { return "telegram"; }

private:
    void runLongPoll();
    void handleStart(const TgBot::Message::Ptr& message);

    TgBot::CurlHttpClient httpClient_;
    TgBot::Bot bot_;
    std::shared_ptr<UserRepository> users_;
    mutable std::mutex usernameMutex_;
    std::string username_;
    std::atomic<bool> running_{false};
    std::thread pollThread_;
};
