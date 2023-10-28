#pragma once

#include <optional>
#include <vector>

#include <tgbot/types/GenericReply.h>

#include <models/button.hpp>

namespace telegram_bot::components::bot::impl {

// may return nullptr
TgBot::GenericReply::Ptr MakeReplyMarkup(
    std::optional<std::vector<std::vector<models::Button>>>&& keyboard);

}  // namespace telegram_bot::components::bot::impl
