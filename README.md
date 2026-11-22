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
# Сохраните существующие настройки, если .env уже есть.
test -f .env || cp .env.example .env
openssl rand -hex 32
# Для нового окружения вставьте результат в TRAFFIC_KEY_HEX файла .env.
openssl rand -hex 32
# Вставьте отдельный результат в IOT_PASSWORD_PEPPER_HEX, если он ещё не задан.
docker compose up --build
```

Для запуска в фоне: `docker compose up --build -d`.
Проверка состояния: `docker compose ps`. Просмотр логов: `docker compose logs -f`.

Веб-интерфейс IoT Studio доступен на https://localhost:8443/app/.
В нём можно зарегистрироваться через Telegram-бота, добавить устройство
и смотреть телеметрию. Для регистрации задайте `IOT_TELEGRAM_BOT_TOKEN` в `.env`.
Для браузера нужен доверенный локальный CA. Настройка сертификата и запуск
Vue в режиме разработки описаны в [front/README.md](front/README.md).

Для интеграционной проверки регистрации, устройств и телеметрии используйте
[локальный стенд с заглушкой Telegram и Mailpit](LOCAL_SETUP.md#8-проверить-тестами-без-реальных-рассылок).

После запуска устройства отправляют телеметрию в адаптер:

```bash
curl -X POST http://127.0.0.1:8081/telemetry \
  -H 'Content-Type: application/json' \
  -d '{"device_id":1,"data":{"temperature":22.5}}'
```

Перед отправкой [зарегистрируйте пользователя, войдите и создайте устройство через HTTPS API](iot-core/README.md#вход-и-доступ-к-api).

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
