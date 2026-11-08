
#include "TelegramNotifier.h"
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

TelegramNotifier::TelegramNotifier(std::string token) : bot_(std::move(token)) {
    bot_.getEvents().onCommand("start", [this](TgBot::Message::Ptr m) {
        bot_.getApi().sendMessage(m->chat->id,
            "Привет! Ваш chat ID: " + std::to_string(m->chat->id) +
            "\nУкажите его в поле telegram_id пользователя в IoT-платформе.");
    });
    bot_.getEvents().onCommand("help", [this](TgBot::Message::Ptr m) {
        bot_.getApi().sendMessage(m->chat->id,
            "Я присылаю уведомления об измерениях ваших устройств.\n"
            "/start — узнать свой chat ID");
    });
    pollThread_ = std::thread(&TelegramNotifier::runLongPoll, this);
}

TelegramNotifier::~TelegramNotifier() {
    running_ = false;
    if (pollThread_.joinable()) pollThread_.join();   // ждёт до ~10 с (таймаут long poll)
}

void TelegramNotifier::runLongPoll() {
    TgBot::TgLongPoll poll(bot_);
    while (running_) {
        try {
            poll.start();
        } catch (const std::exception& e) {
            std::cerr << "[telegram] polling error: " << e.what() << "\n";
            std::this_thread::sleep_for(std::chrono::seconds(3));
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
