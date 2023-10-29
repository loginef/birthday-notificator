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

const std::string kInsertBirthday = R"(
INSERT INTO birthday.birthdays(
  person,
  y,
  m,
  d,
  notification_enabled,
  last_notification_time
)
VALUES (
  $1,
  $2,
  $3,
  $4,
  true,
  null
)
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

void InsertBirthday(const models::BirthdayMonth m, const models::BirthdayDay d,
                    const std::optional<models::BirthdayYear> y,
                    const std::string& person,
                    userver::storages::postgres::Cluster& postgres) {
  postgres.Execute(userver::storages::postgres::ClusterHostType::kMaster,
                   kInsertBirthday, person, y, m, d);
}

}  // namespace telegram_bot::db