Модуль перенесён из iot-platform/iot-core, origin/feature/notifications-semyon,
коммит ef27b14: INotifier, Notification, TelegramNotifier, EmailNotifier,
EmailTemplate, а также services/NotificationService.h/.cpp.

Сохранены TgBot, long polling /start и /help, фоновая очередь,
SMTP/libcurl, MIME-письмо с текстовой и HTML-версиями.
Адаптация к iot2: дополнительные поля правила и изменившихся значений в
Notification и шаблонах. Команда /start с одноразовым параметром подтверждает
Telegram ID в личном чате при регистрации.
Вызов notify выполняется из RuleEngine только после срабатывания правила.
