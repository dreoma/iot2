-- Отдельная база iot_rules. Ссылки на устройства/пользователей проверяет API:
-- PostgreSQL не поддерживает внешние ключи между разными базами.
CREATE TABLE rules (
    id BIGSERIAL PRIMARY KEY,
    device_id BIGINT NOT NULL CHECK (device_id > 0),
    user_id BIGINT NOT NULL CHECK (user_id > 0),
    field VARCHAR(100) NOT NULL CHECK (length(trim(field)) > 0),
    operator VARCHAR(10) NOT NULL CHECK
        (operator IN ('gt', 'gte', 'lt', 'lte', 'eq', 'ne', 'changed', 'delta')),
    threshold DOUBLE PRECISION NOT NULL DEFAULT 0,
    message VARCHAR(2000) NOT NULL CHECK (length(trim(message)) > 0),
    enabled BOOLEAN NOT NULL DEFAULT TRUE,
    CHECK (operator <> 'delta' OR threshold > 0)
);
CREATE INDEX rules_device_idx ON rules(device_id, user_id) WHERE enabled;

-- Очередь готовых уведомлений. Это ещё не факт доставки в Telegram/email.
CREATE TABLE notifications (
    id BIGSERIAL PRIMARY KEY,
    rule_id BIGINT NOT NULL,
    telemetry_id BIGINT NOT NULL,
    device_id BIGINT NOT NULL,
    user_id BIGINT NOT NULL,
    telegram_id BIGINT NOT NULL,
    message TEXT NOT NULL,
    previous_value JSONB,
    current_value JSONB NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE (rule_id, telemetry_id)
);
CREATE INDEX notifications_user_idx ON notifications(user_id, id DESC);
