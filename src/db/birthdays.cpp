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
  birthdays.last_notification_time,
  birthdays.user_id
FROM birthday.birthdays
)";

const std::string kBirthdaysByUserIdQuery = R"(
SELECT
  birthdays.id,
  birthdays.person,
  birthdays.y,
  birthdays.m,
  birthdays.d,
  birthdays.notification_enabled,
  birthdays.last_notification_time,
  birthdays.user_id
FROM birthday.birthdays
WHERE birthdays.user_id = $1
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
  last_notification_time,
  user_id
)
VALUES (
  $1,
  $2,
  $3,
  $4,
  true,
  null,
  $5
)
)";

const std::string kCheckOwnershipQuery = R"(
SELECT birthdays.user_id
FROM birthday.birthdays
WHERE birthdays.id = $1
)";

}  // namespace

std::vector<models::Birthday> FetchAllBirthdays(
    userver::storages::postgres::Cluster& postgres) {
  return postgres
      .Execute(userver::storages::postgres::ClusterHostType::kMaster,
               kAllBirthdaysQuery)
      .AsContainer<std::vector<models::Birthday>>(
          userver::storages::postgres::kRowTag);
}

std::vector<models::Birthday> FetchBirthdays(
    const models::UserId user_id,
    userver::storages::postgres::Cluster& postgres) {
  return postgres
      .Execute(userver::storages::postgres::ClusterHostType::kMaster,
               kBirthdaysByUserIdQuery, user_id)
      .AsContainer<std::vector<models::Birthday>>(
          userver::storages::postgres::kRowTag);
}

bool IsOwnerOfBirthday(const models::UserId user_id,
                       const models::BirthdayId birthday_id,
                       userver::storages::postgres::Cluster& postgres) {
  const auto record_user_id =
      postgres
          .Execute(userver::storages::postgres::ClusterHostType::kMaster,
                   kCheckOwnershipQuery, birthday_id)
          .AsSingleRow<models::UserId>();
  return record_user_id == user_id;
}

void DeleteBirthday(const models::BirthdayId birthday_id,
                    userver::storages::postgres::Cluster& postgres) {
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
                    const std::string& person, const models::UserId user_id,
                    userver::storages::postgres::Cluster& postgres) {
  postgres.Execute(userver::storages::postgres::ClusterHostType::kMaster,
                   kInsertBirthday, person, y, m, d, user_id);
}

}  // namespace telegram_bot::db