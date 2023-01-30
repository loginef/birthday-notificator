#pragma once

#include <atomic>
#include <string>

#include <userver/components/loggable_component_base.hpp>
#include <userver/engine/task/task_with_result.hpp>

//
#include <chrono>
#include <exception>
#include <string>
#include <vector>

#include <fmt/format.h>

#include <userver/components/component_context.hpp>
#include <userver/engine/async.hpp>
#include <userver/engine/sleep.hpp>
#include <userver/engine/task/cancel.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/secdist/component.hpp>
#include <userver/tracing/span.hpp>

#include <chrono>
#include <exception>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include <fmt/format.h>

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/concurrent/mpsc_queue.hpp>
#include <userver/engine/async.hpp>
#include <userver/engine/sleep.hpp>
#include <userver/engine/task/cancel.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/secdist/component.hpp>
#include <userver/testsuite/testpoint.hpp>
#include <userver/tracing/span.hpp>
#include <userver/utils/datetime.hpp>
#include <userver/utils/time_of_day.hpp>

namespace telegram_bot {

class Bot : public userver::components::LoggableComponentBase {
 public:
  static const std::string kName;

  Bot(const userver::components::ComponentConfig&,
      const userver::components::ComponentContext&);

  void SendMessage(const std::string& text);

  static userver::yaml_config::Schema GetStaticConfigSchema();

 private:
  struct SendMessageRequest {
    std::string text;
  };

  std::string telegram_token_;
  int64_t chat_id_;
  bool fake_api_;
  std::shared_ptr<
      userver::concurrent::MpscQueue<std::unique_ptr<SendMessageRequest>>>
      queue_;
  userver::concurrent::MpscQueue<std::unique_ptr<SendMessageRequest>>::Consumer
      consumer_;
  userver::engine::TaskWithResult<void> task_;

 private:
  void Start();
  void Run();
};

}  // namespace telegram_bot
