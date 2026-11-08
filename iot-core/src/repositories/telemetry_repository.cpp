#include "telemetry_repository.h"

#include <json/writer.h>

#include <utility>

TelemetryRepository::TelemetryRepository(drogon::orm::DbClientPtr db) : db_(std::move(db))
{
}

void TelemetryRepository::create(long long deviceId,
                                 const Json::Value &data,
                                 Success onSuccess,
                                 Failure onFailure)
{
    Json::StreamWriterBuilder builder;
    const auto dataJson = Json::writeString(builder, data);
    db_->execSqlAsync(
        // Предыдущее измерение читается в том же SQL snapshot, что и вставка.
        "WITH owner AS (SELECT user_id FROM user_devices WHERE device_id=$1), "
        "previous AS (SELECT data FROM device_telemetry WHERE device_id=$1 "
        "AND user_id=(SELECT user_id FROM owner) "
        "ORDER BY id DESC LIMIT 1), saved AS ("
        // Владельца определяет БД по device_id; клиент не может подставить user_id.
        "INSERT INTO device_telemetry (device_id, user_id, data) "
        "SELECT $1, user_id, $2::jsonb FROM owner "
        "RETURNING id, device_id, user_id, data, received_at) "
        "SELECT s.*, COALESCE((SELECT data FROM previous), '{}'::jsonb) AS previous_data, "
        "d.notifications_enabled, d.name AS device_name, u.telegram_id, u.mail "
        "FROM saved s JOIN devices d ON d.id=s.device_id "
        "JOIN users u ON u.id=s.user_id",
        std::move(onSuccess), std::move(onFailure), deviceId, dataJson);
}
