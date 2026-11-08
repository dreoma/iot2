# IoT services

Подробная установка зависимостей и локальный запуск на macOS —
[LOCAL_SETUP.md](LOCAL_SETUP.md).

Rule engine и примеры API правил описаны в [iot-core/RULES.md](iot-core/RULES.md).

На macOS нужен запущенный Docker Desktop. Drogon, OpenSSL и остальные C++
библиотеки устанавливаются внутри образов; на Mac отдельно их ставить не нужно.

## Запуск всех контейнеров

Сначала один раз создайте локальные TLS-сертификаты:

```bash
chmod +x scripts/generate-certs.sh
./scripts/generate-certs.sh
cp .env.example .env
openssl rand -hex 32
# Вставьте результат в TRAFFIC_KEY_HEX файла .env.
docker compose up --build
```

Для запуска в фоне: `docker compose up --build -d`.
Проверка состояния: `docker compose ps`. Просмотр логов: `docker compose logs -f`.

После запуска можно проверить CRUD пользователей/устройств и шифрованную
передачу телеметрии командой `python3 tests/integration.py`. Проверка создаёт
свои тестовые записи и удаляет их по завершении.

После запуска устройства отправляют телеметрию в адаптер:

```bash
curl -X POST http://127.0.0.1:8081/telemetry \
  -H 'Content-Type: application/json' \
  -d '{"device_id":1,"data":{"temperature":22.5}}'
```

Перед отправкой создайте пользователя и устройство через HTTPS API `iot-core`.
Сертификат выдан для имени `core-tls`, поэтому при локальном запросе укажите
это имя через `--resolve`:

```bash
curl --cacert certs/ca.crt --resolve core-tls:8443:127.0.0.1 \
  -X POST https://core-tls:8443/users \
  -H 'Content-Type: application/json' \
  -d '{"mail":"user@example.com","telegram_id":123456789}'

curl --cacert certs/ca.crt --resolve core-tls:8443:127.0.0.1 \
  -X POST https://core-tls:8443/devices \
  -H 'Content-Type: application/json' \
  -d '{"name":"esp32","user_id":1,"notifications_enabled":true}'
```

## Как работает защита трафика

`http-adapter` сначала сериализует `device_id` и `data` и шифрует их классом
`Encoder` алгоритмом AES-256-GCM. В запрос передаётся только Base64-пакет из
случайного nonce, authentication tag и шифротекста. В `iot-core` класс `Decoder`
проверяет tag и только после этого расшифровывает данные. Подмена либо изменение
данных не пройдут проверку GCM.

Дополнительно адаптер пересылает данные только по `https://core-tls:8443`.
Контейнер `core-tls` является TLS-входом перед `iot-core`. Скрипт создаёт
локальный CA и сертификат для имени `core-tls`; libcurl проверяет и CA, и имя
сервера. Итого AES-GCM защищает payload, TLS защищает весь канал между
адаптером и HTTPS-входом `iot-core`.

Для production храните `TRAFFIC_KEY_HEX` в менеджере секретов, периодически
меняйте его и замените локальный CA и сертификат на сертификаты вашей PKI или
публичного центра сертификации. Приватные ключи не храните в репозитории.
