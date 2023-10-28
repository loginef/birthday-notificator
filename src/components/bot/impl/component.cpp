#include "component.hpp"

#include <chrono>
#include <exception>
#include <tuple>

#include <cctz/time_zone.h>
#include <fmt/format.h>

#include <userver/clients/http/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/engine/sleep.hpp>
#include <userver/engine/task/cancel.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/secdist/component.hpp>
#include <userver/tracing/span.hpp>
#include <userver/utils/async.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

#include <tgbot/types/InlineKeyboardButton.h>
#include <tgbot/types/InlineKeyboardMarkup.h>

#include <components/bot/impl/reply_markup.hpp>
#include <db/birthdays.hpp>

namespace telegram_bot::components::bot::impl {

namespace {

struct Token {
  std::string token;

  Token(const userver::formats::json::Value& doc)
      : token(doc["telegram_token"].As<std::string>()) {}
};

std::string GetToken(const userver::components::ComponentContext& context) {
  const auto& secdist = context.FindComponent<userver::components::Secdist>();
  const auto& secdist_config = secdist.Get();
  return secdist_config.Get<Token>().token;
}

const int32_t kNextBirthdaysLimitOld = 5;

std::string GetNextBirthdaysMessage(
    const int64_t sender_chat_id, const int64_t allowed_chat_id,
    userver::storages::postgres::Cluster& postgres) {
  if (sender_chat_id != allowed_chat_id) {
    return "No birthdays for you, sorry";
  }

  auto list = db::FetchBirthdays(postgres);
  if (list.empty()) {
    return "There are no birthdays";
  }

  cctz::time_zone notification_timezone;
  if (!cctz::load_time_zone("Europe/Moscow", &notification_timezone)) {
    throw std::runtime_error("Unknown timezone Europe/Moscow");
  }
  const auto now = userver::utils::datetime::Now();
  const auto local_time = cctz::convert(now, notification_timezone);
  const auto local_day = cctz::civil_day(local_time);

  std::sort(
      list.begin(), list.end(),
      [local_day](const models::Birthday& lhs, const models::Birthday& rhs) {
        const int l_year = std::tuple(lhs.m, lhs.d) <
                           std::tuple(models::BirthdayMonth{local_day.month()},
                                      models::BirthdayDay{local_day.day()});
        const int r_year = std::tuple(rhs.m, rhs.d) <
                           std::tuple(models::BirthdayMonth{local_day.month()},
                                      models::BirthdayDay{local_day.day()});
        return std::tuple(l_year, lhs.m, lhs.d) <
               std::tuple(r_year, rhs.m, rhs.d);
      });
  if (list.size() > kNextBirthdaysLimitOld) {
    list.resize(kNextBirthdaysLimitOld);
  }

  std::string message = "Next birthdays:";
  for (const auto& birthday : list) {
    message += fmt::format("\n{} on {:02}.{:02}", birthday.person, birthday.d,
                           birthday.m);
  }
  return message;
}

struct MessageWithOptionalKeyboard {
  std::string text;
  std::optional<std::vector<std::vector<models::Button>>> keyboard;
};

const int32_t kNextBirthdaysLimitNew = 6;

MessageWithOptionalKeyboard GetNextBirthdaysMessageNew(
    const int64_t sender_chat_id, const int64_t allowed_chat_id,
    userver::storages::postgres::Cluster& postgres) {
  if (sender_chat_id != allowed_chat_id) {
    return {"No birthdays for you, sorry", {}};
  }

  auto list = db::FetchBirthdays(postgres);
  if (list.empty()) {
    return {"There are no birthdays", {}};
  }

  cctz::time_zone notification_timezone;
  if (!cctz::load_time_zone("Europe/Moscow", &notification_timezone)) {
    throw std::runtime_error("Unknown timezone Europe/Moscow");
  }
  const auto now = userver::utils::datetime::Now();
  LOG_DEBUG() << "at " << userver::utils::datetime::Timestring(now);
  const auto local_time = cctz::convert(now, notification_timezone);
  const auto local_day = cctz::civil_day(local_time);

  std::sort(
      list.begin(), list.end(),
      [local_day](const models::Birthday& lhs, const models::Birthday& rhs) {
        const int l_year = std::tuple(lhs.m, lhs.d) <
                           std::tuple(models::BirthdayMonth{local_day.month()},
                                      models::BirthdayDay{local_day.day()});
        const int r_year = std::tuple(rhs.m, rhs.d) <
                           std::tuple(models::BirthdayMonth{local_day.month()},
                                      models::BirthdayDay{local_day.day()});
        return std::tuple(l_year, lhs.m, lhs.d) <
               std::tuple(r_year, rhs.m, rhs.d);
      });
  if (list.size() > kNextBirthdaysLimitNew) {
    list.resize(kNextBirthdaysLimitNew);
  }

  std::string message = fmt::format("Next {} birthdays:", list.size());
  std::vector<std::vector<models::Button>> keyboard;
  for (const auto& birthday : list) {
    auto title = fmt::format("{} on {:02}.{:02}", birthday.person, birthday.d,
                             birthday.m);
    keyboard.push_back(
        {models::Button{std::move(title), models::ButtonType::kEditBirthday,
                        models::ButtonContext::kNextBirthdays, birthday.id}});
  }
  return {std::move(message), std::move(keyboard)};
}

}  // namespace

Component::Component(const userver::components::ComponentConfig& config,
                     const userver::components::ComponentContext& context)
    : telegram_client_{context.FindComponent<userver::components::HttpClient>()
                           .GetHttpClient()},
      telegram_token_(GetToken(context)),
      chat_id_(config["chat_id"].As<int64_t>()),
      telegram_host_(config["telegram_host"].As<std::string>()),
      bot_(telegram_token_, telegram_client_, telegram_host_),
      postgres_(
          context.FindComponent<userver::components::Postgres>("postgres-db")
              .GetCluster()) {
  bot_.getApi().deleteWebhook();
  Start();
}

void Component::Start() {
  userver::tracing::Span span{kName};
  span.DetachFromCoroStack();
  task_ =
      userver::engine::AsyncNoSpan([this, span = std::move(span)]() mutable {
        span.AttachToCoroStack();
        Run();
      });
}

void Component::OnStartCommand(TgBot::Message::Ptr message) {
  bot_.getApi().sendMessage(message->chat->id, "Hi!");
}

void Component::OnChatIdCommand(TgBot::Message::Ptr message) {
  bot_.getApi().sendMessage(
      message->chat->id, fmt::format("Your chat id is {}", message->chat->id));
}

void Component::OnNextBirthdaysCommand(TgBot::Message::Ptr message) {
  bot_.getApi().sendMessage(
      message->chat->id,
      GetNextBirthdaysMessage(message->chat->id, chat_id_, *postgres_));
}

void Component::OnNextBirthdaysNewCommand(TgBot::Message::Ptr message) {
  auto response =
      GetNextBirthdaysMessageNew(message->chat->id, chat_id_, *postgres_);
  bot_.getApi().sendMessage(
      message->chat->id, response.text, false /*disableWebPagePreview*/,
      0 /*replyToMessageId*/, MakeReplyMarkup(std::move(response.keyboard)));
}

void Component::OnEditBirthdayButton(const int32_t chat_id,
                                     const int32_t message_id,
                                     const models::ButtonData& button_data) {
  LOG_INFO() << "Got edit birthday command";
  if (button_data.birthday_id.has_value()) {
    std::vector<std::vector<models::Button>> keyboard{
        {models::Button{"Delete", models::ButtonType::kDeleteBirthday,
                        models::ButtonContext::kNextBirthdaysEditBirthday,
                        *button_data.birthday_id}},
        {models::Button{"Cancel", models::ButtonType::kCancel,
                        models::ButtonContext::kNextBirthdaysEditBirthday,
                        *button_data.birthday_id}}};
    // TODO list details of a birthday instead
    UpdateMessageWithKeyboard(chat_id, message_id, "Select option", keyboard);
  } else {
    LOG_WARNING() << "No birthday_id in button data";
    UpdateMessageWithKeyboard(chat_id, message_id, "Canceled", {});
  }
}

void Component::OnDeleteBirthdayButton(const int32_t chat_id,
                                       const int32_t message_id,
                                       const models::ButtonData& button_data) {
  LOG_INFO() << "Got delete birthday command";
  if (button_data.birthday_id.has_value()) {
    db::DeleteBirthday(*button_data.birthday_id, *postgres_);
    UpdateMessageWithKeyboard(chat_id, message_id, "Deleted", {});
  } else {
    LOG_WARNING() << "No birthday_id in button data";
    UpdateMessageWithKeyboard(chat_id, message_id, "Canceled", {});
  }
}

void Component::OnCancelButton(const int32_t chat_id,
                               const int32_t message_id) {
  LOG_INFO() << "Got cancel command";
  UpdateMessageWithKeyboard(chat_id, message_id, "Canceled", {});
}

void Component::OnCallbackQuery(const TgBot::CallbackQuery::Ptr callback) {
  if (callback->message) {
    if (callback->message->chat->id != chat_id_) {
      UpdateMessageWithKeyboard(callback->message->chat->id,
                                callback->message->messageId, "Unauthorized",
                                {});
    } else {
      if (!callback->data.empty()) {
        try {
          const auto button_data =
              models::ButtonData::FromBase64Serialized(callback->data);
          switch (button_data.type) {
            case models::ButtonType::kEditBirthday:
              OnEditBirthdayButton(callback->message->chat->id,
                                   callback->message->messageId, button_data);
              break;
            case models::ButtonType::kDeleteBirthday:
              OnDeleteBirthdayButton(callback->message->chat->id,
                                     callback->message->messageId, button_data);
              break;
            case models::ButtonType::kCancel:
              OnCancelButton(callback->message->chat->id,
                             callback->message->messageId);
              break;
          }
        } catch (const std::exception& exc) {
          LOG_ERROR() << "Failed to process callback query: " << exc;
        }
      } else {
        LOG_INFO() << "No data in callback, skip";
      }
    }
  } else {
    LOG_INFO() << "No message in callback, skip";
  }
  bot_.getApi().answerCallbackQuery(callback->id);
}

void Component::RegisterCommand(
    const std::string& command,
    void (Component::*const handler)(TgBot::Message::Ptr)) {
  bot_commands_.insert("/" + command);
  bot_.getEvents().onCommand(command,
                             std::bind(handler, this, std::placeholders::_1));
}

void Component::Run() {
  TgBot::TgLongPoll long_poll(bot_, 100, 1);

  RegisterCommand("start", &Component::OnStartCommand);
  RegisterCommand("chat_id", &Component::OnChatIdCommand);
  RegisterCommand("next_birthdays", &Component::OnNextBirthdaysCommand);
  RegisterCommand("next_birthdays_new", &Component::OnNextBirthdaysNewCommand);

  bot_.getEvents().onAnyMessage([&](TgBot::Message::Ptr message) {
    if (bot_commands_.contains(message->text)) {
      return;
    }

    bot_.getApi().sendMessage(message->chat->id, "Unknown command");
  });

  bot_.getEvents().onCallbackQuery(
      [this](const TgBot::CallbackQuery::Ptr callback) {
        OnCallbackQuery(callback);
      });

  while (!userver::engine::current_task::ShouldCancel()) {
    try {
      LOG_INFO() << "Long poll started";
      long_poll.start();
      LOG_INFO() << "Long poll finished";
    } catch (std::exception& e) {
      LOG_ERROR() << "error: " << e;
    }
  }
}

void Component::SendMessage(const std::string& text) const {
  SendMessageImpl(text, std::nullopt);
}

void Component::SendMessageWithKeyboard(
    const std::string& text,
    const std::vector<std::vector<models::Button>>& button_rows) const {
  SendMessageImpl(text, button_rows);
}

void Component::SendMessageImpl(
    const std::string& text,
    std::optional<std::vector<std::vector<models::Button>>> button_rows) const {
  bot_.getApi().sendMessage(chat_id_, text, false /*disableWebPagePreview*/,
                            0 /*replyToMessageId*/,
                            MakeReplyMarkup(std::move(button_rows)));
}

void Component::UpdateMessageWithKeyboard(
    const int32_t chat_id, int32_t message_id, const std::string& text,
    const std::vector<std::vector<models::Button>>& button_rows) {
  bot_.getApi().editMessageText(
      text, chat_id, message_id, "" /* inlineMessageId */, "" /* parseMode */,
      false /* disableWebPagePreview */, MakeReplyMarkup(button_rows));
}

}  // namespace telegram_bot::components::bot::impl
