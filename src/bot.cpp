#include "bot.hpp"

#include <chrono>
#include <exception>
#include <string>
#include <vector>

#include <fmt/format.h>

#include <userver/components/component_context.hpp>
#include <userver/engine/async.hpp>
#include <userver/engine/sleep.hpp>
#include <userver/engine/task/cancel.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/secdist/component.hpp>
#include <userver/testsuite/testpoint.hpp>
#include <userver/tracing/span.hpp>

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

}  // namespace

const std::string Bot::kName = "telegram-bot";

Bot::Bot(const userver::components::ComponentConfig& config,
         const userver::components::ComponentContext& context)
    : userver::components::LoggableComponentBase(config, context),
      telegram_token_(GetToken(context)),
      chat_id_(config["chat_id"].As<int64_t>()),
      fake_api_(config["fake_api"].As<bool>()),
      queue_(userver::concurrent::MpscQueue<
             std::unique_ptr<SendMessageRequest>>::Create()),
      consumer_(queue_->GetConsumer()) {
  if (!fake_api_) {
    Start();
  }
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

    // TODO override http client
    TgBot::Bot bot(telegram_token_);
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

      bot.getApi().sendMessage(message->chat->id, "unknown command");
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
          // UINVARIANT(false, "falsse");
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
  TESTPOINT("bot-send-message", [&]() {
    userver::formats::json::ValueBuilder builder;
    builder["text"] = text;
    return builder.ExtractValue();
  }());

  if (fake_api_) {
    return;
  }

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
  // UINVARIANT(false, "falsse");
}

}  // namespace telegram_bot
