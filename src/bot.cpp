#include "bot.hpp"

#include <chrono>
#include <exception>
#include <string>
#include <tuple>
#include <vector>

#include <cctz/time_zone.h>
#include <fmt/format.h>

#include <userver/clients/http/client.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/clients/http/form.hpp>
#include <userver/clients/http/response.hpp>
#include <userver/components/component_context.hpp>
#include <userver/engine/async.hpp>
#include <userver/engine/sleep.hpp>
#include <userver/engine/task/cancel.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/http/common_headers.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/secdist/component.hpp>
#include <userver/testsuite/testpoint.hpp>
#include <userver/tracing/span.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

#include <tgbot/tgbot.h>

namespace telegram_bot {

namespace {

struct Token {
  std::string token;

  Token(const userver::formats::json::Value& doc)
      : token(doc["telegram_token"].As<std::string>()) {}
};

std::vector<std::string> bot_commands = {"start", "chat_id", "next_birthdays"};

std::string GetToken(const userver::components::ComponentContext& context) {
  const auto& secdist = context.FindComponent<userver::components::Secdist>();
  const auto& secdist_config = secdist.Get();
  return secdist_config.Get<Token>().token;
}

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

class TelegramApiHttpClient final : public TgBot::HttpClient {
 public:
  explicit TelegramApiHttpClient(userver::clients::http::Client& client)
      : client_{client} {}

  virtual std::string makeRequest(
      const TgBot::Url& url,
      const std::vector<TgBot::HttpReqArg>& args) const override {
    const bool has_files = std::any_of(
        args.begin(), args.end(), [](const auto& arg) { return arg.isFile; });
    UINVARIANT(!has_files, "Unexpected file");
    auto request =
        client_.CreateRequest()
            .method(args.empty() ? userver::clients::http::HttpMethod::kGet
                                 : userver::clients::http::HttpMethod::kPost)
            .url(url.protocol + "://" + url.host + url.path)
            .timeout(500)  // TODO customize
            .headers(
                {{userver::http::headers::kContentType, "multipart/form-data"}})
            .retry(1);

    if (!args.empty()) {
      userver::clients::http::Form form;
      for (const auto& arg : args) {
        form.AddContent(arg.name, arg.value);
      }
      request.form(form);
    }
    LOG_DEBUG() << "Request: " << request.GetUrl();

    auto response = request.perform();
    response->raise_for_status();
    return std::move(*response).body();
  }

 private:
  userver::clients::http::Client& client_;
};

const std::string kAllBirthdaysQuery = R"(
SELECT
  birthdays.person,
  birthdays.m,
  birthdays.d
FROM birthday.birthdays
)";

struct NextBirthday {
  std::string person;
  int m{};
  int d{};
};

std::string GetNextBirthdaysMessage(
    const int64_t sender_chat_id, const int64_t allowed_chat_id,
    userver::storages::postgres::Cluster& postgres) {
  if (sender_chat_id != allowed_chat_id) {
    return "No birthdays for you, sorry";
  }

  const auto rows =
      postgres.Execute(userver::storages::postgres::ClusterHostType::kMaster,
                       kAllBirthdaysQuery);
  if (rows.IsEmpty()) {
    return "There are no birthdays";
  }
  auto list = rows.AsContainer<std::vector<NextBirthday>>(
      userver::storages::postgres::kRowTag);

  cctz::time_zone notification_timezone;
  if (!cctz::load_time_zone("Europe/Moscow", &notification_timezone)) {
    throw std::runtime_error("Unknown timezone Europe/Moscow");
  }
  const auto now = userver::utils::datetime::Now();
  LOG_DEBUG() << "at " << userver::utils::datetime::Timestring(now);
  const auto local_time = cctz::convert(now, notification_timezone);
  const auto local_day = cctz::civil_day(local_time);

  std::sort(list.begin(), list.end(),
            [local_day](const NextBirthday& lhs, const NextBirthday& rhs) {
              const int l_year = std::tuple(lhs.m, lhs.d) <
                                 std::tuple(local_day.month(), local_day.day());
              const int r_year = std::tuple(rhs.m, rhs.d) <
                                 std::tuple(local_day.month(), local_day.day());
              return std::tuple(l_year, lhs.m, lhs.d) <
                     std::tuple(r_year, rhs.m, rhs.d);
            });
  if (list.size() > 5) {
    list.resize(5);
  }

  std::string result = "Next birthdays:";
  for (const auto& birthday : list) {
    result += fmt::format("\n{} on {:02}.{:02}", birthday.person, birthday.d,
                          birthday.m);
  }
  return result;
}

}  // namespace

