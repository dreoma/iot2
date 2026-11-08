-- Пользователи. Поля соответствуют HTTP API.
CREATE TABLE users (
    id BIGSERIAL PRIMARY KEY,
    mail VARCHAR(255) NOT NULL UNIQUE,
    telegram_id BIGINT NOT NULL UNIQUE
);

-- Само устройство и его настройка уведомлений.
CREATE TABLE devices (
    id BIGSERIAL PRIMARY KEY,
    name VARCHAR(200) NOT NULL,
    notifications_enabled BOOLEAN NOT NULL DEFAULT TRUE
);

-- Связь: один пользователь владеет множеством устройств.
-- UNIQUE(device_id) не даёт назначить устройство двум владельцам.
CREATE TABLE user_devices (
    user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    device_id BIGINT NOT NULL UNIQUE REFERENCES devices(id) ON DELETE CASCADE,
    PRIMARY KEY (user_id, device_id)
);
CREATE INDEX user_devices_user_id_idx ON user_devices(user_id);

-- Телеметрия хранится отдельно: у устройства много измерений.
CREATE TABLE device_telemetry (
    id BIGSERIAL PRIMARY KEY,
    device_id BIGINT NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    data JSONB NOT NULL DEFAULT '{}'::jsonb,
    received_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
CREATE INDEX device_telemetry_device_id_idx
    ON device_telemetry(device_id, received_at DESC);
