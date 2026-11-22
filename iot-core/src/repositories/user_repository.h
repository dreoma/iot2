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
                const std::string &passwordHash,
                Success onSuccess,
                Failure onFailure);
    void update(long long userId,
                const std::string &mail,
                int64_t telegramId,
                Success onSuccess,
                Failure onFailure);
    void findById(int64_t userId, Success onSuccess, Failure onFailure);
    void remove(int64_t userId, Success onSuccess, Failure onFailure);
    void credentials(const std::string &mail, Success done, Failure fail);
    void sessionCredentials(const std::string &digest, Success done, Failure fail);
    void saveSession(long long id, const std::string &mail, const std::string &passwordHash,
                     const std::string &updatedHash, const std::string &digest, Success done, Failure fail);
    void clearSession(const std::string &digest, Success done, Failure fail);
    void changePassword(const std::string &digest, const std::string &oldHash,
                        const std::string &newHash, Success done, Failure fail);

  private:
    drogon::orm::DbClientPtr db_;
};
