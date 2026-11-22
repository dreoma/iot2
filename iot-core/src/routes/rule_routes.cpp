#include "rule_routes.h"
#include "repositories/rule_repository.h"

#include <cmath>
#include <set>
#include <string>

using namespace drogon;
using namespace drogon::orm;

namespace
{
using Callback = std::function<void(const HttpResponsePtr &)>;

// Единый формат ошибок
void error(const Callback &done, HttpStatusCode status, const std::string &code, const std::string &message)
{
    Json::Value errorBody;
    errorBody["error"]["code"] = code;
    errorBody["error"]["message"] = message;
    errorBody["error"]["status"] = static_cast<int>(status);
    
    auto response = HttpResponse::newHttpJsonResponse(errorBody);
    response->setStatusCode(status);
    done(response);
}

bool valid(const Json::Value &v)
{
    static const std::set<std::string> operators = {
        "gt", "gte", "lt", "lte", "eq", "ne", "changed", "delta"
    };
    if (!v.isObject() || !v["device_id"].isInt64() || v["device_id"].asInt64() <= 0 ||
        !v["field"].isString() || v["field"].asString().empty() || v["field"].asString().size() > 100 ||
        !v["operator"].isString() || !operators.count(v["operator"].asString()) ||
        !v["message"].isString() || v["message"].asString().empty() || v["message"].asString().size() > 2000 ||
        (v.isMember("enabled") && !v["enabled"].isBool())) return false;
    if (v["operator"] != "changed" || v.isMember("threshold"))
        if (!v["threshold"].isNumeric() || !std::isfinite(v["threshold"].asDouble())) return false;
    return v["operator"] != "delta" || v["threshold"].asDouble() > 0;
}

void save(long long id, const HttpRequestPtr &request, Callback done)
{
    const auto body = request->getJsonObject();
    if (!body || !valid(*body))
    {
        error(done, k400BadRequest, "VALIDATION_ERROR", "required: device_id, field, operator, threshold (except changed), message; optional: enabled");
        return;
    }
    const auto rule = *body;
    
    // Запрос к БД для получения владельца устройства
    app().getDbClient("postgres")->execSqlAsync(
        "SELECT user_id FROM user_devices WHERE device_id=$1",
        [rule, id, done](const Result &owner) {
            if (owner.empty()) { 
                error(done, k404NotFound, "NOT_FOUND", "device or owner not found"); 
                return; 
            }
            RuleRepository().save(id, owner[0]["user_id"].as<long long>(), rule,
                [id, done](const Result &rows) {
                    if (rows.empty()) { 
                        error(done, k404NotFound, "NOT_FOUND", "rule not found"); 
                        return; 
                    }
                    auto response = HttpResponse::newHttpJsonResponse(ruleJson(rows[0]));
                    response->setStatusCode(id == 0 ? k201Created : k200OK);
                    done(response);
                },
                [done](const DrogonDbException &) { 
                    error(done, k503ServiceUnavailable, "SERVICE_UNAVAILABLE", "rules database unavailable or invalid rule"); 
                });
        },
        [done](const DrogonDbException &) { 
            error(done, k503ServiceUnavailable, "SERVICE_UNAVAILABLE", "main database unavailable"); 
        },
        rule["device_id"].asInt64());
}

void sendList(const Result &rows, const Callback &done, int page = 1, int limit = 10)
{
    Json::Value responseBody;
    Json::Value values(Json::arrayValue);
    for (const auto &row : rows) values.append(ruleJson(row));
    
    responseBody["data"] = values;
    responseBody["meta"]["page"] = page;
    responseBody["meta"]["limit"] = limit;
    
    done(HttpResponse::newHttpJsonResponse(responseBody));
}
}  // namespace

void registerRuleRoutes()
{
    // POST /api/v1/rules
    app().registerHandler("/api/v1/rules",
        [](const HttpRequestPtr &request, Callback &&done) { save(0, request, done); }, {Post});
        
    // PUT /api/v1/rules/{id}
    app().registerHandler("/api/v1/rules/{1}",
        [](const HttpRequestPtr &request, Callback &&done, long long id) {
            if (id <= 0) { 
                error(done, k400BadRequest, "VALIDATION_ERROR", "positive rule id required"); 
                return; 
            }
            save(id, request, done);
        }, {Put});

    // GET /api/v1/devices/{id}/rules?user_id=1&page=1&limit=10
    app().registerHandler("/api/v1/devices/{1}/rules",
        [](const HttpRequestPtr &request, Callback &&done, long long id) {
            
            // Извлекаем user_id для проверки прав
            std::string userIdStr = request->getParameter("user_id");
            if (userIdStr.empty()) {
                error(done, k400BadRequest, "VALIDATION_ERROR", "user_id parameter is required");
                return;
            }
            long long userId = std::stoll(userIdStr);

            // Пагинация
            int page = 1;
            int limit = 10;
            if (!request->getParameter("page").empty()) page = std::stoi(request->getParameter("page"));
            if (!request->getParameter("limit").empty()) limit = std::stoi(request->getParameter("limit"));
            int offset = (page - 1) * limit;
            
            // Передаем userId вторым аргументом
            RuleRepository().list(id, userId, limit, offset,
                [done, page, limit](const Result &rows) { sendList(rows, done, page, limit); },
                [done](const DrogonDbException &) { 
                    error(done, k503ServiceUnavailable, "SERVICE_UNAVAILABLE", "rules database unavailable"); 
                });
        }, {Get});

    // DELETE /api/v1/rules/{id}?user_id=1
    app().registerHandler("/api/v1/rules/{1}",
        [](const HttpRequestPtr &request, Callback &&done, long long id) {
            
            // Извлекаем user_id для проверки прав
            std::string userIdStr = request->getParameter("user_id");
            if (userIdStr.empty()) {
                error(done, k400BadRequest, "VALIDATION_ERROR", "user_id parameter is required for authorization");
                return;
            }
            long long userId = std::stoll(userIdStr);

            // Передаем userId вторым аргументом
            RuleRepository().remove(id, userId,
                [done](const Result &rows) {
                    if (rows.empty()) { 
                        error(done, k404NotFound, "NOT_FOUND", "rule not found or access denied"); 
                        return; 
                    }
                    done(HttpResponse::newHttpResponse(k204NoContent, CT_APPLICATION_JSON));
                },
                [done](const DrogonDbException &) { 
                    error(done, k503ServiceUnavailable, "SERVICE_UNAVAILABLE", "rules database unavailable"); 
                });
        }, {Delete});

    // GET /api/v1/users/{id}/notifications?page=1&limit=10
    app().registerHandler("/api/v1/users/{1}/notifications",
        [](const HttpRequestPtr &request, Callback &&done, long long id) {
            int page = 1;
            int limit = 10;
            if (!request->getParameter("page").empty()) page = std::stoi(request->getParameter("page"));
            if (!request->getParameter("limit").empty()) limit = std::stoi(request->getParameter("limit"));
            int offset = (page - 1) * limit;
            
            RuleRepository().notifications(id, limit, offset,
                [done, page, limit](const Result &rows) { sendList(rows, done, page, limit); },
                [done](const DrogonDbException &) { 
                    error(done, k503ServiceUnavailable, "SERVICE_UNAVAILABLE", "rules database unavailable"); 
                });
        }, {Get});
}