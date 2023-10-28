#pragma once

#include <userver/storages/postgres/postgres_fwd.hpp>

#include <models/birthday.hpp>

namespace telegram_bot::db {

std::vector<models::Birthday> FetchBirthdays(
    userver::storages::postgres::Cluster& postgres);

void DeleteBirthday(models::BirthdayId birthday_id,
                    userver::storages::postgres::Cluster& postgres);

void UpdateBirthdayLastNotificationTime(
    models::TimePoint last_notification_time, models::BirthdayId id,
    userver::storages::postgres::Cluster& postgres);

}  // namespace telegram_bot::db