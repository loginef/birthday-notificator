#pragma once

#include <atomic>
#include <string>
#include <vector>

#include <cctz/time_zone.h>

#include <userver/storages/postgres/dist_lock_component_base.hpp>
#include <userver/storages/postgres/postgres_fwd.hpp>
#include <userver/utils/time_of_day.hpp>

#include "bot.hpp"

namespace telegram_bot {

class BirthdayNotificator final
    : public userver::storages::postgres::DistLockComponentBase {
 public:
  static const std::string kName;

  BirthdayNotificator(const userver::components::ComponentConfig&,
                      const userver::components::ComponentContext&);

  ~BirthdayNotificator() override;

  static userver::yaml_config::Schema GetStaticConfigSchema();

 private:
  userver::storages::postgres::ClusterPtr postgres_;
  Bot& bot_;
  cctz::time_zone notification_timezone_;
  userver::utils::datetime::TimeOfDay<std::chrono::minutes>
      notification_time_of_day_;

 private:
  void DoWork() override;
  void DoWorkTestsuite() override;
  void RunIteration();
};

namespace impl {

struct BirthdaysToNotify {
  std::vector<int> ids;
  std::vector<std::string> celebrate_today;
  std::vector<std::string> forgotten;
};

struct BirthdayRow {
  int id{};
  std::string person;
  int m{};
  int d{};
  bool notification_enabled{};
  std::optional<std::chrono::system_clock::time_point> last_notification_time;
};

BirthdaysToNotify FindBirthdaysToNotify(
    const std::vector<BirthdayRow>& rows,
    const cctz::time_zone& notification_timezone,
    const cctz::civil_day& local_day);

}  // namespace impl

}  // namespace telegram_bot
