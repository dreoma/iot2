
#pragma once
#include <sstream>
#include <string>
#include "INotifier.h"

namespace email_tpl {

inline std::string esc(const std::string& s) {
    std::string o;
    for (char c : s) {
        switch (c) {
            case '&': o += "&amp;"; break;
            case '<': o += "&lt;"; break;
            case '>': o += "&gt;"; break;
            case '"': o += "&quot;"; break;
            default: o += c;
        }
    }
    return o;
}

inline std::string subject(const Notification& n) {
    return "IoT: сработало правило — " + n.deviceName;
}

inline std::string text(const Notification& n) {
    std::ostringstream o;
    o << "Сработало правило\n" << n.message << "\n"
      << "Устройство: " << n.deviceName << " (ID " << n.deviceId << ")\n"
      << "Измерение: #" << n.telemetryId << "\n"
      << "Правило: #" << n.ruleId << ", " << n.condition << "\n"
      << "Поле: " << n.field << "\n"
      << "Изменение: " << n.previousValue << " → " << n.currentValue << "\n"
      << "Владелец: #" << n.userId << ", " << n.email << "\n"
      << "Время события: " << n.time << "\n";
    return o.str();
}

// Почтовые клиенты не любят <style> и flex, поэтому: таблицы + inline-стили, ширина 600px.
inline std::string html(const Notification& n) {
    std::ostringstream v;
    v << n.currentValue;
    auto row = [](const std::string& k, const std::string& val) {
        return "<tr><td style=\"padding:10px 0;color:#6b7280;font-size:14px;border-bottom:1px solid #eef0f3;\">" + k +
               "</td><td style=\"padding:10px 0;color:#111827;font-size:14px;font-weight:600;text-align:right;border-bottom:1px solid #eef0f3;\">" +
               val + "</td></tr>";
    };
    std::string s;
    s += "<!DOCTYPE html><html lang=\"ru\"><head><meta charset=\"UTF-8\">"
         "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"></head>"
         "<body style=\"margin:0;padding:0;background:#f3f4f6;font-family:Arial,Helvetica,sans-serif;\">"
         "<table role=\"presentation\" width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" style=\"background:#f3f4f6;padding:24px 0;\"><tr><td align=\"center\">"
         "<table role=\"presentation\" width=\"600\" cellpadding=\"0\" cellspacing=\"0\" style=\"max-width:600px;width:100%;background:#ffffff;border-radius:8px;overflow:hidden;\">"
         // шапка
         "<tr><td style=\"background:#2563eb;padding:20px 28px;color:#ffffff;font-size:20px;font-weight:700;\">IoT Platform</td></tr>"
         // приветствие
         "<tr><td style=\"padding:28px 28px 8px 28px;color:#111827;font-size:16px;\">Здравствуйте, " + esc(n.userName) +
         "!<br><span style=\"color:#6b7280;font-size:14px;\">" + esc(n.message) + "</span></td></tr>"
         // большое значение
         "<tr><td align=\"center\" style=\"padding:16px 28px;\"><div style=\"background:#eff6ff;border-radius:8px;padding:20px;\">"
         "<div style=\"font-size:36px;font-weight:700;color:#1d4ed8;\">" + esc(v.str()) + " " + esc(n.unit) + "</div>"
         "<div style=\"font-size:13px;color:#6b7280;margin-top:4px;\">" + esc(n.deviceName) + "</div></div></td></tr>"
         // детали
         "<tr><td style=\"padding:8px 28px 24px 28px;\"><table role=\"presentation\" width=\"100%\" cellpadding=\"0\" cellspacing=\"0\">";
    s += row("Устройство", esc(n.deviceName));
    s += row("ID устройства", std::to_string(n.deviceId));
    s += row("ID измерения", std::to_string(n.telemetryId));
    s += row("ID правила", std::to_string(n.ruleId));
    s += row("Условие", esc(n.condition));
    s += row("ID владельца", std::to_string(n.userId));
    s += row("Почта владельца", esc(n.email));
    s += row("Поле", esc(n.field));
    s += row("Предыдущее значение", esc(n.previousValue));
    s += row("Значение", esc(v.str()) + " " + esc(n.unit));
    s += row("Время", esc(n.time));
    s += "</table></td></tr>"
         // подвал
         "<tr><td style=\"background:#f9fafb;padding:16px 28px;color:#9ca3af;font-size:12px;\">"
         "Это автоматическое уведомление. Отключить его можно, выставив notifications_enabled=false у устройства."
         "</td></tr></table></td></tr></table></body></html>";
    return s;
}

}  // namespace email_tpl
