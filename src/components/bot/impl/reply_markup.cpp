#include "reply_markup.hpp"

#include <tgbot/types/InlineKeyboardButton.h>
#include <tgbot/types/InlineKeyboardMarkup.h>

#include <userver/logging/log.hpp>

namespace telegram_bot::components::bot::impl {

TgBot::GenericReply::Ptr MakeReplyMarkup(
    std::optional<std::vector<std::vector<models::Button>>>&& keyboard) {
  if (!keyboard.has_value()) {
    return nullptr;
  }

  if (keyboard->empty()) {
    LOG_ERROR() << "Empty keyboard provided, programming error";
    return nullptr;
  }

  std::vector<std::vector<TgBot::InlineKeyboardButton::Ptr>> inline_keyboard;
  inline_keyboard.reserve(keyboard->size());
  for (auto& row : *keyboard) {
    if (row.empty()) {
      LOG_ERROR() << "Empty keyboard row provided, programming error";
      return nullptr;
    }

    std::vector<TgBot::InlineKeyboardButton::Ptr> inline_row;
    inline_row.reserve(row.size());
    for (auto& button : row) {
      models::SerializedButton serialized_button(std::move(button));
      auto inline_button = std::make_shared<TgBot::InlineKeyboardButton>();
      inline_button->text = std::move(serialized_button.title);
      inline_button->callbackData = std::move(serialized_button.data);
      inline_button->pay = false;
      inline_row.push_back(std::move(inline_button));
    }

    inline_keyboard.push_back(std::move(inline_row));
  }

  auto result = std::make_shared<TgBot::InlineKeyboardMarkup>();
  result->inlineKeyboard = std::move(inline_keyboard);
  return result;
}

}  // namespace telegram_bot::components::bot::impl
