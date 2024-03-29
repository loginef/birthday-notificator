#include "birthday_notificator.hpp"

#include <chrono>
#include <exception>
#include <string>
#include <tuple>
#include <vector>

#include <fmt/format.h>

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
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
#include <userver/yaml_config/merge_schemas.hpp>

#include <db/birthdays.hpp>
#include <db/users.hpp>

namespace telegram_bot::components {

namespace {

const std::chrono::days kForgottenBirthdaySearchDistance(3);

template <typename T>
userver::formats::json::ValueBuilder SerializeArray(const T& array) {
  userver::formats::json::ValueBuilder json_array(
      userver::formats::json::Type::kArray);

  for (const auto& item : array) {
    json_array.PushBack(item);
  }

  return json_array;
}

const std::string kComponentConfigSchema = R"(
type: object
description: ClickHouse client component
additionalProperties: false
properties:
    notification_time_of_day:
        description: Hour and minute after which notification can be sent
        type: string
    notification_timezone:
        description: Timezone name for time of day calculation
        type: string
)";

}  // namespace

const std::string BirthdayNotificator::kName = "birthday-notificator";

userver::yaml_config::Schema BirthdayNotificator::GetStaticConfigSchema() {
  return userver::yaml_config::MergeSchemas<
      userver::storages::postgres::DistLockComponentBase>(
      kComponentConfigSchema);
}

BirthdayNotificator::BirthdayNotificator(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : userver::storages::postgres::DistLockComponentBase(config, context),
      postgres_(
          context.FindComponent<userver::components::Postgres>("postgres-db")
              .GetCluster()),
      bot_(context.FindComponent<bot::Component>()) {
  const std::string timezone_name =
      config["notification_timezone"].As<std::string>();
  if (!cctz::load_time_zone(timezone_name, &notification_timezone_)) {
    throw std::runtime_error("Unknown timezone " + timezone_name);
  }

  notification_time_of_day_ =
      userver::utils::datetime::TimeOfDay<std::chrono::minutes>(
          config["notification_time_of_day"].As<std::string>());

  AutostartDistLock();
}

BirthdayNotificator::~BirthdayNotificator() { StopDistLock(); }

void BirthdayNotificator::DoWorkTestsuite() {
  try {
    RunIteration();
  } catch (const std::exception& exc) {
    LOG_ERROR() << exc.what();
  }
}

void BirthdayNotificator::DoWork() {
  while (!userver::engine::current_task::ShouldCancel()) {
    RunIteration();
    userver::engine::InterruptibleSleepFor(std::chrono::minutes(10));
  }
}

void BirthdayNotificator::RunIteration() {
  userver::tracing::Span span(kName);
  LOG_INFO() << "Start birthday-notificator iteration";
  const auto now = userver::utils::datetime::Now();
  LOG_DEBUG() << "at " << userver::utils::datetime::Timestring(now);
  const auto local_time = cctz::convert(now, notification_timezone_);
  const auto local_day = cctz::civil_day(local_time);
  const auto local_notification_hour =
      cctz::civil_hour(local_day) + notification_time_of_day_.Hours().count();
  const auto local_notification_time =
      cctz::civil_minute(local_notification_hour) +
      notification_time_of_day_.Minutes().count();
  // const auto notification_time =
  //     cctz::convert(local_notification_time, notification_timezone_);
  // LOG_INFO() << "Notification time: "
  //            << userver::utils::datetime::Timestring(notification_time);

  // TODO dangerous if notification time is 23:50 or later
  if (local_time < local_notification_time) {
    LOG_DEBUG() << "Notification time not reached, exit";
    return;
  }

  const auto rows = db::FetchAllBirthdays(*postgres_);
  const auto birthdays_to_notify =
      impl::FindBirthdaysToNotify(rows, notification_timezone_, local_day);

  TESTPOINT("birthday-notificator", [&birthdays_to_notify]() {
    userver::formats::json::ValueBuilder builder;
    for (const auto& [user_id, birthdays] : birthdays_to_notify) {
      userver::formats::json::ValueBuilder user_builder;
      user_builder["forgotten"] = SerializeArray(birthdays.forgotten);
      user_builder["celebrate_today"] =
          SerializeArray(birthdays.celebrate_today);
      builder[std::to_string(user_id.GetUnderlying())] = user_builder;
    }
    return builder.ExtractValue();
  }());

  for (const auto& [user_id, birthdays] : birthdays_to_notify) {
    std::vector<std::string> lines;
    if (!birthdays.celebrate_today.empty()) {
      lines.push_back(fmt::format("Today is birthday of {}",
                                  fmt::join(birthdays.celebrate_today, ", ")));
    }
    if (!birthdays.forgotten.empty()) {
      lines.push_back(fmt::format("You forgot about birthdays: \n{}",
                                  fmt::join(birthdays.forgotten, "\n")));
    }

    if (lines.empty()) {
      continue;
    }

    const auto chat_id = db::GetChatId(user_id, *postgres_);
    bot_.SendMessage(chat_id, fmt::format("{}", fmt::join(lines, "\n")));

    for (const auto id : birthdays.ids) {
      db::UpdateBirthdayLastNotificationTime(now, id, *postgres_);
    }
  }
}

namespace impl {

std::unordered_map<models::UserId, BirthdaysToNotify> FindBirthdaysToNotify(
    const std::vector<models::Birthday>& rows,
    const cctz::time_zone& notification_timezone,
    const cctz::civil_day& local_day) {
  const auto farthest_forgotten_day =
      local_day - kForgottenBirthdaySearchDistance.count();

  std::unordered_map<models::UserId, BirthdaysToNotify> result;
  for (const auto& row : rows) {
    if (!row.notification_enabled) {
      LOG_DEBUG() << "Skip birthday with disabled notification";
      continue;
    }
    const auto birthday_year =
        cctz::civil_year(local_day) -
        (std::tuple(row.m, row.d) <=
                 std::tuple(models::BirthdayMonth{local_day.month()},
                            models::BirthdayDay{local_day.day()})
             ? 0
             : 1);
    const auto birthday = cctz::civil_day(cctz::civil_month(birthday_year) +
                                          row.m.GetUnderlying() - 1) +
                          row.d.GetUnderlying() - 1;
    if (birthday < farthest_forgotten_day) {
      LOG_DEBUG() << "Skip too old birthday";
      continue;
    }

    // TODO May cause duplicate notification on timezone change, deal with it
    // later
    if (row.last_notification_time.has_value() &&
        *row.last_notification_time >
            cctz::convert(birthday, notification_timezone)) {
      LOG_DEBUG() << "Skip already notified birthday";
      continue;
    }

    if (birthday == local_day) {
      result[row.user_id].ids.push_back(row.id);
      result[row.user_id].celebrate_today.push_back(row.person);
    } else {
      result[row.user_id].ids.push_back(row.id);
      result[row.user_id].forgotten.push_back(
          fmt::format("{} on {:02}.{:02}", row.person, row.d, row.m));
    }
  }
  return result;
}

}  // namespace impl

}  // namespace telegram_bot::components
