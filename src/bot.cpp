#include "bot.hpp"

#include <chrono>
#include <exception>
#include <string>
#include <vector>

#include <userver/components/component_context.hpp>
#include <userver/engine/async.hpp>
#include <userver/engine/sleep.hpp>
#include <userver/engine/task/cancel.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/secdist/component.hpp>
#include <userver/tracing/span.hpp>

#include <tgbot/tgbot.h>

namespace telegram_bot {

namespace {

struct Token {
  std::string token;

  Token(const userver::formats::json::Value& doc)
      : token(doc["telegram_token"].As<std::string>()) {}
};

std::vector<std::string> bot_commands = {"start", "test"};

}  // namespace

const std::string Bot::kName = "telegram-bot";

Bot::Bot(const userver::components::ComponentConfig& config,
         const userver::components::ComponentContext& context)
    : userver::components::LoggableComponentBase(config, context) {
  const auto& secdist = context.FindComponent<userver::components::Secdist>();
  const auto& secdist_config = secdist.Get();
  telegram_token_ = secdist_config.Get<Token>().token;

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

    bool test_text_state = false;

    TgBot::Bot bot(telegram_token_);
    TgBot::TgLongPoll long_poll(bot);

    bot.getEvents().onCommand("start", [&bot](TgBot::Message::Ptr message) {
      bot.getApi().sendMessage(message->chat->id, "Hi!");
    });

    bot.getEvents().onCommand("test", [&](TgBot::Message::Ptr message) {
      bot.getApi().sendMessage(message->chat->id, "Enter text");
      test_text_state = true;
    });

    bot.getEvents().onAnyMessage([&](TgBot::Message::Ptr message) {
      if (test_text_state) {
        bot.getApi().sendMessage(message->chat->id, message->text);
        test_text_state = false;
        return;
      }

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
        LOG_INFO() << "Long poll started\n";
        long_poll.start();
      }
    } catch (std::exception& e) {
      LOG_ERROR() << "error: " << e.what();
    }
  }
}

}  // namespace telegram_bot
