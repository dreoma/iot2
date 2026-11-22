#include "user_routes.h"

#include "models/user.h"
#include "repositories/user_repository.h"
#include "services/auth.h"

using namespace drogon;
using namespace drogon::orm;

namespace
{
using auth::Callback;
using auth::error;

bool validMail(const Json::Value &value)
{
    if (!value.isString())
        return false;
    const auto mail = value.asString();
    return !mail.empty() && mail.size() <= 255 && mail.find('\0') == std::string::npos;
}

bool validProfile(const std::shared_ptr<Json::Value> &body)
{
    return body && body->isObject() && validMail((*body)["mail"]) &&
           (*body)["telegram_id"].isInt64();
}

auto databaseError(const Callback &done)
{
    return [done](const DrogonDbException &) { error(done, k503ServiceUnavailable, "database unavailable"); };
}

auto profileWriteError(const Callback &done)
{
    return [done](const DrogonDbException &) {
        error(done, k409Conflict, "could not save user; mail or telegram_id may already exist");
    };
}

void noContent(const Callback &done)
{
    auto response = HttpResponse::newHttpResponse();
    response->setStatusCode(k204NoContent);
    done(response);
}

void sendProfile(const Result &rows, const Callback &done, bool array = false)
{
    if (rows.empty())
    {
        error(done, k404NotFound, "user not found");
        return;
    }
    auto profile = userToJson(rows[0]);
    if (array)
    {
        Json::Value profiles(Json::arrayValue);
        profiles.append(std::move(profile));
        done(HttpResponse::newHttpJsonResponse(profiles));
        return;
    }
    done(HttpResponse::newHttpJsonResponse(profile));
}

void login(const std::shared_ptr<UserRepository> &repository, const HttpRequestPtr &request, const Callback &done)
{
    const auto body = request->getJsonObject();
    if (!body || !body->isObject() || !validMail((*body)["mail"]) || !auth::validPasswordInput((*body)["password"]))
    {
        error(done, k400BadRequest, "mail and password are required");
        return;
    }
    const auto mail = (*body)["mail"].asString();
    repository->credentials(mail, [repository, done, mail, password = (*body)["password"].asString()](const Result &rows) {
        const auto id = rows.empty() ? 0 : rows[0]["id"].as<long long>();
        const auto hash = rows.empty() || rows[0]["password_hash"].isNull()
            ? std::string{} : rows[0]["password_hash"].as<std::string>();
        auth::passwordTask(done, [repository, done, id, mail, password, hash] {
            if (!auth::verifyPassword(password, hash))
            {
                error(done, k401Unauthorized, "invalid mail or password");
                return;
            }
            const auto updatedHash = auth::needsPasswordUpgrade(hash) ? auth::hashPassword(password) : hash;
            if (updatedHash.empty())
            {
                error(done, k503ServiceUnavailable, "password hashing unavailable");
                return;
            }
            const auto token = auth::newToken();
            repository->saveSession(id, mail, hash, updatedHash, auth::digest(token), [done, token](const Result &saved) {
                if (saved.empty())
                {
                    error(done, k401Unauthorized, "invalid mail or password");
                    return;
                }
                Json::Value result;
                result["token"] = token;
                result["user_id"] = Json::Int64(saved[0]["id"].as<long long>());
                result["expires_in"] = 86400;
                auto response = HttpResponse::newHttpJsonResponse(result);
                response->addHeader("Cache-Control", "no-store");
                done(response);
            }, databaseError(done));
        });
    }, databaseError(done));
}

void changePassword(const std::shared_ptr<UserRepository> &repository,
                    const HttpRequestPtr &request, const Callback &done)
{
    const auto body = request->getJsonObject();
    if (!body || !body->isObject() || !auth::validPasswordInput((*body)["current_password"]) ||
        !auth::validNewPassword((*body)["new_password"]))
    {
        error(done, k400BadRequest, "current_password and new_password (15 to 256 bytes) are required");
        return;
    }
    const auto digest = auth::tokenDigest(request);
    repository->sessionCredentials(digest,
        [repository, done, digest, current = (*body)["current_password"].asString(),
         password = (*body)["new_password"].asString()](const Result &rows) {
            if (rows.empty() || rows[0]["password_hash"].isNull())
            {
                error(done, k401Unauthorized, "invalid or expired session");
                return;
            }
            const auto oldHash = rows[0]["password_hash"].as<std::string>();
            auth::passwordTask(done, [repository, done, digest, current, password, oldHash] {
                if (!auth::verifyPassword(current, oldHash))
                {
                    error(done, k403Forbidden, "incorrect current password");
                    return;
                }
                const auto newHash = auth::hashPassword(password);
                if (newHash.empty())
                {
                    error(done, k503ServiceUnavailable, "password hashing unavailable");
                    return;
                }
                repository->changePassword(digest, oldHash, newHash, [done](const Result &updated) {
                    if (updated.empty())
                        error(done, k401Unauthorized, "invalid or expired session");
                    else
                        noContent(done);
                }, databaseError(done));
            });
        }, databaseError(done));
}
}  // namespace

