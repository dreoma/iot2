#pragma once

#include <drogon/orm/DbClient.h>
#include <json/value.h>

#include <functional>

class TelemetryRepository
{
  public:
    using Success = std::function<void(const drogon::orm::Result &)>;
    using Failure = std::function<void(const drogon::orm::DrogonDbException &)>;

    explicit TelemetryRepository(drogon::orm::DbClientPtr db);
    void setDb(drogon::orm::DbClientPtr db) { db_ = db; }

    void create(long long deviceId, const Json::Value &data,
                Success onSuccess, Failure onFailure);

  private:
    drogon::orm::DbClientPtr db_;
};
