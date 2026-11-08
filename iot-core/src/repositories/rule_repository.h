#pragma once

#include <drogon/drogon.h>

// Правила и очередь уведомлений живут в отдельной PostgreSQL-базе.
class RuleRepository
{
  public:
    using Success = std::function<void(const drogon::orm::Result &)>;
    using Failure = std::function<void(const drogon::orm::DrogonDbException &)>;

    void save(long long id, long long userId, const Json::Value &rule,
              Success done, Failure fail) const;
    void list(long long deviceId, Success done, Failure fail) const;
    void remove(long long id, Success done, Failure fail) const;
    void active(long long deviceId, long long userId, Success done, Failure fail) const;
    void enqueue(const Json::Value &notifications, Success done, Failure fail) const;
    void notifications(long long userId, Success done, Failure fail) const;
};

// SQL возвращает JSON; преобразование используется и в маршрутах, и в engine.
Json::Value ruleJson(const drogon::orm::Row &row);
