#include <userver/clients/dns/component.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/server/handlers/ping.hpp>
#include <userver/server/handlers/server_monitor.hpp>
#include <userver/server/handlers/tests_control.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/secdist/component.hpp>
#include <userver/storages/secdist/provider_component.hpp>
#include <userver/testsuite/testsuite_support.hpp>
#include <userver/utils/daemon_run.hpp>

#include <components/birthday_notificator.hpp>
#include <components/bot/component.hpp>

int main(int argc, char* argv[]) {
  auto component_list =
      userver::components::MinimalServerComponentList()
          .Append<userver::clients::dns::Component>()
          .Append<userver::components::DefaultSecdistProvider>()
          .Append<userver::components::HttpClient>()
          .Append<userver::components::Postgres>("postgres-db")
          .Append<userver::components::Secdist>()
          .Append<userver::components::TestsuiteSupport>()
          .Append<userver::server::handlers::Ping>()
          .Append<userver::server::handlers::ServerMonitor>()
          .Append<userver::server::handlers::TestsControl>()
          .Append<telegram_bot::components::BirthdayNotificator>()
          .Append<telegram_bot::components::bot::Component>();

  return userver::utils::DaemonMain(argc, argv, component_list);
}
