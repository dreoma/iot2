#include "device_routes.h"

#include "models/device.h"
#include "repositories/device_repository.h"
#include "repositories/telemetry_repository.h"
#include "services/rule_engine.h"

#include "crypto/decoder.h"

#include <drogon/drogon.h>

#include <sstream>
#include <cstdlib>

using namespace drogon;
using namespace drogon::orm;

namespace
{
// 1. Единый формат ошибок для всего API
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

bool isValidDeviceBody(const Json::Value *body)
{
    return body && body->isMember("name") && body->isMember("user_id") &&
           (*body)["name"].isString() && (*body)["user_id"].isInt64();
}

Json::Value jsonFromDatabase(const std::string &json)
{
    Json::Value result;
    Json::CharReaderBuilder builder;
    std::string errors;
    std::istringstream stream(json);
    Json::parseFromStream(builder, stream, &result, &errors);
    return result;
}

std::string trafficKey()
{
    const char *value = std::getenv("TRAFFIC_KEY_HEX");
    if (!value)
        throw std::runtime_error("TRAFFIC_KEY_HEX is not set");
    return value;
}
}  // namespace

void registerDeviceRoutes(std::shared_ptr<NotificationService> notifications)
{
    auto repository = std::make_shared<DeviceRepository>(nullptr);
    auto telemetryRepository = std::make_shared<TelemetryRepository>(nullptr);
    app().registerBeginningAdvice([repository, telemetryRepository] {
        repository->setDb(app().getDbClient("postgres"));
        telemetryRepository->setDb(app().getDbClient("postgres"));
    });

    // ==========================================
    // ДОБАВЛЕН ПРЕФИКС /api/v1
    // ==========================================

    // POST /api/v1/devices
    app().registerHandler(
        "/api/v1/devices",
        [repository](const HttpRequestPtr &request,
                     std::function<void(const HttpResponsePtr &)> &&callback) {
            const auto body = request->getJsonObject();
            if (!isValidDeviceBody(body.get()))
            {
                sendError(callback, k400BadRequest, "VALIDATION_ERROR", "name and user_id are required");
                return;
            }
            const bool notifications = body->get("notifications_enabled", true).asBool();
            repository->create(
                (*body)["name"].asString(), notifications, (*body)["user_id"].asInt64(),
                [callback](const Result &result) {
                    auto response = HttpResponse::newHttpJsonResponse(deviceToJson(result[0]));
                    response->setStatusCode(k201Created);
                    callback(response);
                },
                [callback](const DrogonDbException &) {
                    sendError(callback, k409Conflict, "CONFLICT", "user does not exist");
                });
        },
        {Post});

        // GET /api/v1/devices?user_id=1&page=1&limit=10
    app().registerHandler(
        "/api/v1/devices",
        [repository](const HttpRequestPtr &request,
                     std::function<void(const HttpResponsePtr &)> &&callback) {
            
            // 1. Получаем user_id (обязательный параметр)
            std::string userIdStr = request->getParameter("user_id");
            if (userIdStr.empty()) {
                sendError(callback, k400BadRequest, "VALIDATION_ERROR", "user_id parameter is required");
                return;
            }
            long long userId = std::stoll(userIdStr);

            // 2. Получаем пагинацию
            int page = 1;
            int limit = 10;
            if (!request->getParameter("page").empty()) page = std::stoi(request->getParameter("page"));
            if (!request->getParameter("limit").empty()) limit = std::stoi(request->getParameter("limit"));
            int offset = (page - 1) * limit;

            // 3. Вызываем репозиторий (передаем userId, limit, offset)
            repository->findAll(userId, limit, offset,
                [callback, page, limit](const Result &result) {
                    Json::Value responseBody;
                    Json::Value devices(Json::arrayValue);
                    for (const auto &row : result)
                        devices.append(deviceToJson(row));
                    
                    responseBody["data"] = devices;
                    responseBody["meta"]["page"] = page;
                    responseBody["meta"]["limit"] = limit;
                    
                    callback(HttpResponse::newHttpJsonResponse(responseBody));
                },
                [callback](const DrogonDbException &) {
                    sendError(callback, k500InternalServerError, "DATABASE_ERROR", "database error");
                });
        },
        {Get});

        // GET /api/v1/devices?user_id=1&page=1&limit=10
    app().registerHandler(
        "/api/v1/devices",
        [repository](const HttpRequestPtr &request,
                     std::function<void(const HttpResponsePtr &)> &&callback) {
            
            // Получаем user_id (обязательно для фильтрации устройств)
            std::string userIdStr = request->getParameter("user_id");
            if (userIdStr.empty()) {
                sendError(callback, k400BadRequest, "VALIDATION_ERROR", "user_id parameter is required");
                return;
            }
            long long userId = std::stoll(userIdStr);

            // Получаем пагинацию
            int page = 1;
            int limit = 10;
            if (!request->getParameter("page").empty()) page = std::stoi(request->getParameter("page"));
            if (!request->getParameter("limit").empty()) limit = std::stoi(request->getParameter("limit"));
            int offset = (page - 1) * limit;

            // Передаем userId в репозиторий
            repository->findAll(userId, limit, offset,
                [callback, page, limit](const Result &result) {
                    Json::Value responseBody;
                    Json::Value devices(Json::arrayValue);
                    for (const auto &row : result)
                        devices.append(deviceToJson(row));
                    
                    responseBody["data"] = devices;
                    responseBody["meta"]["page"] = page;
                    responseBody["meta"]["limit"] = limit;
                    
                    callback(HttpResponse::newHttpJsonResponse(responseBody));
                },
                [callback](const DrogonDbException &) {
                    sendError(callback, k500InternalServerError, "DATABASE_ERROR", "database error");
                });
        },
        {Get});

    // PUT /api/v1/devices/{id}
    app().registerHandler(
        "/api/v1/devices/{1}",
        [repository](const HttpRequestPtr &request,
                     std::function<void(const HttpResponsePtr &)> &&callback,
                     long long deviceId) {
            const auto body = request->getJsonObject();
            if (!isValidDeviceBody(body.get()))
            {
                sendError(callback, k400BadRequest, "VALIDATION_ERROR", "name and user_id are required");
                return;
            }
            const bool notifications = body->get("notifications_enabled", true).asBool();
            repository->update(
                deviceId, (*body)["name"].asString(), notifications,
                (*body)["user_id"].asInt64(),
                [callback](const Result &result) {
                    if (result.empty())
                    {
                        sendError(callback, k404NotFound, "NOT_FOUND", "device not found");
                        return;
                    }
                    callback(HttpResponse::newHttpJsonResponse(deviceToJson(result[0])));
                },
                [callback](const DrogonDbException &) {
                    sendError(callback, k409Conflict, "CONFLICT", "user does not exist");
                });
        },
        {Put});

        // DELETE /api/v1/devices/{id}?user_id=1
    app().registerHandler(
        "/api/v1/devices/{1}",
        [repository](const HttpRequestPtr &request,
                     std::function<void(const HttpResponsePtr &)> &&callback,
                     long long deviceId) {
            
            // Получаем user_id для проверки прав (что устройство принадлежит ему)
            std::string userIdStr = request->getParameter("user_id");
            if (userIdStr.empty()) {
                sendError(callback, k400BadRequest, "VALIDATION_ERROR", "user_id parameter is required for authorization");
                return;
            }
            long long userId = std::stoll(userIdStr);

            // Передаем и deviceId, и userId
            repository->remove(
                deviceId, userId,
                [callback](const Result &result) {
                    if (result.empty())
                    {
                        // Если удаление не произошло, значит либо нет такого ID, либо он не принадлежит юзеру
                        sendError(callback, k404NotFound, "NOT_FOUND", "device not found or access denied");
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

    // POST /api/v1/telemetry
    // Примечание: я убрал слово "encrypted" из URL. REST не любит глаголы/прилагательные в путях.
    // Клиент просто отправляет зашифрованные данные на этот эндпоинт.
    app().registerHandler(
        "/api/v1/telemetry",
        [telemetryRepository, notifications](const HttpRequestPtr &request,
                              std::function<void(const HttpResponsePtr &)> &&callback) {
            const auto body = request->getJsonObject();
            if (!body || !body->isMember("payload") || !(*body)["payload"].isString())
            {
                sendError(callback, k400BadRequest, "VALIDATION_ERROR", "encrypted payload is required");
                return;
            }
            try
            {
                Decoder decoder(trafficKey());
                Json::Value telemetry;
                Json::CharReaderBuilder reader;
                std::string errors;
                std::istringstream stream(decoder.decrypt((*body)["payload"].asString()));
                if (!Json::parseFromStream(reader, stream, &telemetry, &errors) ||
                    !telemetry.isMember("device_id") || !telemetry["device_id"].isInt64() ||
                    !telemetry.isMember("data") || !telemetry["data"].isObject())
                {
                    sendError(callback, k400BadRequest, "VALIDATION_ERROR", "invalid encrypted telemetry");
                    return;
                }
                telemetryRepository->create(
                    telemetry["device_id"].asInt64(), telemetry["data"],
                    [callback, notifications](const Result &result) {
                        if (result.empty())
                        {
                            sendError(callback, k404NotFound, "NOT_FOUND", "device with owner not found");
                            return;
                        }
                        const auto &row = result[0];
                        Json::Value saved;
                        saved["id"] = Json::Int64(row["id"].as<long long>());
                        saved["device_id"] = Json::Int64(row["device_id"].as<long long>());
                        saved["user_id"] = Json::Int64(row["user_id"].as<long long>());
                        saved["data"] = jsonFromDatabase(row["data"].as<std::string>());
                        saved["received_at"] = row["received_at"].as<std::string>();
                        
                        auto stored = saved;
                        stored["device_name"] = row["device_name"].as<std::string>();
                        stored["mail"] = row["mail"].isNull() ? "" : row["mail"].as<std::string>();
                        stored["previous_data"] = jsonFromDatabase(row["previous_data"].as<std::string>());
                        stored["notifications_enabled"] = row["notifications_enabled"].as<bool>();
                        stored["user_id"] = row["user_id"].isNull() ? Json::Value() : Json::Value(Json::Int64(row["user_id"].as<long long>()));
                        stored["telegram_id"] = row["telegram_id"].isNull() ? Json::Value() : Json::Value(Json::Int64(row["telegram_id"].as<int64_t>()));
                        
                        RuleEngine(notifications).evaluate(stored, [saved, callback](bool ok, int count) mutable {
                            saved["rules_evaluated"] = ok;
                            saved["notifications_created"] = count;
                            if (!ok) LOG_ERROR << "Rule evaluation failed for telemetry " << saved["id"].asInt64();
                            auto response = HttpResponse::newHttpJsonResponse(saved);
                            response->setStatusCode(k201Created);
                            callback(response);
                        });
                    },
                    [callback](const DrogonDbException &) {
                        sendError(callback, k409Conflict, "CONFLICT", "device does not exist");
                    });
            }
            catch (const std::exception &)
            {
                sendError(callback, k400BadRequest, "DECRYPTION_ERROR", "cannot decrypt telemetry");
            }
        },
        {Post});
}