#include "bot.hpp"

#include <chrono>
#include <exception>
#include <string>
#include <vector>

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

std::vector<std::string> bot_commands = {"start", "chat_id"};

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
            ->method(args.empty() ? userver::clients::http::HttpMethod::kGet
                                  : userver::clients::http::HttpMethod::kPost)
            ->url(url.protocol + "://" + url.host + url.path)
            ->timeout(500)  // TODO customize
            ->headers(
                {{userver::http::headers::kContentType, "multipart/form-data"}})
            ->retry(1);

    if (!args.empty()) {
      userver::clients::http::Form form;
      for (const auto& arg : args) {
        form.AddContent(arg.name, arg.value);
      }
      request->form(form);
    }
    LOG_DEBUG() << "Request: " << request->GetUrl();

    auto response = request->perform();
    response->raise_for_status();
    return std::move(*response).body();
  }

 private:
  userver::clients::http::Client& client_;
};

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
