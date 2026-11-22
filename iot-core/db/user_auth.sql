-- NULL оставляет существующие учётные записи без пароля до активации.
ALTER TABLE users
    ADD COLUMN IF NOT EXISTS password_hash TEXT,
    ADD COLUMN IF NOT EXISTS session_digest TEXT,
    ADD COLUMN IF NOT EXISTS session_expires_at TIMESTAMPTZ;

CREATE UNIQUE INDEX IF NOT EXISTS users_session_digest_idx ON users(session_digest);
