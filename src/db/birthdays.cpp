#include "birthdays.hpp"

#include <string>

#include <userver/storages/postgres/cluster.hpp>

namespace telegram_bot::db {

namespace {

const std::string kAllBirthdaysQuery = R"(
SELECT
  birthdays.id,
  birthdays.person,
  birthdays.y,
  birthdays.m,
  birthdays.d,
  birthdays.notification_enabled,
  birthdays.last_notification_time
FROM birthday.birthdays
)";

const std::string kDeleteBirthdayQuery = R"(
DELETE
FROM birthday.birthdays
WHERE birthdays.id = $1
)";

const std::string kUpdateLastNotificationTime = R"(
UPDATE birthday.birthdays
SET last_notification_time = $1
WHERE birthdays.id = $2
)";

}  // namespace

std::vector<models::Birthday> FetchBirthdays(
    userver::storages::postgres::Cluster& postgres) {
  // TODO check ownership
  return postgres
      .Execute(userver::storages::postgres::ClusterHostType::kMaster,
               kAllBirthdaysQuery)
      .AsContainer<std::vector<models::Birthday>>(
          userver::storages::postgres::kRowTag);
}

void DeleteBirthday(const models::BirthdayId birthday_id,
                    userver::storages::postgres::Cluster& postgres) {
  // TODO check ownership
  postgres.Execute(userver::storages::postgres::ClusterHostType::kMaster,
                   kDeleteBirthdayQuery, birthday_id);
}

void UpdateBirthdayLastNotificationTime(
    const models::TimePoint last_notification_time, const models::BirthdayId id,
    userver::storages::postgres::Cluster& postgres) {
  postgres.Execute(
      userver::storages::postgres::ClusterHostType::kMaster,
      kUpdateLastNotificationTime,
      userver::storages::postgres::TimePointTz{last_notification_time}, id);
}

}  // namespace telegram_bot::db