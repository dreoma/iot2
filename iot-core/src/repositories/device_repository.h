#pragma once

#include <drogon/orm/DbClient.h>

#include <functional>
#include <string>

// SQL для устройств, их владельцев и телеметрии.
class DeviceRepository
{
  public:
    using Success = std::function<void(const drogon::orm::Result &)>;
    using Failure = std::function<void(const drogon::orm::DrogonDbException &)>;

    explicit DeviceRepository(drogon::orm::DbClientPtr db);
    void setDb(drogon::orm::DbClientPtr db) { db_ = db; }

    void create(const std::string &name, bool notificationsEnabled, long long userId,
                Success onSuccess, Failure onFailure);
                
    // ДОБАВЛЕНЫ limit и offset
    void findAll(long long userId, int limit, int offset, Success onSuccess, Failure onFailure);
    
    void findById(long long deviceId, long long userId, Success onSuccess, Failure onFailure);
    void update(long long deviceId, const std::string &name, bool notificationsEnabled,
                long long userId, Success onSuccess, Failure onFailure);
    void remove(long long deviceId, long long userId, Success onSuccess, Failure onFailure);

  private:
    drogon::orm::DbClientPtr db_;
};