#include "rule_engine.h"
#include "repositories/rule_repository.h"

#include <cmath>

using namespace drogon::orm;

namespace
{
bool compare(double value, double threshold, const std::string &op)
{
    if (op == "gt") return value > threshold;
    if (op == "gte") return value >= threshold;
    if (op == "lt") return value < threshold;
    if (op == "lte") return value <= threshold;
    if (op == "eq") return value == threshold;
    if (op == "ne") return value != threshold;
    return false;
}
}  // namespace

bool RuleEngine::matches(const Json::Value &rule,
                          const Json::Value &previous, const Json::Value &current)
{
    const auto field = rule["field"].asString();
    const auto op = rule["operator"].asString();
    if (!current.isMember(field) || current[field].isNull()) return false;
    const bool hasPrevious = previous.isMember(field) && !previous[field].isNull();
    if (op == "changed") return hasPrevious && previous[field] != current[field];
    if (!current[field].isNumeric() || !std::isfinite(current[field].asDouble())) return false;
    const double threshold = rule["threshold"].asDouble();
    const bool numericPrevious = hasPrevious && previous[field].isNumeric();
    if (op == "delta")
        return numericPrevious &&
               std::abs(current[field].asDouble() - previous[field].asDouble()) >= threshold;

    // Пороговое правило срабатывает при входе в условие, а не на каждом пакете.
    return compare(current[field].asDouble(), threshold, op) &&
           (!numericPrevious || !compare(previous[field].asDouble(), threshold, op));
}

void RuleEngine::evaluate(const Json::Value &stored, Done done) const
{
    if (!stored["notifications_enabled"].asBool() || stored["user_id"].isNull())
    {
        done(true, 0);
        return;
    }
    RuleRepository repository;
    auto notifications = notifications_;
    repository.active(stored["device_id"].asInt64(), stored["user_id"].asInt64(),
        [stored, done, notifications](const Result &rules) {
            Json::Value pending(Json::arrayValue);
            std::vector<Notification> messages;
            for (const auto &row : rules)
            {
                const auto rule = ruleJson(row);
                if (!RuleEngine::matches(rule, stored["previous_data"], stored["data"])) continue;
                const auto field = rule["field"].asString();
                Json::Value notification;
                notification["rule_id"] = rule["id"];
                notification["telemetry_id"] = stored["id"];
                notification["device_id"] = stored["device_id"];
                notification["user_id"] = stored["user_id"];
                notification["telegram_id"] = stored["telegram_id"];
                notification["message"] = rule["message"];
                notification["previous_value"] = stored["previous_data"][field];
                notification["current_value"] = stored["data"][field];
                Json::Value context;
                context["occurred_at"] = stored["received_at"];
                context["device"]["id"] = stored["device_id"];
                context["device"]["name"] = stored["device_name"];
                context["user"]["id"] = stored["user_id"];
                context["user"]["mail"] = stored["mail"];
                context["user"]["telegram_id"] = stored["telegram_id"];
                context["rule"] = rule;
                context["telemetry"]["id"] = stored["id"];
                context["telemetry"]["data"] = stored["data"];
                context["telemetry"]["previous_data"] = stored["previous_data"];
                notification["context"] = context;
                pending.append(notification);
                Notification message;
                message.userId = stored["user_id"].asInt64();
                message.deviceId = stored["device_id"].asInt64();
                message.telemetryId = stored["id"].asInt64();
                message.ruleId = rule["id"].asInt64();
                message.condition = field + " " + rule["operator"].asString();
                if (rule["operator"] != "changed")
                    message.condition += " " + Json::writeString(Json::StreamWriterBuilder(), rule["threshold"]);
                message.sensorId = std::to_string(stored["device_id"].asInt64());
                message.deviceName = stored["device_name"].asString();
                message.message = rule["message"].asString();
                message.field = field;
                message.previousValue = Json::writeString(Json::StreamWriterBuilder(), stored["previous_data"][field]);
                message.currentValue = Json::writeString(Json::StreamWriterBuilder(), stored["data"][field]);
                if (stored["data"][field].isNumeric()) message.value = stored["data"][field].asDouble();
                message.time = stored["received_at"].asString();
                message.email = stored["mail"].asString();
                message.userName = message.email; // В iot2 у пользователя нет отдельного имени.
                message.telegramChatId = std::to_string(stored["telegram_id"].asInt64());
                messages.push_back(std::move(message));
            }
            if (pending.empty()) { done(true, 0); return; }
            RuleRepository().enqueue(pending,
                [done, notifications, messages](const Result &rows) {
                    // Отправляем только после сохранения истории срабатывания.
                    for (const auto &message : messages) notifications->notify(message);
                    done(true, static_cast<int>(rows.size()));
                },
                [done](const DrogonDbException &) { done(false, 0); });
        },
        [done](const DrogonDbException &) { done(false, 0); });
}
