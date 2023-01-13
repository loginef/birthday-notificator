#pragma once

#include <atomic>
#include <string>

#include <userver/components/loggable_component_base.hpp>
#include <userver/engine/task/task_with_result.hpp>

namespace telegram_bot {

class Bot : public userver::components::LoggableComponentBase {
 public:
  static const std::string kName;

  Bot(const userver::components::ComponentConfig&,
      const userver::components::ComponentContext&);

 private:
  userver::engine::TaskWithResult<void> task_;
  std::string telegram_token_;

 private:
  void Start();
  void Run();
};

}  // namespace telegram_bot
