#include "http_client.hpp"

#include <userver/clients/http/form.hpp>
#include <userver/http/common_headers.hpp>

namespace telegram_bot::components::bot::impl {

TelegramApiHttpClient::TelegramApiHttpClient(
    userver::clients::http::Client& client)
    : client_{client} {}

std::string TelegramApiHttpClient::makeRequest(
    const TgBot::Url& url, const std::vector<TgBot::HttpReqArg>& args) const {
  const bool has_files = std::any_of(
      args.begin(), args.end(), [](const auto& arg) { return arg.isFile; });
  UINVARIANT(!has_files, "Unexpected file");
  auto request =
      client_.CreateRequest()
          .method(args.empty() ? userver::clients::http::HttpMethod::kGet
                               : userver::clients::http::HttpMethod::kPost)
          .url(url.protocol + "://" + url.host + url.path)
          .timeout(10000)  // TODO customize
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

}  // namespace telegram_bot::components::bot::impl
