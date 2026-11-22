#pragma once

#include <functional>
#include <string>

// Регистрирует все маршруты, связанные с пользователями.
void registerUserRoutes(std::function<std::string()> telegramUsername);
