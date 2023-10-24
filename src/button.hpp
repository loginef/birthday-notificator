#pragma once

#include <optional>
#include <string>

namespace telegram_bot {

enum class ButtonType : int32_t {
  kEditBirthday = 0,
  kDeleteBirthday = 1,
  kCancel = 2
};
enum class ButtonContext : int32_t {
  kNextBirthdays = 0,
  kNextBirthdaysEditBirthday = 1
};

struct ButtonData {
  ButtonType type;
  ButtonContext context;
  std::optional<int32_t> birthday_id;

  static ButtonData FromBase64Serialized(const std::string& data);
};

struct Button {
  Button(std::string title_, ButtonType type, ButtonContext context,
         std::optional<int32_t> birthday_id);

  std::string title;
  ButtonData data;
};

struct SerializedButton {
  SerializedButton(std::string title_, std::string data_);
  explicit SerializedButton(Button button);

  std::string title;
  std::string data;
};

}  // namespace telegram_bot