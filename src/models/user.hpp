#pragma once

#include <userver/utils/strong_typedef.hpp>

namespace telegram_bot::models {

using UserId = userver::utils::StrongTypedef<class UserIdTag, int32_t>;
using ChatId = userver::utils::StrongTypedef<class ChatIdTag, int64_t>;

struct User {
  UserId id;
  ChatId chat_id;
};

}  // namespace telegram_bot::models
