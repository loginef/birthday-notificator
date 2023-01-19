#include "birthday_notificator.hpp"

#include <userver/utest/utest.hpp>
#include <userver/utils/datetime/from_string_saturating.hpp>
#include <userver/utils/mock_now.hpp>

UTEST(FindBirthdaysToNotify, BasicChecks) {
  userver::utils::datetime::MockNowSet(userver::utils::datetime::Stringtime(
      "2023-02-16T20:30:00+0300", "Europe/Moscow"));

  cctz::time_zone moscow_timezone;
  ASSERT_TRUE(cctz::load_time_zone("Europe/Moscow", &moscow_timezone));

  auto local_day = cctz::civil_day(
      cctz::convert(userver::utils::datetime::Now(), moscow_timezone));
  ASSERT_EQ(local_day.year(), 2023);
  ASSERT_EQ(local_day.month(), 2);
  ASSERT_EQ(local_day.day(), 16);

  auto result = telegram_bot::impl::FindBirthdaysToNotify(
      {
          telegram_bot::impl::BirthdayRow{
              .person = "person1",
              .m = 2,
              .d = 16,
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2022-02-16T15:00:00+0300")},
          telegram_bot::impl::BirthdayRow{
              .person = "person2",
              .m = 2,
              .d = 16,
              .notification_enabled = true,
              .last_notification_time = std::nullopt},
          telegram_bot::impl::BirthdayRow{
              .person = "person3",
              .m = 2,
              .d = 16,
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2023-02-16T15:00:00+0300")},
          telegram_bot::impl::BirthdayRow{
              .person = "person4",
              .m = 2,
              .d = 15,
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2022-02-15T15:00:00+0300")},
          telegram_bot::impl::BirthdayRow{
              .person = "person5",
              .m = 2,
              .d = 12,
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2022-02-12T15:00:00+0300")},
          telegram_bot::impl::BirthdayRow{
              .person = "person6",
              .m = 2,
              .d = 16,
              .notification_enabled = false,
              .last_notification_time = std::nullopt},
          telegram_bot::impl::BirthdayRow{
              .person = "person7",
              .m = 2,
              .d = 17,
              .notification_enabled = true,
              .last_notification_time = std::nullopt},
          telegram_bot::impl::BirthdayRow{
              .person = "person8",
              .m = 3,
              .d = 16,
              .notification_enabled = true,
              .last_notification_time = std::nullopt},
          telegram_bot::impl::BirthdayRow{
              .person = "person9",
              .m = 1,
              .d = 16,
              .notification_enabled = true,
              .last_notification_time = std::nullopt},
      },
      moscow_timezone, local_day);
  EXPECT_EQ(result.celebrate_today,
            std::vector<std::string>({"person1", "person2"}));
  EXPECT_EQ(result.forgotten, std::vector<std::string>{"person4 on 15.02"});
}

UTEST(FindBirthdaysToNotify, MonthBorder) {
  userver::utils::datetime::MockNowSet(userver::utils::datetime::Stringtime(
      "2023-02-01T20:30:00+0300", "Europe/Moscow"));

  cctz::time_zone moscow_timezone;
  ASSERT_TRUE(cctz::load_time_zone("Europe/Moscow", &moscow_timezone));

  auto local_day = cctz::civil_day(
      cctz::convert(userver::utils::datetime::Now(), moscow_timezone));
  ASSERT_EQ(local_day.year(), 2023);
  ASSERT_EQ(local_day.month(), 2);
  ASSERT_EQ(local_day.day(), 1);

  auto result = telegram_bot::impl::FindBirthdaysToNotify(
      {
          telegram_bot::impl::BirthdayRow{
              .person = "person1",
              .m = 2,
              .d = 1,
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2022-02-01T15:00:00+0300")},
          telegram_bot::impl::BirthdayRow{
              .person = "person2",
              .m = 1,
              .d = 31,
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2022-01-31T15:00:00+0300")},
          telegram_bot::impl::BirthdayRow{
              .person = "person3",
              .m = 1,
              .d = 28,
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2022-01-28T15:00:00+0300")},
      },
      moscow_timezone, local_day);
  EXPECT_EQ(result.celebrate_today, std::vector<std::string>({"person1"}));
  EXPECT_EQ(result.forgotten, std::vector<std::string>{"person2 on 31.01"});
}

UTEST(FindBirthdaysToNotify, YearBorder) {
  userver::utils::datetime::MockNowSet(userver::utils::datetime::Stringtime(
      "2023-01-01T20:30:00+0300", "Europe/Moscow"));

  cctz::time_zone moscow_timezone;
  ASSERT_TRUE(cctz::load_time_zone("Europe/Moscow", &moscow_timezone));

  auto local_day = cctz::civil_day(
      cctz::convert(userver::utils::datetime::Now(), moscow_timezone));
  ASSERT_EQ(local_day.year(), 2023);
  ASSERT_EQ(local_day.month(), 1);
  ASSERT_EQ(local_day.day(), 1);

  auto result = telegram_bot::impl::FindBirthdaysToNotify(
      {
          telegram_bot::impl::BirthdayRow{
              .person = "person1",
              .m = 1,
              .d = 1,
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2022-01-01T15:00:00+0300")},
          telegram_bot::impl::BirthdayRow{
              .person = "person2",
              .m = 12,
              .d = 31,
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2021-12-31T15:00:00+0300")},
          telegram_bot::impl::BirthdayRow{
              .person = "person3",
              .m = 12,
              .d = 28,
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2021-12-28T15:00:00+0300")},
          telegram_bot::impl::BirthdayRow{
              .person = "person4",
              .m = 12,
              .d = 31,
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2022-12-31T15:00:00+0300")},
      },
      moscow_timezone, local_day);
  EXPECT_EQ(result.celebrate_today, std::vector<std::string>({"person1"}));
  EXPECT_EQ(result.forgotten, std::vector<std::string>{"person2 on 31.12"});
}

UTEST(FindBirthdaysToNotify, TimezoneApplication) {
  userver::utils::datetime::MockNowSet(userver::utils::datetime::Stringtime(
      "2023-01-01T20:30:00+0300", "Europe/Moscow"));

  cctz::time_zone vladivostok_timezone;
  ASSERT_TRUE(cctz::load_time_zone("Asia/Vladivostok", &vladivostok_timezone));

  auto local_day = cctz::civil_day(
      cctz::convert(userver::utils::datetime::Now(), vladivostok_timezone));
  ASSERT_EQ(local_day.year(), 2023);
  ASSERT_EQ(local_day.month(), 1);
  ASSERT_EQ(local_day.day(), 2);

  auto result = telegram_bot::impl::FindBirthdaysToNotify(
      {
          telegram_bot::impl::BirthdayRow{
              .person = "person1",
              .m = 1,
              .d = 1,
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2022-01-01T15:00:00+0300")},
          telegram_bot::impl::BirthdayRow{
              .person = "person2",
              .m = 1,
              .d = 2,
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2022-01-02T15:00:00+0300")},
      },
      vladivostok_timezone, local_day);
  EXPECT_EQ(result.celebrate_today, std::vector<std::string>({"person2"}));
  EXPECT_EQ(result.forgotten, std::vector<std::string>{"person1 on 01.01"});
}
