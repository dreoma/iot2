
#pragma once
#include <string>
#include <cstdint>

struct Notification {
    int64_t userId = 0;
    int64_t deviceId = 0;
    int64_t telemetryId = 0;
    int64_t ruleId = 0;
    std::string condition;
    // Дополнительные данные сработавшего правила.
    std::string message;
    std::string field;
    std::string previousValue;
    std::string currentValue;
    // что произошло
    std::string sensorId;
    std::string deviceName;
    double value = 0;
    std::string unit;
    std::string time;          // строка с временем измерения
    // кому
    std::string userName;
    std::string email;
    std::string telegramChatId;
};

class INotifier {
public:
    virtual ~INotifier() = default;
    virtual bool send(const Notification& n) = 0;
    virtual const char* name() const = 0;
};
