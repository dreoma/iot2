
#include "TelegramNotifier.h"
#include "repositories/user_repository.h"
#include "services/auth.h"
#include <chrono>
#include <iostream>
#include <sstream>

namespace {
std::string esc(const std::string& s) {      // экранирование для parse_mode=HTML
    std::string o;
    for (char c : s) {
        if (c == '&') o += "&amp;";
        else if (c == '<') o += "&lt;";
        else if (c == '>') o += "&gt;";
        else o += c;
    }
    return o;
}
}

TelegramNotifier::TelegramNotifier(std::string token, std::string apiUrl)
    : bot_(std::move(token), httpClient_, std::move(apiUrl)) {
    bot_.getEvents().onCommand("start", [this](TgBot::Message::Ptr m) {
        handleStart(m);
    });
    bot_.getEvents().onCommand("help", [this](TgBot::Message::Ptr m) {
        bot_.getApi().sendMessage(m->chat->id,
            "Я присылаю уведомления об измерениях ваших устройств.\n"
            "Для регистрации откройте ссылку на бота с сайта IoT Studio.");
    });
}

void TelegramNotifier::start(std::shared_ptr<UserRepository> users) {
    users_ = std::move(users);
    running_ = true;
    pollThread_ = std::thread(&TelegramNotifier::runLongPoll, this);
}

TelegramNotifier::~TelegramNotifier() {
    stop();
}

void TelegramNotifier::stop() {
    running_ = false;
    if (pollThread_.joinable()) pollThread_.join();
}

std::string TelegramNotifier::username() const {
    std::lock_guard lock(usernameMutex_);
    return username_;
}

void TelegramNotifier::handleStart(const TgBot::Message::Ptr& message) {
    if (!message || !message->chat || !message->from || message->from->isBot ||
        message->chat->type != TgBot::Chat::Type::Private || message->from->id <= 0 ||
        message->chat->id != message->from->id)
        return;
    const auto chatId = message->chat->id;
    std::istringstream command(message->text.value_or(""));
    std::string name, token, extra;
    command >> name >> token;
    if (!auth::isToken(token) || command >> extra) {
        bot_.getApi().sendMessage(chatId, "Для регистрации в IoT Studio откройте ссылку на бота с сайта.");
        return;
    }
    if (users_->confirmRegistration(auth::digest(token), chatId)) {
        bot_.getApi().sendMessage(chatId, "Telegram подтверждён. Вернитесь на сайт IoT Studio и задайте пароль.");
    } else {
        bot_.getApi().sendMessage(chatId,
            "Ссылка недействительна, истекла или этот Telegram уже связан с аккаунтом. Вернитесь на сайт IoT Studio.");
    }
}

void TelegramNotifier::runLongPoll() {
    TgBot::TgLongPoll poll(bot_);
    while (running_) {
        try {
            if (username().empty()) {
                const auto account = bot_.getApi().getMe();
                const auto name = account ? account->username.value_or("") : "";
                if (name.empty() || name.find_first_not_of(
                        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != std::string::npos)
                    throw std::runtime_error("bot username unavailable");
                std::lock_guard lock(usernameMutex_);
                username_ = name;
            }
            poll.start();
        } catch (const std::exception&) {
            std::cerr << "[telegram] polling or command handling failed\n";
            if (running_) std::this_thread::sleep_for(std::chrono::seconds(3));
        }
    }
}

bool TelegramNotifier::send(const Notification& n) {
    if (n.telegramChatId.empty()) return false;
    std::ostringstream t;
    t << "📡 <b>Сработало правило</b>\n" << esc(n.message) << "\n"
      << "Устройство: <b>" << esc(n.deviceName) << "</b> (ID " << n.deviceId << ")\n"
      << "Измерение: #" << n.telemetryId << "\n"
      << "Правило: #" << n.ruleId << ", " << esc(n.condition) << "\n"
      << "Поле: " << esc(n.field) << "\n"
      << "Изменение: <b>" << esc(n.previousValue) << " → " << esc(n.currentValue) << "</b>\n"
      << "Владелец: #" << n.userId << ", " << esc(n.email) << "\n"
      << "Время события: " << esc(n.time);
     try {
        TgBot::SendMessageArgs args;
        args.chatId = std::stoll(n.telegramChatId);
        args.text = t.str();
        args.parseMode = "HTML";
        bot_.getApi().sendMessage(args);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[telegram] send failed: " << e.what() << "\n";   // токен не логируем
        return false;
    }
}
