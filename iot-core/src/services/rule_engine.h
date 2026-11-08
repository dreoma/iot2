#pragma once

#include <json/value.h>
#include <functional>
#include <memory>
#include "NotificationService.h"

class RuleEngine
{
  public:
    explicit RuleEngine(std::shared_ptr<NotificationService> notifications)
        : notifications_(std::move(notifications)) {}
    using Done = std::function<void(bool success, int notificationsCreated)>;
    // stored содержит измерение, предыдущие данные и текущего владельца.
    void evaluate(const Json::Value &stored, Done done) const;

    // Чистая функция: удобна для проверки логики без сервера и БД.
    static bool matches(const Json::Value &rule,
                        const Json::Value &previous, const Json::Value &current);
  private:
    std::shared_ptr<NotificationService> notifications_;
};
