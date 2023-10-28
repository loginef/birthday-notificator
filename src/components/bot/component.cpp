#include "component.hpp"

#include <userver/yaml_config/merge_schemas.hpp>

#include <components/bot/impl/component.hpp>

namespace telegram_bot::components::bot {

namespace {

const std::string kComponentConfigSchema = R"(
type: object
description: ClickHouse client component
additionalProperties: false
properties:
    chat_id:
        description: Recipient's telegram chat id to send messages
        type: integer
    telegram_host:
        description: Host to connect to
        type: string
)";

}  // namespace

const std::string Component::kName = impl::Component::kName;

Component::Component(const userver::components::ComponentConfig& config,
                     const userver::components::ComponentContext& context)
    : userver::components::LoggableComponentBase(config, context),
      impl_{std::make_unique<impl::Component>(config, context)} {}

userver::yaml_config::Schema Component::GetStaticConfigSchema() {
  return userver::yaml_config::MergeSchemas<
      userver::components::LoggableComponentBase>(kComponentConfigSchema);
}

Component::~Component() = default;

void Component::SendMessage(const std::string& text) const {
  impl_->SendMessage(text);
}

void Component::SendMessageWithKeyboard(
    const std::string& text,
    const std::vector<std::vector<models::Button>>& button_rows) const {
  impl_->SendMessageWithKeyboard(text, button_rows);
}

}  // namespace telegram_bot::components::bot
