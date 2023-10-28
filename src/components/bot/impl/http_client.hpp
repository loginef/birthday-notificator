#pragma once

#include <vector>

#include <userver/clients/http/client.hpp>

#include <tgbot/net/HttpClient.h>
#include <tgbot/net/HttpReqArg.h>
#include <tgbot/net/Url.h>

namespace telegram_bot::components::bot::impl {

class TelegramApiHttpClient final : public TgBot::HttpClient {
 public:
  explicit TelegramApiHttpClient(userver::clients::http::Client& client);

  virtual std::string makeRequest(
      const TgBot::Url& url,
      const std::vector<TgBot::HttpReqArg>& args) const override;

 private:
  userver::clients::http::Client& client_;
};

}  // namespace telegram_bot::components::bot::impl
