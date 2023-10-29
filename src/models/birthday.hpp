#pragma once

#include <optional>
#include <string>

#include <userver/utils/strong_typedef.hpp>

#include <models/time_point.hpp>

namespace telegram_bot::models {

using BirthdayId = userver::utils::StrongTypedef<class BirthdayIdTag, int32_t>;
using BirthdayDay = userver::utils::StrongTypedef<class BirthdayDayTag, int>;
using BirthdayMonth =
    userver::utils::StrongTypedef<class BirthdayMonthTag, int>;
using BirthdayYear = userver::utils::StrongTypedef<class BirthdayYearTag, int>;

struct Birthday {
  BirthdayId id{};
  std::string person;
  std::optional<BirthdayYear> y{};
  BirthdayMonth m{};
  BirthdayDay d{};
  bool notification_enabled{};
  std::optional<TimePoint> last_notification_time;
};

}  // namespace telegram_bot::models