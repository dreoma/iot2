-- Старым измерениям не приписываем текущего владельца: он мог измениться.
ALTER TABLE device_telemetry ADD COLUMN IF NOT EXISTS user_id BIGINT
    REFERENCES users(id) ON DELETE SET NULL;
CREATE INDEX IF NOT EXISTS telemetry_owner_idx ON device_telemetry(device_id, user_id, id DESC);
