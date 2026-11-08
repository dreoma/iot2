# iot-core

Минимальный C++ сервис: Drogon, PostgreSQL и ORM-клиент Drogon.

Схема в `db/init.sql` состоит из `users`, `devices`, `user_devices` и
`device_telemetry`. `user_devices` связывает пользователя с устройствами, а
`device_telemetry` хранит измерения устройств.

## Сборка сервиса

Установите Drogon (например, через vcpkg), затем:

```bash
cmake -S . -B build
cmake --build build
./build/iot-core
```

## Маршруты

- `POST /users` — создать пользователя;
- `GET /users` — получить всех пользователей;
- `GET /users/{id}` — получить пользователя по идентификатору;
- `PUT /users/{id}` — изменить пользователя;
- `DELETE /users/{id}` — удалить пользователя.

Для устройств доступны аналогичные маршруты: `POST /devices`, `GET /devices`,
`GET /devices/{id}`, `PUT /devices/{id}`, `DELETE /devices/{id}`.

Маршруты находятся в `src/routes/user_routes.cpp`, а все обращения к БД — в
`src/repositories/user_repository.cpp`. Для добавления нового маршрута сначала
добавьте метод в репозиторий, затем зарегистрируйте HTTP-обработчик в routes.

## Создание пользователя

```bash
curl -X POST http://127.0.0.1:8080/users \
  -H 'Content-Type: application/json' \
  -d '{"mail": "user@example.com", "telegram_id": 123456789}'
```

Успешный ответ: `201 Created`. Повторяющийся `telegram_id` или `mail`: `409 Conflict`.

## Примеры GET-запросов

```bash
curl http://127.0.0.1:8080/users
curl http://127.0.0.1:8080/users/1
curl -X PUT http://127.0.0.1:8080/users/1 \
  -H 'Content-Type: application/json' \
  -d '{"mail":"new@example.com","telegram_id":987654321}'
curl -X DELETE http://127.0.0.1:8080/users/1
```

## Примеры устройств

```bash
curl -X POST http://127.0.0.1:8080/devices \
  -H 'Content-Type: application/json' \
  -d '{"name":"esp32-office","user_id":1,"notifications_enabled":true}'

curl http://127.0.0.1:8080/devices/1

curl -X PUT http://127.0.0.1:8080/devices/1 \
  -H 'Content-Type: application/json' \
  -d '{"name":"esp32-home","user_id":1,"notifications_enabled":false}'

curl -X DELETE http://127.0.0.1:8080/devices/1
```
