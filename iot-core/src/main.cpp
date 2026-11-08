#include <drogon/drogon.h>
#include <curl/curl.h>
#include "notifications/TelegramNotifier.h"
#include "notifications/EmailNotifier.h"
#include "services/NotificationService.h"
#include <cstdlib>

#include "routes/user_routes.h"
#include "routes/device_routes.h"
#include "routes/rule_routes.h"

int main()
{
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
