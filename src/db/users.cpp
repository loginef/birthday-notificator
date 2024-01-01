#include "users.hpp"

#include <string>

#include <userver/storages/postgres/cluster.hpp>

namespace telegram_bot::db {

namespace {

const std::string kInsertUserQuery = R"(
INSERT
INTO birthday.users (chat_id)
VALUES ($1)
)";

const std::string kFindUserByChatIdQuery = R"(
SELECT
  users.id,
  users.chat_id
FROM birthday.users
WHERE users.chat_id = $1
)";

const std::string kFindUserById = R"(
SELECT
  users.id,
  users.chat_id
FROM birthday.users
WHERE users.id = $1
)";

struct Row {
  models::UserId id{};
  models::ChatId chat_id{};
};

}  // namespace

void InsertUser(const models::ChatId chat_id,
                userver::storages::postgres::Cluster& postgres) {
  postgres.Execute(userver::storages::postgres::ClusterHostType::kMaster,
                   kInsertUserQuery, chat_id);
}

std::optional<models::UserId> FindUser(
    const models::ChatId chat_id,
    userver::storages::postgres::Cluster& postgres) {
  const auto rows =
      postgres.Execute(userver::storages::postgres::ClusterHostType::kMaster,
                       kFindUserByChatIdQuery, chat_id);
  if (rows.IsEmpty()) {
    return std::nullopt;
  }
  const auto row = rows.AsSingleRow<Row>(userver::storages::postgres::kRowTag);
  return row.id;
}

models::ChatId GetChatId(const models::UserId user_id,
                         userver::storages::postgres::Cluster& postgres) {
  const auto rows =
      postgres.Execute(userver::storages::postgres::ClusterHostType::kMaster,
                       kFindUserById, user_id);
  const auto row = rows.AsSingleRow<Row>(userver::storages::postgres::kRowTag);
  return row.chat_id;
}

}  // namespace telegram_bot::db
