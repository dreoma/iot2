CREATE TABLE IF NOT EXISTS pending_registrations (
    registration_digest TEXT PRIMARY KEY,
    telegram_digest TEXT NOT NULL UNIQUE,
    mail VARCHAR(255) NOT NULL,
    telegram_id BIGINT,
    expires_at TIMESTAMPTZ NOT NULL DEFAULT (now() + interval '10 minutes')
);

CREATE INDEX IF NOT EXISTS pending_registrations_expiry_idx ON pending_registrations(expires_at);
