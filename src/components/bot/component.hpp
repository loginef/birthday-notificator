#pragma once

#include <memory>
#include <string>
#include <vector>

#include <userver/components/loggable_component_base.hpp>
#include <userver/yaml_config/schema.hpp>

#include <models/button.hpp>

namespace telegram_bot::components::bot {

namespace impl {

class Component;

}  // namespace impl

class Component final : public userver::components::LoggableComponentBase {
 public:
  static const std::string kName;

  Component(const userver::components::ComponentConfig&,
            const userver::components::ComponentContext&);
  virtual ~Component() override;

  void SendMessage(const std::string& text) const;
  void SendMessageWithKeyboard(
      const std::string& text,
      const std::vector<std::vector<models::Button>>& button_rows) const;

  static userver::yaml_config::Schema GetStaticConfigSchema();

 private:
  std::unique_ptr<impl::Component> impl_;
};

}  // namespace telegram_bot::components::bot
