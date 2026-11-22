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

void UserRepository::startRegistration(const std::string &mail, const std::string &registrationDigest,
                                       const std::string &telegramDigest, Success done, Failure fail)
{
    db_->execSqlAsync(R"SQL(
        WITH expired AS (DELETE FROM pending_registrations WHERE expires_at <= now())
        INSERT INTO pending_registrations (mail, registration_digest, telegram_digest)
        SELECT $1::text, $2, $3 WHERE NOT EXISTS (SELECT 1 FROM users WHERE mail = $1)
        RETURNING registration_digest
    )SQL", std::move(done), std::move(fail), mail, registrationDigest, telegramDigest);
}

void UserRepository::registrationStatus(const std::string &registrationDigest, Success done, Failure fail)
{
    db_->execSqlAsync("SELECT telegram_id IS NOT NULL AS confirmed FROM pending_registrations "
                     "WHERE registration_digest=$1 AND expires_at > now()",
                     std::move(done), std::move(fail), registrationDigest);
}

bool UserRepository::confirmRegistration(const std::string &telegramDigest, int64_t telegramId)
{
    // Вызывается только из потока Telegram, поэтому не блокирует HTTP-обработчики.
    return !db_->execSqlSync(R"SQL(
        UPDATE pending_registrations SET telegram_id=$2
        WHERE telegram_digest=$1 AND expires_at > now()
            AND (telegram_id IS NULL OR telegram_id=$2)
            AND NOT EXISTS (SELECT 1 FROM users WHERE telegram_id=$2)
        RETURNING telegram_id
    )SQL", telegramDigest, telegramId).empty();
}

void UserRepository::finishRegistration(const std::string &registrationDigest, const std::string &passwordHash,
                                        Success done, Failure fail)
{
    // Токен можно использовать один раз. Конфликт email или Telegram проверяет сама таблица users.
    db_->execSqlAsync(R"SQL(
        WITH confirmed AS (
            DELETE FROM pending_registrations
            WHERE registration_digest=$1 AND telegram_id IS NOT NULL AND expires_at > now()
            RETURNING mail, telegram_id
        )
        INSERT INTO users (mail, telegram_id, password_hash)
        SELECT mail, telegram_id, $2 FROM confirmed
        ON CONFLICT DO NOTHING
        RETURNING id, mail, telegram_id, '[]'::json AS users_devices
    )SQL", std::move(done), std::move(fail), registrationDigest, passwordHash);
}

void UserRepository::update(long long userId,
                            const std::string &mail,
                            Success onSuccess,
                            Failure onFailure)
{
    db_->execSqlAsync(
        "WITH updated_user AS ("
        "  UPDATE users SET mail = $2 WHERE id = $1 "
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
        mail);
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

void UserRepository::credentials(const std::string &mail, Success done, Failure fail)
{
    db_->execSqlAsync("SELECT id, password_hash FROM users WHERE mail=$1",
                     std::move(done), std::move(fail), mail);
}

void UserRepository::sessionCredentials(const std::string &digest, Success done, Failure fail)
{
    db_->execSqlAsync("SELECT password_hash FROM users WHERE session_digest=$1 AND session_expires_at > now()",
                     std::move(done), std::move(fail), digest);
}

void UserRepository::saveSession(long long id, const std::string &mail, const std::string &passwordHash,
                                 const std::string &updatedHash, const std::string &digest, Success done, Failure fail)
{
    // Не выдаём сессию, если email или пароль изменились во время проверки.
    db_->execSqlAsync("UPDATE users SET password_hash=$5, session_digest=$4, session_expires_at=now() + interval '24 hours' "
                     "WHERE id=$1 AND mail=$2 AND password_hash=$3 RETURNING id",
                     std::move(done), std::move(fail), id, mail, passwordHash, digest, updatedHash);
}

void UserRepository::clearSession(const std::string &digest, Success done, Failure fail)
{
    db_->execSqlAsync("UPDATE users SET session_digest=NULL, session_expires_at=NULL WHERE session_digest=$1",
                     std::move(done), std::move(fail), digest);
}

void UserRepository::changePassword(const std::string &digest, const std::string &oldHash,
                                    const std::string &newHash, Success done, Failure fail)
{
    db_->execSqlAsync("UPDATE users SET password_hash=$3, session_digest=NULL, session_expires_at=NULL "
                     "WHERE session_digest=$1 AND password_hash=$2 AND session_expires_at > now() RETURNING id",
                     std::move(done), std::move(fail), digest, oldHash, newHash);
}
