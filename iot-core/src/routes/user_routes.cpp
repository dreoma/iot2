#include "user_routes.h"

#include "models/user.h"
#include "repositories/user_repository.h"
#include "services/auth.h"

#include <algorithm>

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

bool validEmail(const Json::Value &value)
{
    if (!validMail(value))
        return false;
    const auto mail = value.asString();
    const auto at = mail.find('@');
    return at != std::string::npos && at > 0 && at + 1 < mail.size() &&
           mail.find('@', at + 1) == std::string::npos &&
           std::none_of(mail.begin(), mail.end(), [](unsigned char ch) { return ch <= ' ' || ch == 127; });
}

auto databaseError(const Callback &done)
{
    return [done](const DrogonDbException &) { error(done, k503ServiceUnavailable, "database unavailable"); };
}

auto profileWriteError(const Callback &done)
{
    return [done](const DrogonDbException &) {
        error(done, k409Conflict, "could not save user; mail may already exist");
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

void startRegistration(const std::shared_ptr<UserRepository> &repository, const HttpRequestPtr &request,
                       const Callback &done, const std::string &telegramUsername)
{
    const auto body = request->getJsonObject();
    if (!body || !body->isObject() || body->size() != 1 || !validEmail((*body)["mail"]))
        return error(done, k400BadRequest, "mail is required");
    if (telegramUsername.empty())
        return error(done, k503ServiceUnavailable, "telegram registration unavailable");

    const auto token = auth::newToken();
    const auto telegramToken = auth::newToken();
    const auto link = "https://t.me/" + telegramUsername + "?start=" + telegramToken;
    repository->startRegistration((*body)["mail"].asString(), auth::digest(token), auth::digest(telegramToken),
        [done, token, link](const Result &rows) {
            if (rows.empty())
                return error(done, k409Conflict, "mail is already registered");
            Json::Value result;
            result["registration_token"] = token;
            result["telegram_url"] = link;
            result["expires_in"] = 600;
            auto response = HttpResponse::newHttpJsonResponse(result);
            response->setStatusCode(k201Created);
            response->addHeader("Cache-Control", "no-store");
            done(response);
        }, databaseError(done));
}

void registrationStatus(const std::shared_ptr<UserRepository> &repository, const HttpRequestPtr &request,
                        const Callback &done)
{
    const auto digest = auth::tokenDigest(request);
    if (digest.empty())
        return error(done, k401Unauthorized, "registration token required");
    repository->registrationStatus(digest, [done](const Result &rows) {
        if (rows.empty())
            return error(done, k404NotFound, "registration expired or not found");
        Json::Value result;
        result["telegram_confirmed"] = rows[0]["confirmed"].as<bool>();
        auto response = HttpResponse::newHttpJsonResponse(result);
        response->addHeader("Cache-Control", "no-store");
        done(response);
    }, databaseError(done));
}

void finishRegistration(const std::shared_ptr<UserRepository> &repository, const HttpRequestPtr &request,
                        const Callback &done)
{
    const auto body = request->getJsonObject();
    if (!body || !body->isObject() || body->size() != 1 || !auth::validNewPassword((*body)["password"]))
        return error(done, k400BadRequest, "password (15 to 256 bytes) is required");
    const auto digest = auth::tokenDigest(request);
    if (digest.empty())
        return error(done, k401Unauthorized, "registration token required");

    repository->registrationStatus(digest, [repository, done, digest, password = (*body)["password"].asString()]
                                  (const Result &rows) {
        if (rows.empty())
            return error(done, k404NotFound, "registration expired or not found");
        if (!rows[0]["confirmed"].as<bool>())
            return error(done, k409Conflict, "confirm telegram first");
        auth::passwordTask(done, [repository, done, digest, password] {
            const auto hash = auth::hashPassword(password);
            if (hash.empty())
                return error(done, k503ServiceUnavailable, "password hashing unavailable");
            repository->finishRegistration(digest, hash, [done](const Result &created) {
                if (created.empty())
                    return error(done, k409Conflict, "registration expired or mail or telegram already registered");
                auto response = HttpResponse::newHttpJsonResponse(userToJson(created[0]));
                response->setStatusCode(k201Created);
                done(response);
            }, databaseError(done));
        });
    }, databaseError(done));
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

void registerUserRoutes(std::function<std::string()> telegramUsername)
{
    auto repository = std::make_shared<UserRepository>(nullptr);
    app().registerBeginningAdvice([repository] { repository->setDb(app().getDbClient("postgres")); });

    app().registerHandler("/auth/register/start",
        [repository, telegramUsername = std::move(telegramUsername)](const HttpRequestPtr &request, Callback &&done) {
            startRegistration(repository, request, done, telegramUsername());
        }, {Post});
    app().registerHandler("/auth/register/status", [repository](const HttpRequestPtr &request, Callback &&done) {
        registrationStatus(repository, request, done);
    }, {Get});
    app().registerHandler("/users", [repository](const HttpRequestPtr &request, Callback &&done) {
        finishRegistration(repository, request, done);
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
        if (!body || !body->isObject() || body->size() != 1 || !validEmail((*body)["mail"]))
            return error(done, k400BadRequest, "only mail can be updated here");
        repository->update(id, (*body)["mail"].asString(),
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
