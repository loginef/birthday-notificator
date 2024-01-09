#pragma once

#include <atomic>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include <userver/components/component_fwd.hpp>
#include <userver/engine/task/task_with_result.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/utils/statistics/entry.hpp>

#include <tgbot/tgbot.h>

#include <components/bot/impl/http_client.hpp>
#include <models/button.hpp>

namespace telegram_bot::components::bot::impl {

struct Metrics {
  std::atomic<int64_t> received_commands{};
  std::atomic<int64_t> received_callbacks{};
  std::atomic<int64_t> sent_messages{};
  std::atomic<int64_t> updated_messages{};
};

class Component final {
 public:
  Component(const userver::components::ComponentConfig&,
            const userver::components::ComponentContext&);

  static constexpr const auto kName = "telegram-bot";

  void SendMessage(models::ChatId chat_id, const std::string& text);
  void SendMessageWithKeyboard(
      models::ChatId chat_id, const std::string& text,
      const std::vector<std::vector<models::Button>>& button_rows);
  void UpdateMessageWithKeyboard(
      models::ChatId chat_id, int32_t message_id, const std::string& text,
      const std::vector<std::vector<models::Button>>& button_rows);

 private:
  TelegramApiHttpClient telegram_client_;
  std::string telegram_token_;
  std::string telegram_host_;
  TgBot::Bot bot_;
  userver::storages::postgres::ClusterPtr postgres_;
  std::unordered_set<std::string> bot_commands_;
  Metrics metrics_;
  userver::utils::statistics::Entry statistics_holder_;
  userver::engine::TaskWithResult<void> task_;

 private:
  void Start();
  void Run();
  void SendMessageImpl(
      models::ChatId chat_id, const std::string& text,
      std::optional<std::vector<std::vector<models::Button>>> button_rows);
  void RegisterCommand(const std::string& command,
                       void (Component::*handler)(TgBot::Message::Ptr));

  void OnStartCommand(TgBot::Message::Ptr message);
  void OnChatIdCommand(TgBot::Message::Ptr message);
  void OnNextBirthdaysCommand(TgBot::Message::Ptr message);
  void OnAddBirthdayCommand(TgBot::Message::Ptr message);
  void OnEditBirthdayButton(models::ChatId chat_id, int32_t message_id,
                            const models::ButtonData& button_data);
  void OnDeleteBirthdayButton(models::ChatId chat_id, int32_t message_id,
                              const models::ButtonData& button_data);
  void OnCancelButton(models::ChatId chat_id, int32_t message_id);
  void OnRegisterCommand(TgBot::Message::Ptr message);
  void OnCallbackQuery(const TgBot::CallbackQuery::Ptr callback);
};

}  // namespace telegram_bot::components::bot::impl
