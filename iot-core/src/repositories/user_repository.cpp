#include "user_repository.h"

#include <utility>

using namespace drogon::orm;

namespace
{
// Присоединяет к пользователю массив его устройств.
constexpr const char *kUsersWithDevices = R"SQL(
SELECT u.id, u.mail, u.telegram_id,
       COALESCE(
           json_agg(
               json_build_object(
                   'id', d.id,
                   'name', d.name,
                   'notifications_enabled', d.notifications_enabled
               )
           ) FILTER (WHERE d.id IS NOT NULL),
           '[]'::json
       ) AS users_devices
FROM users u
LEFT JOIN user_devices ud ON ud.user_id = u.id
LEFT JOIN devices d ON d.id = ud.device_id
)SQL";
}  // namespace

UserRepository::UserRepository(DbClientPtr db) : db_(std::move(db))
{
}

void UserRepository::create(const std::string &mail,
                            int64_t telegramId,
                            Success onSuccess,
                            Failure onFailure)
{
    db_->execSqlAsync(
        "INSERT INTO users (mail, telegram_id) VALUES ($1, $2) "
        "RETURNING id, mail, telegram_id, '[]'::json AS users_devices",
        std::move(onSuccess),
        std::move(onFailure),
        mail,
        telegramId);
}

void UserRepository::findAll(Success onSuccess, Failure onFailure)
{
    db_->execSqlAsync(std::string(kUsersWithDevices) +
                          " GROUP BY u.id ORDER BY u.id",
                      std::move(onSuccess),
                      std::move(onFailure));
}

void UserRepository::update(long long userId,
                            const std::string &mail,
                            int64_t telegramId,
                            Success onSuccess,
                            Failure onFailure)
{
    db_->execSqlAsync(
        "WITH updated_user AS ("
        "  UPDATE users SET mail = $2, telegram_id = $3 WHERE id = $1 "
        "  RETURNING id, mail, telegram_id"
        ") SELECT u.id, u.mail, u.telegram_id, "
        "COALESCE(json_agg(json_build_object('id', d.id, 'name', d.name, "
        "'notifications_enabled', d.notifications_enabled)) "
        "FILTER (WHERE d.id IS NOT NULL), '[]'::json) AS users_devices "
        "FROM updated_user u "
        "LEFT JOIN user_devices ud ON ud.user_id = u.id "
        "LEFT JOIN devices d ON d.id = ud.device_id "
        "GROUP BY u.id, u.mail, u.telegram_id",
        std::move(onSuccess),
        std::move(onFailure),
        userId,
        mail,
        telegramId);
}

void UserRepository::findById(int64_t userId, Success onSuccess, Failure onFailure)
{
    db_->execSqlAsync(std::string(kUsersWithDevices) +
                          " WHERE u.id = $1 GROUP BY u.id",
                      std::move(onSuccess),
                      std::move(onFailure),
                      userId);
}

void UserRepository::remove(int64_t userId, Success onSuccess, Failure onFailure)
{
    db_->execSqlAsync("DELETE FROM users WHERE id = $1 RETURNING id",
                      std::move(onSuccess),
                      std::move(onFailure),
                      userId);
}
