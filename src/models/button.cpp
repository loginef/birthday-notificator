#include "button.hpp"

#include <userver/crypto/base64.hpp>
#include <userver/logging/log.hpp>

#include <button.pb.h>

namespace telegram_bot::models {

namespace {

std::string ToProto(const ButtonData& button_data) {
  proto::ButtonData data;
  data.set_button_type(static_cast<int32_t>(button_data.type));
  data.set_context(static_cast<int32_t>(button_data.context));
  if (button_data.birthday_id.has_value()) {
    data.set_bd_id(button_data.birthday_id->GetUnderlying());
  }

  std::string result;
  data.SerializeToString(&result);
  return result;
}

}  // namespace

Button::Button(std::string title_, ButtonType type, ButtonContext context,
               std::optional<BirthdayId> birthday_id)
    : title{std::move(title_)}, data{type, context, birthday_id} {}

SerializedButton::SerializedButton(std::string title_, std::string data_)
    : title{std::move(title_)} {
  if (data_.empty() || data_.size() > 64) {
    throw std::runtime_error("Button data must be 1-64 bytes long");
  }
  data = std::move(data_);
}

SerializedButton::SerializedButton(Button button)
    : SerializedButton{
          std::move(button.title),
          userver::crypto::base64::Base64Encode(ToProto(button.data))} {}

ButtonData ButtonData::FromBase64Serialized(const std::string& data) {
  const auto binary = userver::crypto::base64::Base64Decode(data);
  proto::ButtonData parsed_data;
  parsed_data.ParseFromString(binary);

  ButtonData result;
  if (parsed_data.has_button_type()) {
    result.type = static_cast<ButtonType>(parsed_data.button_type());
  } else {
    throw std::runtime_error("Missing button_type field in message");
  }

  if (parsed_data.has_context()) {
    result.context = static_cast<ButtonContext>(parsed_data.context());
  } else {
    throw std::runtime_error("Missing context field in message");
  }

  if (parsed_data.has_bd_id()) {
    result.birthday_id = BirthdayId{parsed_data.bd_id()};
  }

  return result;
}

}  // namespace telegram_bot::models
