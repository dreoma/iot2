#include "rule_routes.h"
#include "repositories/rule_repository.h"
#include "services/auth.h"

#include <cmath>
#include <set>

using namespace drogon;
using namespace drogon::orm;

namespace
{
using Callback = std::function<void(const HttpResponsePtr &)>;

void error(const Callback &done, HttpStatusCode status, const std::string &message)
{
    Json::Value body;
    body["error"] = message;
    auto response = HttpResponse::newHttpJsonResponse(body);
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

void withOwnedDevice(long long deviceId, long long userId, const Callback &done, std::function<void()> action)
{
    app().getDbClient("postgres")->execSqlAsync(
        "SELECT 1 FROM user_devices WHERE device_id=$1 AND user_id=$2",
        [done, action = std::move(action)](const Result &owner) {
            if (owner.empty())
                error(done, k404NotFound, "device not found");
            else
                action();
        },
        [done](const DrogonDbException &) { error(done, k503ServiceUnavailable, "main database unavailable"); },
        deviceId, userId);
}

void save(long long id, const HttpRequestPtr &request, Callback done)
{
    const auto body = request->getJsonObject();
    if (!body || !valid(*body))
    {
        error(done, k400BadRequest, "required: device_id, field, operator, threshold (except changed), message; optional: enabled");
        return;
    }
    const auto rule = *body;
    const auto userId = auth::userId(request);
    withOwnedDevice(rule["device_id"].asInt64(), userId, done, [rule, id, done, userId] {
        RuleRepository().save(id, userId, rule,
            [id, done](const Result &rows) {
                if (rows.empty()) { error(done, k404NotFound, "rule not found"); return; }
                auto response = HttpResponse::newHttpJsonResponse(ruleJson(rows[0]));
                response->setStatusCode(id == 0 ? k201Created : k200OK);
                done(response);
            },
            [done](const DrogonDbException &) { error(done, k503ServiceUnavailable, "rules database unavailable or invalid rule"); });
    });
}

void sendList(const Result &rows, const Callback &done)
{
    Json::Value values(Json::arrayValue);
    for (const auto &row : rows) values.append(ruleJson(row));
    done(HttpResponse::newHttpJsonResponse(values));
}
}  // namespace

void registerRuleRoutes()
{
    app().registerHandler("/rules",
        [](const HttpRequestPtr &request, Callback &&done) { save(0, request, done); }, {Post, "UserAuthFilter"});
    app().registerHandler("/rules/{1}",
        [](const HttpRequestPtr &request, Callback &&done, long long id) {
            if (id <= 0) { error(done, k400BadRequest, "positive rule id required"); return; }
            save(id, request, done);
        }, {Put, "UserAuthFilter"});
    app().registerHandler("/devices/{1}/rules",
        [](const HttpRequestPtr &request, Callback &&done, long long id) {
            const auto userId = auth::userId(request);
            withOwnedDevice(id, userId, done, [id, userId, done] {
                RuleRepository().list(id, userId,
                    [done](const Result &rows) { sendList(rows, done); },
                    [done](const DrogonDbException &) { error(done, k503ServiceUnavailable, "rules database unavailable"); });
            });
        }, {Get, "UserAuthFilter"});
    app().registerHandler("/rules/{1}",
        [](const HttpRequestPtr &request, Callback &&done, long long id) {
            RuleRepository().remove(id, auth::userId(request),
                [done](const Result &rows) {
                    if (rows.empty()) { error(done, k404NotFound, "rule not found"); return; }
                    done(HttpResponse::newHttpResponse(k204NoContent, CT_APPLICATION_JSON));
                },
                [done](const DrogonDbException &) { error(done, k503ServiceUnavailable, "rules database unavailable"); });
        }, {Delete, "UserAuthFilter"});
    app().registerHandler("/users/{1}/notifications",
        [](const HttpRequestPtr &request, Callback &&done, long long id) {
            if (id != auth::userId(request))
                return error(done, k404NotFound, "user not found");
            RuleRepository().notifications(id,
                [done](const Result &rows) { sendList(rows, done); },
                [done](const DrogonDbException &) { error(done, k503ServiceUnavailable, "rules database unavailable"); });
        }, {Get, "UserAuthFilter"});
}