void registerUserRoutes()
{
    auto repository = std::make_shared<UserRepository>(nullptr);
    app().registerBeginningAdvice([repository] { repository->setDb(app().getDbClient("postgres")); });

    app().registerHandler("/users", [repository](const HttpRequestPtr &request, Callback &&done) {
        const auto body = request->getJsonObject();
        if (!validProfile(body) || !auth::validNewPassword((*body)["password"]))
        {
            error(done, k400BadRequest, "mail, telegram_id and password (15 to 256 bytes) are required");
            return;
        }
        auth::passwordTask(done, [repository, done, mail = (*body)["mail"].asString(),
                                  telegramId = (*body)["telegram_id"].asInt64(),
                                  password = (*body)["password"].asString()] {
            const auto hash = auth::hashPassword(password);
            if (hash.empty())
            {
                error(done, k503ServiceUnavailable, "password hashing unavailable");
                return;
            }
            repository->create(mail, telegramId, hash, [done](const Result &rows) {
                auto response = HttpResponse::newHttpJsonResponse(userToJson(rows[0]));
                response->setStatusCode(k201Created);
                done(response);
            }, profileWriteError(done));
        });
    }, {Post});

    app().registerHandler("/auth/login", [repository](const HttpRequestPtr &request, Callback &&done) {
        login(repository, request, done);
    }, {Post});

    app().registerHandler("/auth/logout", [repository](const HttpRequestPtr &request, Callback &&done) {
        repository->clearSession(auth::tokenDigest(request), [done](const Result &) { noContent(done); },
                                 databaseError(done));
    }, {Post, "UserAuthFilter"});

    app().registerHandler("/auth/password", [repository](const HttpRequestPtr &request, Callback &&done) {
        changePassword(repository, request, done);
    }, {Put, "UserAuthFilter"});

    app().registerHandler("/users", [repository](const HttpRequestPtr &request, Callback &&done) {
        repository->findById(auth::userId(request), [done](const Result &rows) { sendProfile(rows, done, true); },
                             databaseError(done));
    }, {Get, "UserAuthFilter"});

    app().registerHandler("/users/{1}", [repository](const HttpRequestPtr &request, Callback &&done, long long id) {
        if (id != auth::userId(request))
            return error(done, k404NotFound, "user not found");
        repository->findById(id, [done](const Result &rows) { sendProfile(rows, done); }, databaseError(done));
    }, {Get, "UserAuthFilter"});

    app().registerHandler("/users/{1}", [repository](const HttpRequestPtr &request, Callback &&done, long long id) {
        if (id != auth::userId(request))
            return error(done, k404NotFound, "user not found");
        const auto body = request->getJsonObject();
        if (!validProfile(body) || body->isMember("password"))
            return error(done, k400BadRequest, "mail and telegram_id are required; use /auth/password to change password");
        repository->update(id, (*body)["mail"].asString(), (*body)["telegram_id"].asInt64(),
                           [done](const Result &rows) { sendProfile(rows, done); }, profileWriteError(done));
    }, {Put, "UserAuthFilter"});

    app().registerHandler("/users/{1}", [repository](const HttpRequestPtr &request, Callback &&done, long long id) {
        if (id != auth::userId(request))
            return error(done, k404NotFound, "user not found");
        repository->remove(id, [done](const Result &rows) {
            if (rows.empty())
                error(done, k404NotFound, "user not found");
            else
                noContent(done);
        }, databaseError(done));
    }, {Delete, "UserAuthFilter"});
}
