#pragma once

#include <optional>

#include <userver/storages/postgres/postgres_fwd.hpp>

#include <models/user.hpp>

namespace telegram_bot::db {

void InsertUser(models::ChatId chat_id,
                userver::storages::postgres::Cluster& postgres);

std::optional<models::UserId> FindUser(
    models::ChatId chat_id, userver::storages::postgres::Cluster& postgres);

models::ChatId GetChatId(models::UserId user_id,
                         userver::storages::postgres::Cluster& postgres);

}  // namespace telegram_bot::db
