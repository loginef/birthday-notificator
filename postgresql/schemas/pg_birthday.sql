DROP SCHEMA IF EXISTS birthday CASCADE;

CREATE SCHEMA birthday;

CREATE TABLE birthday.birthdays(
    id                     SERIAL PRIMARY KEY,
    person                 TEXT NOT NULL,
    y                      INTEGER NULL,
    m                      INTEGER NOT NULL,
    d                      INTEGER NOT NULL,
    notification_enabled   BOOLEAN NOT NULL,
    last_notification_time TIMESTAMPTZ
);

DROP SCHEMA IF EXISTS service CASCADE;
CREATE SCHEMA service;

CREATE TABLE service.distlocks(
    key             TEXT PRIMARY KEY,
    owner           TEXT,
    expiration_time TIMESTAMPTZ
);