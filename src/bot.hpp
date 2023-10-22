#pragma once

#include <string>

#include <userver/clients/http/client.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/components/loggable_component_base.hpp>
#include <userver/concurrent/mpsc_queue.hpp>
#include <userver/engine/task/cancel.hpp>
#include <userver/engine/task/task_with_result.hpp>
#include <userver/storages/postgres/cluster.hpp>

#include <tgbot/tgbot.h>

#include "button.hpp"

namespace telegram_bot {

class Bot : public userver::components::LoggableComponentBase {
 public:
  static const std::string kName;

  Bot(const userver::components::ComponentConfig&,
      const userver::components::ComponentContext&);

  void SendMessage(const std::string& text) const;
  void SendMessageWithKeyboard(
      const std::string& text,
      const std::vector<std::vector<Button>>& button_rows) const;

  static userver::yaml_config::Schema GetStaticConfigSchema();

 private:
  struct SendMessageRequest {
    std::string text;
    std::optional<std::vector<std::vector<Button>>> keyboard;
  };

  userver::clients::http::Client& http_client_;
  std::string telegram_token_;
  int64_t chat_id_;
  std::string telegram_host_;
  userver::storages::postgres::ClusterPtr postgres_;
  std::shared_ptr<
      userver::concurrent::MpscQueue<std::unique_ptr<SendMessageRequest>>>
      queue_;
  userver::concurrent::MpscQueue<std::unique_ptr<SendMessageRequest>>::Consumer
      consumer_;
  userver::engine::TaskWithResult<void> task_;

 private:
  void Start();
  void Run();
  void SendSendMessageRequest(SendMessageRequest&& request) const;
};

}  // namespace telegram_bot
