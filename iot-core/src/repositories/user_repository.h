#pragma once

#include <drogon/orm/DbClient.h>

#include <cstdint>
#include <functional>
#include <string>

// В этом классе хранится весь SQL для users и user_devices.
class UserRepository
{
  public:
    using Success = std::function<void(const drogon::orm::Result &)>;
    using Failure = std::function<void(const drogon::orm::DrogonDbException &)>;

    explicit UserRepository(drogon::orm::DbClientPtr db);
    void setDb(drogon::orm::DbClientPtr db) { db_ = db; }

    void create(const std::string &mail,
                int64_t telegramId,
                Success onSuccess,
                Failure onFailure);
    void update(long long userId,
                const std::string &mail,
                int64_t telegramId,
                Success onSuccess,
                Failure onFailure);
    void findAll(Success onSuccess, Failure onFailure);
    void findById(int64_t userId, Success onSuccess, Failure onFailure);
    void remove(int64_t userId, Success onSuccess, Failure onFailure);

  private:
    drogon::orm::DbClientPtr db_;
};
