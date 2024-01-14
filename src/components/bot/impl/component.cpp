#include "component.hpp"

#include <chrono>
#include <exception>
#include <regex>
#include <tuple>

#include <cctz/time_zone.h>
#include <fmt/format.h>

#include <userver/clients/http/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/components/statistics_storage.hpp>
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
#include <db/users.hpp>

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

struct MessageWithOptionalKeyboard {
  std::string text;
  std::optional<std::vector<std::vector<models::Button>>> keyboard;
};

const int32_t kNextBirthdaysLimitNew = 6;

MessageWithOptionalKeyboard GetNextBirthdaysMessage(
    const models::ChatId chat_id,
    userver::storages::postgres::Cluster& postgres) {
  const auto user_id = db::FindUser(chat_id, postgres);
  if (!user_id.has_value()) {
    return {"You are not registered yet", {}};
  }

  auto list = db::FetchBirthdays(*user_id, postgres);
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
      telegram_host_(config["telegram_host"].As<std::string>()),
      bot_(telegram_token_, telegram_client_, telegram_host_),
      postgres_(
          context.FindComponent<userver::components::Postgres>("postgres-db")
              .GetCluster()) {
  auto& storage =
      context.FindComponent<userver::components::StatisticsStorage>()
          .GetStorage();

  statistics_holder_ = storage.RegisterWriter(
      kName, [this](userver::utils::statistics::Writer& writer) {
        writer["received-commands"] = metrics_.received_commands;
        writer["received-callbacks"] = metrics_.received_callbacks;
        writer["sent-messages"] = metrics_.sent_messages;
        writer["updated-messages"] = metrics_.updated_messages;
      });

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
  ++metrics_.received_commands;
  SendMessage(models::ChatId{message->chat->id}, "Hi!");
}

void Component::OnChatIdCommand(TgBot::Message::Ptr message) {
  ++metrics_.received_commands;
  SendMessage(models::ChatId{message->chat->id},
              fmt::format("Your chat id is {}", message->chat->id));
}

void Component::OnNextBirthdaysCommand(TgBot::Message::Ptr message) {
  ++metrics_.received_commands;

  const models::ChatId chat_id{message->chat->id};
  auto response = GetNextBirthdaysMessage(chat_id, *postgres_);
  if (response.keyboard.has_value()) {
    SendMessageWithKeyboard(chat_id, response.text, *response.keyboard);
  } else {
    SendMessage(chat_id, response.text);
  }
}

void Component::OnAddBirthdayCommand(TgBot::Message::Ptr message) {
  ++metrics_.received_commands;

  const models::ChatId chat_id{message->chat->id};
  const auto user_id = db::FindUser(chat_id, *postgres_);
  if (!user_id.has_value()) {
    SendMessage(chat_id, "Not registered yet, try to /register");
    return;
  }

  std::regex re(R"(^/add_birthday[^\s]*\s+(\d{2})\.(\d{2})(.(\d{4}))?\s+(.*))");
  std::smatch match;
  if (!std::regex_match(message->text, match, re)) {
    SendMessage(chat_id, "Usage: /add_birthday DD.MM[.YYYY] Person Name");
    return;
  }

  UINVARIANT(match.size() == 6, "unexpected match size, check regexp");
  models::BirthdayDay d{std::stoi(match[1])};
  models::BirthdayMonth m{std::stoi(match[2])};
  std::optional<models::BirthdayYear> y{};
  if (match[4].matched) {
    // skip dot, which is index 4
    y = models::BirthdayYear{std::stoi(match[4])};
  }
  if (!models::IsValidDate(y, m, d)) {
    SendMessage(chat_id, "Invalid date");
    return;
  }

  std::string person = match[5];
  if (person.size() > 128) {
    SendMessage(chat_id, "Too long name, provide up to 128 characters please");
    return;
  }

  db::InsertBirthday(m, d, y, person, *user_id, *postgres_);
  SendMessage(chat_id, fmt::format("Inserted the birthday of {} on {:02}.{:02}",
                                   person, d, m));
}

void Component::OnEditBirthdayButton(const models::ChatId chat_id,
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

void Component::OnDeleteBirthdayButton(const models::ChatId chat_id,
                                       const int32_t message_id,
                                       const models::ButtonData& button_data) {
  LOG_INFO() << "Got delete birthday command";
  if (!button_data.birthday_id.has_value()) {
    LOG_WARNING() << "No birthday_id in button data";
    UpdateMessageWithKeyboard(chat_id, message_id, "Canceled", {});
    return;
  }

  const auto user_id = db::FindUser(chat_id, *postgres_);
  if (!user_id.has_value()) {
    LOG_WARNING() << "Got button from missing user";
    UpdateMessageWithKeyboard(chat_id, message_id, "Canceled", {});
    return;
  }

  if (!db::IsOwnerOfBirthday(*user_id, *button_data.birthday_id, *postgres_)) {
    LOG_WARNING() << "Tried to delete another user's data";
    UpdateMessageWithKeyboard(chat_id, message_id, "Canceled", {});
    return;
  }

  db::DeleteBirthday(*button_data.birthday_id, *postgres_);
  UpdateMessageWithKeyboard(chat_id, message_id, "Deleted", {});
}

void Component::OnCancelButton(const models::ChatId chat_id,
                               const int32_t message_id) {
  LOG_INFO() << "Got cancel command";
  UpdateMessageWithKeyboard(chat_id, message_id, "Canceled", {});
}

void Component::OnRegisterCommand(TgBot::Message::Ptr message) {
  ++metrics_.received_commands;

  const models::ChatId chat_id{message->chat->id};
  const auto user_id = db::FindUser(chat_id, *postgres_);
  if (user_id.has_value()) {
    SendMessage(chat_id, "Already registered");
    return;
  }

  db::InsertUser(chat_id, *postgres_);
  SendMessage(chat_id,
              "Done. Note that all your personal data will be stored in plain "
              "text. I promise not to look :)");
}

void Component::OnUnregisterCommand(TgBot::Message::Ptr message) {
  ++metrics_.received_commands;

  const models::ChatId chat_id{message->chat->id};
  const auto user_id = db::FindUser(chat_id, *postgres_);
  if (!user_id.has_value()) {
    SendMessage(chat_id, "You are not registered");
    return;
  }

  db::DeleteAllBirthdays(*user_id, *postgres_);
  db::DeleteUser(*user_id, *postgres_);

  SendMessage(chat_id, "Deleted all your birthdays and forgot about you");
}

void Component::OnCallbackQuery(const TgBot::CallbackQuery::Ptr callback) {
  ++metrics_.received_callbacks;

  if (callback->message) {
    const models::ChatId chat_id{callback->message->chat->id};
    if (!callback->data.empty()) {
      try {
        const auto button_data =
            models::ButtonData::FromBase64Serialized(callback->data);
        switch (button_data.type) {
          case models::ButtonType::kEditBirthday:
            OnEditBirthdayButton(chat_id, callback->message->messageId,
                                 button_data);
            break;
          case models::ButtonType::kDeleteBirthday:
            OnDeleteBirthdayButton(chat_id, callback->message->messageId,
                                   button_data);
            break;
          case models::ButtonType::kCancel:
            OnCancelButton(chat_id, callback->message->messageId);
            break;
        }
      } catch (const std::exception& exc) {
        LOG_ERROR() << "Failed to process callback query: " << exc;
      }
    } else {
      LOG_INFO() << "No data in callback, skip";
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
  try {
    TgBot::TgLongPoll long_poll(bot_, 100, 1);

    RegisterCommand("add_birthday", &Component::OnAddBirthdayCommand);
    RegisterCommand("chat_id", &Component::OnChatIdCommand);
    RegisterCommand("next_birthdays", &Component::OnNextBirthdaysCommand);
    RegisterCommand("register", &Component::OnRegisterCommand);
    RegisterCommand("start", &Component::OnStartCommand);
    RegisterCommand("unregister", &Component::OnUnregisterCommand);

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
  } catch (const std::exception& exc) {
    LOG_ERROR() << "Unexpected exception " << exc;
    throw;
  }
}

void Component::SendMessage(const models::ChatId chat_id,
                            const std::string& text) {
  SendMessageImpl(chat_id, text, std::nullopt);
}

void Component::SendMessageWithKeyboard(
    const models::ChatId chat_id, const std::string& text,
    const std::vector<std::vector<models::Button>>& button_rows) {
  SendMessageImpl(chat_id, text, button_rows);
}

void Component::SendMessageImpl(
    const models::ChatId chat_id, const std::string& text,
    std::optional<std::vector<std::vector<models::Button>>> button_rows) {
  ++metrics_.sent_messages;

  bot_.getApi().sendMessage(
      chat_id.GetUnderlying(), text, false /*disableWebPagePreview*/,
      0 /*replyToMessageId*/, MakeReplyMarkup(std::move(button_rows)));
}

void Component::UpdateMessageWithKeyboard(
    const models::ChatId chat_id, int32_t message_id, const std::string& text,
    const std::vector<std::vector<models::Button>>& button_rows) {
  ++metrics_.updated_messages;

  bot_.getApi().editMessageText(text, chat_id.GetUnderlying(), message_id,
                                "" /* inlineMessageId */, "" /* parseMode */,
                                false /* disableWebPagePreview */,
                                MakeReplyMarkup(button_rows));
}

}  // namespace telegram_bot::components::bot::impl
