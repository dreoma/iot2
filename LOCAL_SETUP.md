# Установка зависимостей и локальный запуск

Инструкция для macOS, включая Apple Silicon. Все команды выполняются из корня
`iot2`, где лежит `docker-compose.yml`.

## 1. Что установить на Mac

- [Docker Desktop](https://docs.docker.com/desktop/setup/install/mac-install/) —
  выберите версию для Apple Silicon или Intel. После установки запустите приложение.
- Git — для получения проекта. На macOS можно установить вместе с Command Line Tools:
  `xcode-select --install` (если инструменты уже установлены, повторять не нужно).
- Python 3 — только для интеграционных тестов. Проверьте `python3 --version`.
  Если его нет, установите с [python.org](https://www.python.org/downloads/macos/)
  или командой `brew install python`, если уже используете Homebrew.
- Postman — необязательно, для ручного тестирования API.

Проверка Docker:

```bash
docker --version
docker compose version
docker info
```

Если `docker info` сообщает, что daemon недоступен, откройте Docker Desktop
и дождитесь завершения запуска.

## 2. Какие зависимости использует проект

При запуске через Docker они устанавливаются автоматически внутри Linux-образов.
На Mac устанавливать Drogon, TgBot и PostgreSQL отдельно не требуется.

| Зависимость | Назначение |
| --- | --- |
| C++20, GCC, CMake >= 3.20 | Сборка обоих сервисов |
| Drogon | HTTP API и клиент PostgreSQL |
| PostgreSQL 16 / libpq | Данные пользователей, устройств, телеметрия и правила |
| OpenSSL | AES-256-GCM и TLS |
| libcurl | HTTPS-запросы адаптера и отправка Email по SMTP |
| tgbot-cpp / TgBot | Telegram, команды `/start` и `/help` |
| Boost.System, nlohmann-json | Зависимости TgBot |
| Jsoncpp | JSON в API |
| Nginx | HTTPS-вход перед iot-core |
| Mailpit | Локальная проверка Email без настоящего SMTP |

Точный набор пакетов для сборки находится в `iot-core/Dockerfile` и
`http-adapter/Dockerfile`. В Ubuntu системный Drogon также требует dev-пакеты
UUID, Zlib, SQLite, MariaDB, Brotli, Hiredis, yaml-cpp и c-ares. Это зависимости
установленного пакета Drogon; данные приложения хранятся в PostgreSQL.

TgBot собирается из официального репозитория
[tgbot-cpp](https://github.com/reo7sp/tgbot-cpp), версия зафиксирована коммитом
в `iot-core/Dockerfile`. При первой сборке нужен доступ в интернет.

## 3. Подготовить окружение

```bash
cd /Users/dreoma/Documents/iot2
```

Если проект расположен в другой папке, используйте свой путь.

Только если `.env` ещё нет, скопируйте шаблон:

```bash
test -f .env || cp .env.example .env
```

Существующий `.env` не заменяйте: там могут быть рабочий ключ шифрования и токен.

Для нового окружения выполните команду дважды, чтобы получить два независимых ключа:

```bash
openssl rand -hex 32
```

Вставьте первый ключ в `TRAFFIC_KEY_HEX`, второй в `IOT_PASSWORD_PEPPER_HEX`:

```dotenv
TRAFFIC_KEY_HEX=сюда_64_шестнадцатеричных_символа
IOT_PASSWORD_PEPPER_HEX=сюда_другие_64_шестнадцатеричных_символа
IOT_TELEGRAM_BOT_TOKEN=
IOT_SMTP_URL=smtp://mailpit:1025
IOT_SMTP_USER=
IOT_SMTP_PASSWORD=
IOT_SMTP_FROM=iot@example.com
```

Если ключ телеметрии уже задан, сохраните его и добавьте только перец
`IOT_PASSWORD_PEPPER_HEX`. Compose передаёт перец только ядру.
`.env` исключён из Git. Сохраните перец вместе с резервной копией настроек
отдельно от БД. При потере перца пароли с ним придётся переустановить.

## 4. Подготовить HTTPS-сертификаты

```bash
sh scripts/generate-certs.sh
openssl verify -CAfile certs/ca.crt certs/server.crt
```

Ожидаемый результат проверки: `certs/server.crt: OK`.
Скрипт создаёт локальный CA и сертификат для имени `core-tls`; при наличии
сертификатов пропускает генерацию. Не добавляйте приватные ключи в Git.

## 5. Собрать и запустить все сервисы

```bash
docker compose up --build -d
docker compose ps -a
```

Первая сборка может занять несколько минут: скачиваются образы, устанавливаются
библиотеки и собирается TgBot. Последующие сборки используют кэш.

Работать должны `db`, `rules-db`, `iot-core`, `core-tls`, `http-adapter`, `mailpit`.
У баз и Mailpit ожидается `healthy`. Сервис `db-migrate` должен завершиться с
`Exited (0)` — это нормально: он один раз применяет повторяемые миграции перед
запуском ядра. Данные существующих баз при этом не удаляются.

| Сервис | Адрес на Mac |
| --- | --- |
| API iot-core через Nginx | `https://localhost:8443` |
| HTTP-адаптер устройств | `http://localhost:8081` |
| Просмотр писем Mailpit | `http://localhost:8025` |

PostgreSQL доступен только внутри Docker-сети. Основная база называется
`iot_core`, база правил — `iot_rules`. Данные сохраняются в Docker volumes.

## 6. Проверить API

Сертификат выдан для `core-tls`, поэтому для проверки curl без отключения TLS
используйте `--resolve` и `--cacert`:

```bash
curl --noproxy '*' --cacert certs/ca.crt \
  --resolve core-tls:8443:127.0.0.1 \
  -i https://core-tls:8443/users
```

Без токена ожидается `401 Unauthorized`. [Регистрация, вход и примеры запросов](iot-core/README.md#вход-и-доступ-к-api).

В Postman импортируйте:

- `postman/iot2.postman_collection.json` — полный сценарий с тестовыми данными;
- `postman/notifications.postman_collection.json` — уведомления на ваши контакты.

Для второй коллекции заполните `notify_mail` и `notify_password` во вкладке Variables. Подробности — в `postman/NOTIFICATIONS.md`.

Телеметрия отправляется в адаптер как обычный JSON:

```bash
curl --noproxy '*' -X POST http://localhost:8081/telemetry \
  -H 'Content-Type: application/json' \
  -d '{"device_id":1,"data":{"temperature":35}}'
```

Сначала создайте пользователя, его устройство и правило через API. Вместо `1`
используйте реальный ID устройства. Адаптер сам зашифрует пакет для ядра.

## 7. Настроить уведомления

Для Telegram укажите `IOT_TELEGRAM_BOT_TOKEN` от BotFather в `.env`. При
регистрации сайт выдаёт ссылку на личный чат с ботом. После нажатия «Старт»
ядро само получает Telegram ID и сохраняет его при создании аккаунта.
С пустым токеном новые регистрации через Telegram недоступны. Уже
зарегистрированные пользователи могут входить по почте и паролю.

По умолчанию Email отправляется в Mailpit, даже если указан настоящий адрес
получателя. Для внешней почты задайте свой SMTP:

```dotenv
IOT_SMTP_URL=smtps://smtp.example.com:465
IOT_SMTP_USER=ваш_логин
IOT_SMTP_PASSWORD=пароль_приложения
IOT_SMTP_FROM=адрес_отправителя
```

После изменения `.env` пересоздайте ядро, чтобы оно прочитало новые переменные:

```bash
docker compose up -d --force-recreate iot-core
docker compose restart core-tls
```

Правила, их API и формат события описаны в `iot-core/RULES.md`.

## 8. Проверить тестами без реальных рассылок

Тестовый override направляет Telegram в локальную заглушку, а Email — в Mailpit.
Стенд слушает отдельные локальные порты: `18443`, `18081`, `18025` и `18085`.

```bash
docker compose -p iot2-test -f docker-compose.yml -f tests/compose.local.yml up --build -d --wait
COMPOSE_PROJECT_NAME=iot2-test CHECK_EMAIL=1 python3 tests/integration.py
docker compose -p iot2-test -f docker-compose.yml -f tests/compose.local.yml down
```

Тест создаёт своих пользователей, устройства и правила, проверяет телеметрию,
связь с владельцем, снимки событий и доставку писем, затем удаляет свои записи
из баз. Тестовые письма остаются в локальном Mailpit.

Чтобы после проверки запустить обычный проект с настройками из `.env`, выполните:

```bash
docker compose up -d --no-build
```

## 9. Остановка, повторный запуск и диагностика

```bash
# Остановить, сохранив контейнеры и данные
docker compose stop

# Запустить снова в фоне
docker compose up -d

# Пересобрать после изменения C++ кода
docker compose up --build -d
docker compose restart core-tls

# Проверить состояние и последние логи
docker compose ps -a
docker compose logs --tail=100 iot-core http-adapter core-tls
```

`docker compose down` удаляет контейнеры и сеть, но сохраняет данные в volumes.
Не добавляйте `-v`, если данные нужны: этот флаг удаляет volumes баз.

Частые причины ошибок:

- `Cannot connect to Docker daemon` — Docker Desktop ещё не запущен.
- `port is already allocated` — порты 8081, 8443 или 8025 заняты другим процессом.
- `TRAFFIC_KEY_HEX ...` — ключ в `.env` отсутствует или вместо 64 hex-символов
  остался текст из шаблона.
- Ошибка сертификата при запросе к `localhost` — сертификат рассчитан на
  `core-tls`; используйте curl-команду из раздела 6.
- `502` от Nginx — проверьте логи ядра; после его пересоздания перезапустите
  `core-tls`, чтобы обновился адрес upstream.
- Нет писем во внешнем ящике — проверьте, не используется ли локальный Mailpit.

## Если нужна сборка C++ непосредственно на Mac

Это отдельный режим разработки, не требующийся для описанного Docker-запуска.
Понадобятся Xcode Command Line Tools, CMake, Drogon **с поддержкой PostgreSQL**,
libpq, OpenSSL, libcurl, Boost.System, Jsoncpp, nlohmann-json и TgBot той же версии,
что в Dockerfile. Одна установка `brew install drogon` не гарантирует включённый
PostgreSQL-клиент: проверяйте конфигурацию сборки библиотеки.

После установки всех библиотек сборка выполняется из корня проекта:

```bash
cmake -S iot-core -B iot-core/build -DCMAKE_BUILD_TYPE=Debug
cmake --build iot-core/build -j2
cmake -S http-adapter -B http-adapter/build -DCMAKE_BUILD_TYPE=Debug
cmake --build http-adapter/build -j2
```

Требования к окружению нативных процессов:

- `iot-core` читает `config.json` из текущего каталога: его запускают из
  `iot-core` командой `./build/iot-core`. Конфиг ожидает основную БД на
  `127.0.0.1:5432`, базу правил на `127.0.0.1:5433`. Создайте базы, примените
  схемы `db/init.sql`, `db/rules.sql` для новых баз и миграции из
  [инструкции iot-core](iot-core/README.md#сборка-сервиса).
- `http-adapter` запускают из `http-adapter` командой `./build/http-adapter`.
  Укажите `CORE_URL` — адрес HTTPS-входа ядра, `CORE_CA_FILE` — абсолютный путь
  к `certs/ca.crt`. Имя в URL должно совпадать с сертификатом (`core-tls`).
- Обоим процессам задайте одинаковый `TRAFFIC_KEY_HEX`, а ядру также задайте
  `IOT_PASSWORD_PEPPER_HEX`. При нативном запуске
  `.env` автоматически не загружается — переменные нужно экспортировать в shell.
- Nginx, адреса БД и DNS нужно настроить под нативные процессы отдельно:
  готовый Compose использует Docker-имена `db`, `rules-db`, `iot-core`.

Этот режим здесь не проверялся; воспроизводимый и проверенный вариант запуска —
Docker Desktop по шагам выше. Справка по пакетам:
[Drogon](https://formulae.brew.sh/formula/drogon),
[libpq](https://formulae.brew.sh/formula/libpq),
[libcurl](https://formulae.brew.sh/formula/curl),
[nlohmann-json](https://formulae.brew.sh/formula/nlohmann-json).
