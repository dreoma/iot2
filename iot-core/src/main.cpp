#include <drogon/drogon.h>
#include <curl/curl.h>
#include "notifications/TelegramNotifier.h"
#include "notifications/EmailNotifier.h"
#include "services/NotificationService.h"
#include "services/auth.h"
#include <cstdlib>
#include <iostream>
#include <string_view>

#include "routes/user_routes.h"
#include "routes/device_routes.h"
#include "routes/rule_routes.h"

int main(int argc, char **argv)
{
    if (!auth::init())
    {
        std::cerr << "Cannot initialize password hashing. Set IOT_PASSWORD_PEPPER_HEX to 64 hex characters.\n";
        return 1;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--hash-password")
    {
        std::string password;
        if (!std::getline(std::cin, password) || !auth::validNewPassword(Json::Value(password)))
        {
            std::cerr << "Password must contain 15 to 256 bytes\n";
            return 1;
        }
        const auto hash = auth::hashPassword(password);
        if (hash.empty())
            return 1;
        std::cout << hash << '\n';
        return 0;
    }
    curl_global_init(CURL_GLOBAL_DEFAULT);
    auto env = [](const char *key, const char *fallback = "") -> std::string {
        const auto value = std::getenv(key);
        return value ? value : fallback;
    };
    std::vector<std::unique_ptr<INotifier>> notifiers;
    if (auto token = env("IOT_TELEGRAM_BOT_TOKEN"); !token.empty())
        notifiers.push_back(std::make_unique<TelegramNotifier>(token));
    if (auto url = env("IOT_SMTP_URL"); !url.empty())
        notifiers.push_back(std::make_unique<EmailNotifier>(SmtpConfig{
            url, env("IOT_SMTP_USER"), env("IOT_SMTP_PASSWORD"), env("IOT_SMTP_FROM", "iot@example.com")}));
    // Один сервис с обоими каналами — без дублирования бота из исходного main.cc.
    auto notifications = std::make_shared<NotificationService>(std::move(notifiers));
    drogon::app().loadConfigFile("config.json");
    registerUserRoutes();
    registerDeviceRoutes(notifications);
    registerRuleRoutes();
    drogon::app().run();
}
