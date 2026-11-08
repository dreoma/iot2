#include "rule_repository.h"

#include <sstream>
#include <utility>

using namespace drogon;
using namespace drogon::orm;

Json::Value ruleJson(const Row &row)
{
    Json::Value result;
    Json::CharReaderBuilder reader;
    std::string errors;
    std::istringstream stream(row["body"].as<std::string>());
    if (!Json::parseFromStream(reader, stream, &result, &errors))
        throw std::runtime_error("invalid database JSON");
    return result;
}

void RuleRepository::save(long long id, long long userId, const Json::Value &rule,
                          Success done, Failure fail) const
{
    // id=0 создаёт правило; PUT заменяет все его настройки.
    const std::string sql = id == 0
        ? "WITH saved AS (INSERT INTO rules(device_id,user_id,field,operator,threshold,message,enabled) "
          "VALUES($1,$2,$3,$4,$5,$6,$7) RETURNING *) SELECT to_jsonb(saved) AS body FROM saved"
        : "WITH saved AS (UPDATE rules SET device_id=$1,user_id=$2,field=$3,operator=$4,"
          "threshold=$5,message=$6,enabled=$7 WHERE id=$8 RETURNING *) "
          "SELECT to_jsonb(saved) AS body FROM saved";
    auto db = app().getDbClient("rules");
    if (id == 0)
        db->execSqlAsync(sql, std::move(done), std::move(fail),
            rule["device_id"].asInt64(), userId, rule["field"].asString(),
            rule["operator"].asString(), rule.get("threshold", 0).asDouble(),
            rule["message"].asString(), rule.get("enabled", true).asBool());
    else
        db->execSqlAsync(sql, std::move(done), std::move(fail),
            rule["device_id"].asInt64(), userId, rule["field"].asString(),
            rule["operator"].asString(), rule.get("threshold", 0).asDouble(),
            rule["message"].asString(), rule.get("enabled", true).asBool(), id);
}

void RuleRepository::list(long long deviceId, Success done, Failure fail) const
{
    app().getDbClient("rules")->execSqlAsync(
        "SELECT to_jsonb(r) AS body FROM rules r WHERE device_id=$1 ORDER BY id",
        std::move(done), std::move(fail), deviceId);
}

void RuleRepository::remove(long long id, Success done, Failure fail) const
{
    app().getDbClient("rules")->execSqlAsync("DELETE FROM rules WHERE id=$1 RETURNING id",
        std::move(done), std::move(fail), id);
}

void RuleRepository::active(long long deviceId, long long userId, Success done, Failure fail) const
{
    app().getDbClient("rules")->execSqlAsync(
        "SELECT to_jsonb(r) AS body FROM rules r "
        "WHERE device_id=$1 AND user_id=$2 AND enabled ORDER BY id",
        std::move(done), std::move(fail), deviceId, userId);
}

void RuleRepository::enqueue(const Json::Value &notifications, Success done, Failure fail) const
{
    // Одна атомарная вставка для всех сработавших правил измерения.
    const auto json = Json::writeString(Json::StreamWriterBuilder(), notifications);
    app().getDbClient("rules")->execSqlAsync(R"SQL(
        INSERT INTO notifications(rule_id,telemetry_id,device_id,user_id,telegram_id,
                                  message,previous_value,current_value,context)
        SELECT rule_id,telemetry_id,device_id,user_id,telegram_id,
               message,previous_value,current_value,context
        FROM jsonb_to_recordset($1::jsonb) AS n(
            rule_id bigint,telemetry_id bigint,device_id bigint,user_id bigint,
            telegram_id bigint,message text,previous_value jsonb,current_value jsonb,context jsonb)
        ON CONFLICT (rule_id,telemetry_id) DO NOTHING RETURNING id
    )SQL", std::move(done), std::move(fail), json);
}

void RuleRepository::notifications(long long userId, Success done, Failure fail) const
{
    app().getDbClient("rules")->execSqlAsync(
        "SELECT to_jsonb(n) AS body FROM notifications n WHERE user_id=$1 ORDER BY id DESC LIMIT 100",
        std::move(done), std::move(fail), userId);
}
