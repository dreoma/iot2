#include "device_repository.h"

#include <utility>

using namespace drogon::orm;

namespace
{
// Один запрос возвращает устройство, владельца и всю его телеметрию.
constexpr const char *kDevicesWithTelemetry = R"SQL(
SELECT d.id, d.name, d.notifications_enabled, ud.user_id,
       COALESCE(
           json_agg(
               json_build_object('id', t.id, 'user_id', t.user_id, 'data', t.data, 'received_at', t.received_at)
               ORDER BY t.received_at DESC
           ) FILTER (WHERE t.id IS NOT NULL),
           '[]'::json
       ) AS telemetry
FROM devices d
JOIN user_devices ud ON ud.device_id = d.id
LEFT JOIN device_telemetry t ON t.device_id = d.id
)SQL";
}  // namespace

DeviceRepository::DeviceRepository(DbClientPtr db) : db_(std::move(db))
{
}

void DeviceRepository::create(const std::string &name,
                              bool notificationsEnabled,
                              long long userId,
                              Success onSuccess,
                              Failure onFailure)
{
    // CTE создаёт устройство и связь с владельцем как одну атомарную операцию.
    db_->execSqlAsync(
        "WITH new_device AS ("
        "  INSERT INTO devices (name, notifications_enabled) VALUES ($1, $2) RETURNING id, name, notifications_enabled"
        "), owner AS ("
        "  INSERT INTO user_devices (user_id, device_id) SELECT $3, id FROM new_device RETURNING user_id, device_id"
        ") SELECT d.id, d.name, d.notifications_enabled, o.user_id, '[]'::json AS telemetry "
        "FROM new_device d JOIN owner o ON o.device_id = d.id",
        std::move(onSuccess), std::move(onFailure), name, notificationsEnabled, userId);
}

void DeviceRepository::findAll(long long userId, int limit, int offset, Success onSuccess, Failure onFailure) {
    db_->execSqlAsync(
        "SELECT * FROM devices WHERE user_id = $1 ORDER BY id LIMIT $2 OFFSET $3",
        onSuccess, onFailure,
        userId, limit, offset
    );
}

void DeviceRepository::findById(long long deviceId, long long userId, Success onSuccess, Failure onFailure)
{
    db_->execSqlAsync(std::string(kDevicesWithTelemetry) +
                          " WHERE d.id = $1 AND ud.user_id = $2 GROUP BY d.id, ud.user_id",
                      std::move(onSuccess), std::move(onFailure), deviceId, userId);
}

void DeviceRepository::update(long long deviceId,
                              const std::string &name,
                              bool notificationsEnabled,
                              long long userId,
                              Success onSuccess,
                              Failure onFailure)
{
    db_->execSqlAsync(
        "UPDATE devices d SET name = $2, notifications_enabled = $3 FROM user_devices ud "
        "WHERE d.id = $1 AND ud.device_id = d.id AND ud.user_id = $4 "
        "RETURNING d.id, d.name, d.notifications_enabled, ud.user_id, '[]'::json AS telemetry",
        std::move(onSuccess), std::move(onFailure), deviceId, name, notificationsEnabled, userId);
}

void DeviceRepository::remove(long long deviceId, long long userId, Success onSuccess, Failure onFailure)
{
    db_->execSqlAsync("DELETE FROM devices d USING user_devices ud "
                      "WHERE d.id = $1 AND ud.device_id = d.id AND ud.user_id = $2 RETURNING d.id",
                      std::move(onSuccess), std::move(onFailure), deviceId, userId);
}
