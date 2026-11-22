# iot-core

Минимальный C++ сервис: Drogon, PostgreSQL и ORM-клиент Drogon.

Схема в `db/init.sql` состоит из `users`, `devices`, `user_devices` и
`device_telemetry`. `user_devices` связывает пользователя с устройствами, а
`device_telemetry` хранит измерения устройств.

## Сборка сервиса

Для ручной сборки понадобятся Drogon с PostgreSQL, OpenSSL, libcurl, Boost, TgBot,
libsodium и pkg-config. Docker установит их автоматически.

Перед запуском примените SQL-миграции из `db/` к соответствующим базам. Для
основной базы порядок такой: `init.sql` для новой базы, затем `telemetry_context.sql`,
`user_auth.sql` и `telegram_registration.sql`. Для базы правил: `rules.sql` для новой базы, затем
`notification_context.sql`. Docker Compose применяет их при запуске.

```bash
cmake -S . -B build
cmake --build build
./build/iot-core
```

## Вход и доступ к API

Для регистрации задайте `IOT_TELEGRAM_BOT_TOKEN` в `.env`. Ядро получает имя бота
от Telegram. При недоступности бота старт регистрации возвращает `503`.

1. `POST /auth/register/start` принимает `mail` и возвращает `201` с полями
   `registration_token`, `telegram_url` и `expires_in` (600 секунд).
2. Пользователь открывает `telegram_url` и нажимает «Старт» в личном чате.
   Бот связывает Telegram ID отправителя с начатой регистрацией. Клиент
   проверяет `GET /auth/register/status` с `Authorization: Bearer <registration_token>`.
   Ответ содержит `telegram_confirmed`.
3. После подтверждения `POST /users` принимает только `password` и тот же
   регистрационный токен в заголовке. Ответ `201` содержит профиль и ID
   пользователя. Токен регистрации используется один раз. Для входа после
   регистрации вызовите обычный `POST /auth/login`.

Ссылка бота и токен регистрации содержат разные случайные значения. PostgreSQL
временно хранит их SHA-256 в `pending_registrations`. Завершить регистрацию
можно только в течение 10 минут. Ручной ввод Telegram ID отсутствует и
в регистрации, и при изменении профиля.

Пароль должен занимать от 15 до 256 байт. Сервер сначала вычисляет HMAC-SHA-256
от пароля с секретом `IOT_PASSWORD_PEPPER_HEX` из `.env`, затем хеширует результат
с уникальной солью через Argon2id. В PostgreSQL хранится итоговый хеш.
Пароли, сохранённые без перца, обновляются при первом успешном входе.
Сохраните перец при переносе или восстановлении базы.

`POST /auth/login` принимает `mail` и `password` и возвращает
`token`, `user_id`, `expires_in`. Email указывается как при регистрации.
Отправляйте токен в заголовке `Authorization: Bearer <token>` по HTTPS.
Сессия действует 24 часа. Повторный вход заменяет текущую сессию.

| Маршрут | Действие |
| --- | --- |
| `GET /users` | Вернуть массив с собственным профилем |
| `GET /users/{id}`, `PUT /users/{id}`, `DELETE /users/{id}` | Прочитать свой профиль, изменить его email (поле `mail`) или удалить профиль |
| `POST /devices`, `GET /devices` | Создать устройство для себя или получить список своих устройств |
| `GET /devices/{id}`, `PUT /devices/{id}`, `DELETE /devices/{id}` | Прочитать, обновить или удалить своё устройство |
| `POST /auth/logout` | Завершить текущую сессию |
| `PUT /auth/password` | Изменить пароль с полями `current_password` и `new_password`. После смены нужно войти заново |

API определяет владельца устройства по сессии. Для правил и истории уведомлений
тоже нужен токен владельца. Без действующей сессии ответ будет `401`, для чужого
ресурса — `404`. Маршрут приёма телеметрии использует ключ обмена с адаптером.

Пример для локального Docker Compose. Пароль в примере тестовый.

```bash
core() {
  curl --noproxy '*' --cacert certs/ca.crt --resolve core-tls:8443:127.0.0.1 "$@"
}

core https://core-tls:8443/auth/register/start -H 'Content-Type: application/json' \
  -d '{"mail":"user@example.com"}'

# Сохраните registration_token в REG_TOKEN, откройте telegram_url и нажмите «Старт».
core https://core-tls:8443/auth/register/status -H "Authorization: Bearer $REG_TOKEN"
# После telegram_confirmed: true задайте пароль.
core https://core-tls:8443/users -H "Authorization: Bearer $REG_TOKEN" \
  -H 'Content-Type: application/json' -d '{"password":"Example login password"}'

core https://core-tls:8443/auth/login -H 'Content-Type: application/json' \
  -d '{"mail":"user@example.com","password":"Example login password"}'

# Подставьте token из ответа входа.
core https://core-tls:8443/devices -H 'Authorization: Bearer <token>' \
  -H 'Content-Type: application/json' \
  -d '{"name":"esp32","notifications_enabled":true}'
```

## Пароли для существующих пользователей

Миграция `user_auth.sql` сохраняет пользователей, устройства и историю. Старые
учётные записи получают доступ после установки пароля оператором. Для каждой
такой записи получите Argon2id-хеш пароля через стандартный ввод в Bash:

```bash
read -r -s -p 'Password: ' IOT_EXISTING_PASSWORD
printf '\n'
printf '%s\n' "$IOT_EXISTING_PASSWORD" | docker compose exec -T iot-core ./iot-core --hash-password
unset IOT_EXISTING_PASSWORD
```

Выполните в `docker compose exec db psql -U iot_user -d iot_core`. Укажите
ID и сохраните полученный хеш одним запросом:

```sql
\prompt 'User ID: ' target_user_id
\prompt 'Argon2id hash: ' target_hash
UPDATE users SET password_hash = :'target_hash'
WHERE id = :'target_user_id'::bigint AND password_hash IS NULL;
```

`UPDATE 1` означает, что пароль установлен. Передайте пароль пользователю через
приватный канал. После первого входа пользователь может сменить его.
