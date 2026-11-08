#pragma once

#include <drogon/drogon.h>

#include <cstdint>
#include <sstream>
#include <string>

// Представление пользователя для API: id, mail, telegram_id и его устройства.
struct User
{
    long long id;
    std::string mail;
    int64_t telegramId;
    Json::Value usersDevices{Json::arrayValue};
};

inline Json::Value jsonArrayFromDatabase(const std::string &json)
{
    Json::Value result(Json::arrayValue);
    Json::CharReaderBuilder builder;
    std::string errors;
    std::istringstream stream(json);
    Json::parseFromStream(builder, stream, &result, &errors);
    return result.isArray() ? result : Json::Value(Json::arrayValue);
}

inline Json::Value userToJson(const drogon::orm::Row &row)
{
    Json::Value user;
    user["id"] = Json::Int64(row["id"].as<long long>());
    user["mail"] = row["mail"].as<std::string>();
    user["telegram_id"] = Json::Int64(row["telegram_id"].as<int64_t>());
    user["users_devices"] = jsonArrayFromDatabase(
        row["users_devices"].as<std::string>());
    return user;
}
