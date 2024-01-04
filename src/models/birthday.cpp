#include "birthday.hpp"

#include <chrono>

namespace telegram_bot::models {

bool IsValidDate(const std::optional<BirthdayYear> y, const BirthdayMonth m,
                 const BirthdayDay d) {
  if (m.GetUnderlying() < 0 || d.GetUnderlying() < 0) {
    return false;
  }
  const auto month =
      std::chrono::month{static_cast<unsigned int>(m.GetUnderlying())};
  const auto day =
      std::chrono::day{static_cast<unsigned int>(d.GetUnderlying())};

  if (y.has_value()) {
    std::chrono::year_month_day date{std::chrono::year{y->GetUnderlying()},
                                     month, day};
    return date.ok();
  }

  std::chrono::month_day date{month, day};
  return date.ok();
}

}  // namespace telegram_bot::models
