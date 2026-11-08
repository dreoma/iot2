#pragma once

#include <drogon/drogon.h>

#include <sstream>
#include <string>

// Модель таблицы devices. Телеметрия хранится в device_telemetry,
// потому что у одного устройства может быть много измерений.
struct Device
{
    long long id;
    std::string name;
    bool notificationsEnabled;
};

inline Json::Value deviceTelemetryFromDatabase(const std::string &json)
{
    Json::Value result(Json::arrayValue);
    Json::CharReaderBuilder builder;
    std::string errors;
    std::istringstream stream(json);
    Json::parseFromStream(builder, stream, &result, &errors);
    return result.isArray() ? result : Json::Value(Json::arrayValue);
}

inline Json::Value deviceToJson(const drogon::orm::Row &row)
{
    Json::Value device;
    device["id"] = Json::Int64(row["id"].as<long long>());
    device["name"] = row["name"].as<std::string>();
    device["user_id"] = Json::Int64(row["user_id"].as<long long>());
    device["notifications_enabled"] = row["notifications_enabled"].as<bool>();
    device["telemetry"] = deviceTelemetryFromDatabase(row["telemetry"].as<std::string>());
    return device;
}
