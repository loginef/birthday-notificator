DROP SCHEMA IF EXISTS birthday CASCADE;

CREATE SCHEMA birthday;

CREATE TABLE birthday.users(
    id      SERIAL PRIMARY KEY,
    chat_id BIGINT NOT NULL,

    UNIQUE(chat_id)
);

CREATE TABLE birthday.birthdays(
    id                     SERIAL PRIMARY KEY,
    person                 TEXT NOT NULL,
    y                      INTEGER NULL,
    m                      INTEGER NOT NULL,
    d                      INTEGER NOT NULL,
    notification_enabled   BOOLEAN NOT NULL,
    last_notification_time TIMESTAMPTZ,
    user_id                INTEGER NOT NULL REFERENCES birthday.users(id)
);

CREATE INDEX birthdays_user_id_idx ON birthday.birthdays(user_id);

DROP SCHEMA IF EXISTS service CASCADE;
CREATE SCHEMA service;

CREATE TABLE service.distlocks(
    key             TEXT PRIMARY KEY,
    owner           TEXT,
    expiration_time TIMESTAMPTZ
);