const std::string Bot::kName = "telegram-bot";

userver::yaml_config::Schema Bot::GetStaticConfigSchema() {
  return userver::yaml_config::MergeSchemas<
      userver::components::LoggableComponentBase>(kComponentConfigSchema);
}

Bot::Bot(const userver::components::ComponentConfig& config,
         const userver::components::ComponentContext& context)
    : userver::components::LoggableComponentBase(config, context),
      http_client_{context.FindComponent<userver::components::HttpClient>()
                       .GetHttpClient()},
      telegram_token_(GetToken(context)),
      chat_id_(config["chat_id"].As<int64_t>()),
      telegram_host_(config["telegram_host"].As<std::string>()),
      postgres_(
          context.FindComponent<userver::components::Postgres>("postgres-db")
              .GetCluster()),
      queue_(userver::concurrent::MpscQueue<
             std::unique_ptr<SendMessageRequest>>::Create()),
      consumer_(queue_->GetConsumer()) {
  Start();
}

void Bot::Start() {
  userver::tracing::Span span(kName);
  span.DetachFromCoroStack();
  task_ =
      userver::engine::AsyncNoSpan([this, span = std::move(span)]() mutable {
        span.AttachToCoroStack();
        Run();
      });
}

void Bot::Run() {
  while (!userver::engine::current_task::ShouldCancel()) {
    userver::engine::InterruptibleSleepFor(std::chrono::seconds(1));

    TelegramApiHttpClient client{http_client_};
    TgBot::Bot bot(telegram_token_, client, telegram_host_);
    TgBot::TgLongPoll long_poll(bot, 100, 1);

    bot.getEvents().onCommand("start", [&bot](TgBot::Message::Ptr message) {
      bot.getApi().sendMessage(message->chat->id, "Hi!");
    });

    bot.getEvents().onCommand("chat_id", [&](TgBot::Message::Ptr message) {
      bot.getApi().sendMessage(
          message->chat->id,
          fmt::format("Your chat id is {}", message->chat->id));
    });

    bot.getEvents().onCommand(
        "next_birthdays", [&](TgBot::Message::Ptr message) {
          bot.getApi().sendMessage(
              message->chat->id,
              GetNextBirthdaysMessage(message->chat->id, chat_id_, *postgres_));
        });

    bot.getEvents().onAnyMessage([&](TgBot::Message::Ptr message) {
      for (const auto& command : bot_commands) {
        if ("/" + command == message->text) {
          return;
        }
      }

      bot.getApi().sendMessage(message->chat->id, "Unknown command");
    });

    try {
      LOG_DEBUG() << "Bot username: " << bot.getApi().getMe()->username;
      bot.getApi().deleteWebhook();

      while (!userver::engine::current_task::ShouldCancel()) {
        LOG_INFO() << "Long poll started";
        long_poll.start();
        LOG_INFO() << "Long poll finished, process other requests from other "
                      "components";

        auto consumer_task = userver::utils::Async("consumer", [this, &bot] {
          while (true) {
            std::unique_ptr<SendMessageRequest> request;
            if (consumer_.Pop(request, userver::engine::Deadline::FromDuration(
                                           std::chrono::milliseconds(1)))) {
              UINVARIANT(request, "request is null");
              bot.getApi().sendMessage(chat_id_, request->text);
            } else {
              // the queue is empty and there are no more live producers
              return;
            }
          }
        });
        consumer_task.Get();
      }
    } catch (std::exception& e) {
      LOG_ERROR() << "error: " << e.what();
    }
  }
}

void Bot::SendMessage(const std::string& text) {
  auto producer = queue_->GetProducer();
  auto producer_task = userver::utils::Async("producer", [&text, &producer] {
    auto request =
        std::make_unique<SendMessageRequest>(SendMessageRequest{.text = text});
    if (!producer.Push(std::move(request),
                       userver::engine::Deadline::FromDuration(
                           std::chrono::seconds(60)))) {
      throw std::runtime_error("Producer push timeout");
    }
  });
  producer_task.Get();
}

}  // namespace telegram_bot
