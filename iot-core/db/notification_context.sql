-- Снимок события сохраняет смысл истории после переименования/удаления сущностей.
ALTER TABLE notifications ADD COLUMN IF NOT EXISTS context JSONB NOT NULL DEFAULT '{}'::jsonb;
