#pragma once

// Регистрирует все HTTP-маршруты устройств.
#include <memory>
class NotificationService;
void registerDeviceRoutes(std::shared_ptr<NotificationService> notifications);
