#pragma once

#include <optional>

#include <userver/storages/postgres/postgres_fwd.hpp>

#include <models/birthday.hpp>
#include <models/time_point.hpp>

namespace telegram_bot::db {

std::vector<models::Birthday> FetchAllBirthdays(
    userver::storages::postgres::Cluster& postgres);

std::vector<models::Birthday> FetchBirthdays(
    models::UserId user_id, userver::storages::postgres::Cluster& postgres);

bool IsOwnerOfBirthday(models::UserId user_id, models::BirthdayId birthday_id,
                       userver::storages::postgres::Cluster& postgres);

void DeleteBirthday(models::BirthdayId birthday_id,
                    userver::storages::postgres::Cluster& postgres);

void UpdateBirthdayLastNotificationTime(
    models::TimePoint last_notification_time, models::BirthdayId id,
    userver::storages::postgres::Cluster& postgres);

void InsertBirthday(models::BirthdayMonth m, models::BirthdayDay d,
                    std::optional<models::BirthdayYear> y,
                    const std::string& person, models::UserId user_id,
                    userver::storages::postgres::Cluster& postgres);

}  // namespace telegram_bot::db