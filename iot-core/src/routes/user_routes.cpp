#include "user_routes.h"

#include "models/user.h"
#include "repositories/user_repository.h"

#include <drogon/drogon.h>

using namespace drogon;
using namespace drogon::orm;

namespace
{
// 1. Улучшенная функция отправки ошибок (в стиле RFC 7807)
void sendError(const std::function<void(const HttpResponsePtr &)> &callback,
               HttpStatusCode status,
               const std::string &code,
               const std::string &message)
{
    Json::Value errorBody;
    errorBody["error"]["code"] = code;
    errorBody["error"]["message"] = message;
    errorBody["error"]["status"] = static_cast<int>(status);
    
    auto response = HttpResponse::newHttpJsonResponse(errorBody);
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

    // ==========================================
    // 2. ДОБАВЛЕН ПРЕФИКС /api/v1
    // ==========================================

    // POST /api/v1/users (Создание)
    app().registerHandler(
        "/api/v1/users",
        [repository](const HttpRequestPtr &request,
                     std::function<void(const HttpResponsePtr &)> &&callback) {
            const auto body = request->getJsonObject();
            if (!body || !body->isMember("telegram_id") || !body->isMember("mail"))
            {
                sendError(callback, k400BadRequest, "VALIDATION_ERROR", "mail and telegram_id are required");
                return;
            }

            repository->create(
                (*body)["mail"].asString(),
                (*body)["telegram_id"].asInt64(),
                "",
                [callback](const Result &result) {
                    auto response = HttpResponse::newHttpJsonResponse(userToJson(result[0]));
                    response->setStatusCode(k201Created);
                    // Хорошая практика: возвращать заголовок Location с URL нового ресурса
                    // response->addHeader("Location", "/api/v1/users/" + std::to_string(result[0]["id"].as<long long>()));
                    callback(response);
                },
                [callback](const DrogonDbException &) {
                    sendError(callback, k409Conflict, "CONFLICT", "mail or telegram_id already exists");
                });
        },
        {Post});

    // GET /api/v1/users (Получение списка с ПАГИНАЦИЕЙ)
    app().registerHandler(
        "/api/v1/users",
        [repository](const HttpRequestPtr &request,
                     std::function<void(const HttpResponsePtr &)> &&callback) {
            // 3. Читаем параметры пагинации из URL (?page=1&limit=10)
            int page = 1;
            int limit = 10;
            
            if (request->getParameter("page") != "") page = std::stoi(request->getParameter("page"));
            if (request->getParameter("limit") != "") limit = std::stoi(request->getParameter("limit"));
            int offset = (page - 1) * limit;

            // ВАЖНО: Вам нужно будет обновить метод findAll в репозитории, 
            // чтобы он принимал limit и offset (например: SELECT * FROM users LIMIT $1 OFFSET $2)
            repository->findAll(limit, offset, 
                [callback, page, limit](const Result &result) {
                    Json::Value responseBody;
                    Json::Value users(Json::arrayValue);
                    for (const auto &row : result)
                        users.append(userToJson(row));
                        
                    // Формируем "конверт" ответа
                    responseBody["data"] = users;
                    responseBody["meta"]["page"] = page;
                    responseBody["meta"]["limit"] = limit;
                    // responseBody["meta"]["total"] = ...; // Желательно добавить подсчет общего количества
                    
                    callback(HttpResponse::newHttpJsonResponse(responseBody));
                },
                [callback](const DrogonDbException &) {
                    sendError(callback, k500InternalServerError, "DATABASE_ERROR", "database error");
                });
        },
        {Get});

    // GET /api/v1/users/{id} (Получение по ID)
    app().registerHandler(
        "/api/v1/users/{1}",
        [repository](const HttpRequestPtr &,
                     std::function<void(const HttpResponsePtr &)> &&callback,
                     long long usrId) {
            repository->findById(
                usrId,
                [callback](const Result &result) {
                    if (result.empty())
                    {
                        sendError(callback, k404NotFound, "NOT_FOUND", "user not found");
                        return;
                    }
                    callback(HttpResponse::newHttpJsonResponse(userToJson(result[0])));
                },
                [callback](const DrogonDbException &) {
                    sendError(callback, k500InternalServerError, "DATABASE_ERROR", "database error");
                });
        },
        {Get});

    // PUT /api/v1/users/{id} (Полное обновление)
    app().registerHandler(
        "/api/v1/users/{1}",
        [repository](const HttpRequestPtr &request,
                     std::function<void(const HttpResponsePtr &)> &&callback,
                     long long userId) {
            const auto body = request->getJsonObject();
            if (!body || !body->isMember("telegram_id") || !body->isMember("mail"))
            {
                sendError(callback, k400BadRequest, "VALIDATION_ERROR", "mail and telegram_id are required");
                return;
            }
            repository->update(
                userId,
                (*body)["mail"].asString(),
                (*body)["telegram_id"].asInt64(),
                [callback](const Result &result) {
                    if (result.empty())
                    {
                        sendError(callback, k404NotFound, "NOT_FOUND", "user not found");
                        return;
                    }
                    callback(HttpResponse::newHttpJsonResponse(userToJson(result[0])));
                },
                [callback](const DrogonDbException &) {
                    sendError(callback, k409Conflict, "CONFLICT", "mail or telegram_id already exists");
                });
        },
        {Put});
        
    // PATCH /api/v1/users/{id} (ЧАСТИЧНОЕ обновление)
    // (Опционально, но настоятельно рекомендуется для REST)
    app().registerHandler(
        "/api/v1/users/{1}",
        [repository](const HttpRequestPtr &request,
                     std::function<void(const HttpResponsePtr &)> &&callback,
                     long long userId) {
            const auto body = request->getJsonObject();
            if (!body) {
                sendError(callback, k400BadRequest, "VALIDATION_ERROR", "JSON body is required");
                return;
            }
            
            // Логика частичного обновления: если поля нет - берем старое значение или передаем NULL
            // Это потребует написания отдельного метода updatePartial в репозитории.
            // ... (реализация зависит от вашего SQL-запроса)
            
            sendError(callback, k501NotImplemented, "NOT_IMPLEMENTED", "PATCH method not implemented yet");
        },
        {Patch});

    // DELETE /api/v1/users/{id} (Удаление)
    app().registerHandler(
        "/api/v1/users/{1}",
        [repository](const HttpRequestPtr &,
                     std::function<void(const HttpResponsePtr &)> &&callback,
                     long long userId) {
            repository->remove(
                userId,
                [callback](const Result &result) {
                    if (result.empty())
                    {
                        sendError(callback, k404NotFound, "NOT_FOUND", "user not found");
                        return;
                    }
                    auto response = HttpResponse::newHttpResponse();
                    response->setStatusCode(k204NoContent);
                    callback(response);
                },
                [callback](const DrogonDbException &) {
                    sendError(callback, k500InternalServerError, "DATABASE_ERROR", "database error");
                });
        },
        {Delete});
}