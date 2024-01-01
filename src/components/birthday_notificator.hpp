#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <vector>

#include <cctz/time_zone.h>

#include <userver/storages/postgres/dist_lock_component_base.hpp>
#include <userver/storages/postgres/postgres_fwd.hpp>
#include <userver/utils/time_of_day.hpp>

#include <components/bot/component.hpp>

namespace telegram_bot::components {

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
  bot::Component& bot_;
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
  std::vector<models::BirthdayId> ids;
  std::vector<std::string> celebrate_today;
  std::vector<std::string> forgotten;
};

std::unordered_map<models::UserId, BirthdaysToNotify> FindBirthdaysToNotify(
    const std::vector<models::Birthday>& rows,
    const cctz::time_zone& notification_timezone,
    const cctz::civil_day& local_day);

}  // namespace impl

}  // namespace telegram_bot::components
