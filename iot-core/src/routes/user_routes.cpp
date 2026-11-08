#include "user_routes.h"

#include "models/user.h"
#include "repositories/user_repository.h"

#include <drogon/drogon.h>

using namespace drogon;
using namespace drogon::orm;

namespace
{
void sendError(const std::function<void(const HttpResponsePtr &)> &callback,
               HttpStatusCode status,
               const std::string &message)
{
    Json::Value body;
    body["error"] = message;
    auto response = HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(status);
    callback(response);
}
}  // namespace

void registerUserRoutes()
{
    auto repository = std::make_shared<UserRepository>(nullptr);
    app().registerBeginningAdvice([repository] {
        repository->setDb(app().getDbClient("postgres"));
    });

    // Чтобы добавить новый маршрут, зарегистрируйте его здесь и вызовите нужный
    // метод репозитория. SQL остаётся только в repositories/user_repository.cpp.
    app().registerHandler(
        "/users",
        [repository](const HttpRequestPtr &request,
                     std::function<void(const HttpResponsePtr &)> &&callback) {
            const auto body = request->getJsonObject();
            if (!body || !body->isMember("telegram_id") || !body->isMember("mail"))
            {
                sendError(callback, k400BadRequest, "mail and telegram_id are required");
                return;
            }

            repository->create(
                (*body)["mail"].asString(),
                (*body)["telegram_id"].asInt64(),
                [callback](const Result &result) {
                    auto response = HttpResponse::newHttpJsonResponse(userToJson(result[0]));
                    response->setStatusCode(k201Created);
                    callback(response);
                },
                [callback](const DrogonDbException &) {
                    sendError(callback, k409Conflict, "mail or telegram_id already exists");
                });
        },
        {Post});

    // Пример GET: получить весь список пользователей.
    app().registerHandler(
        "/users",
        [repository](const HttpRequestPtr &,
                     std::function<void(const HttpResponsePtr &)> &&callback) {
            repository->findAll(
                [callback](const Result &result) {
                    Json::Value users(Json::arrayValue);
                    for (const auto &row : result)
                        users.append(userToJson(row));
                    callback(HttpResponse::newHttpJsonResponse(users));
                },
                [callback](const DrogonDbException &) {
                    sendError(callback, k500InternalServerError, "database error");
                });
        },
        {Get});

    // Пример GET с параметром: /users/1.
    app().registerHandler(
        "/users/{1}",
        [repository](const HttpRequestPtr &,
                     std::function<void(const HttpResponsePtr &)> &&callback,
                     long long usrId) {
            repository->findById(
                usrId,
                [callback](const Result &result) {
                    if (result.empty())
                    {
                        sendError(callback, k404NotFound, "user not found");
                        return;
                    }
                    callback(HttpResponse::newHttpJsonResponse(userToJson(result[0])));
                },
                [callback](const DrogonDbException &) {
                    sendError(callback, k500InternalServerError, "database error");
                });
        },
        {Get});

    app().registerHandler(
        "/users/{1}",
        [repository](const HttpRequestPtr &request,
                     std::function<void(const HttpResponsePtr &)> &&callback,
                     long long userId) {
            const auto body = request->getJsonObject();
            if (!body || !body->isMember("telegram_id") || !body->isMember("mail"))
            {
                sendError(callback, k400BadRequest, "mail and telegram_id are required");
                return;
            }
            repository->update(
                userId,
                (*body)["mail"].asString(),
                (*body)["telegram_id"].asInt64(),
                [callback](const Result &result) {
                    if (result.empty())
                    {
                        sendError(callback, k404NotFound, "user not found");
                        return;
                    }
                    callback(HttpResponse::newHttpJsonResponse(userToJson(result[0])));
                },
                [callback](const DrogonDbException &) {
                    sendError(callback, k409Conflict, "mail or telegram_id already exists");
                });
        },
        {Put});

    // Удаляет пользователя и строки его связи с устройствами.
    app().registerHandler(
        "/users/{1}",
        [repository](const HttpRequestPtr &,
                     std::function<void(const HttpResponsePtr &)> &&callback,
                     long long userId) {
            repository->remove(
                userId,
                [callback](const Result &result) {
                    if (result.empty())
                    {
                        sendError(callback, k404NotFound, "user not found");
                        return;
                    }
                    auto response = HttpResponse::newHttpResponse();
                    response->setStatusCode(k204NoContent);
                    callback(response);
                },
                [callback](const DrogonDbException &) {
                    sendError(callback, k500InternalServerError, "database error");
                });
        },
        {Delete});
}
