#include "birthday_notificator.hpp"

#include <userver/utest/utest.hpp>
#include <userver/utils/datetime/from_string_saturating.hpp>
#include <userver/utils/mock_now.hpp>

using telegram_bot::components::impl::FindBirthdaysToNotify;
using telegram_bot::models::Birthday;
using telegram_bot::models::BirthdayDay;
using telegram_bot::models::BirthdayMonth;

const telegram_bot::models::UserId kUserId1{1};
const telegram_bot::models::UserId kUserId2{2};

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

  auto result = FindBirthdaysToNotify(
      {
          Birthday{
              .person = "person1",
              .m = BirthdayMonth{2},
              .d = BirthdayDay{16},
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2022-02-16T15:00:00+0300"),
              .user_id = kUserId1},
          Birthday{.person = "person2",
                   .m = BirthdayMonth{2},
                   .d = BirthdayDay{16},
                   .notification_enabled = true,
                   .last_notification_time = std::nullopt,
                   .user_id = kUserId1},
          Birthday{
              .person = "person3",
              .m = BirthdayMonth{2},
              .d = BirthdayDay{16},
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2023-02-16T15:00:00+0300"),
              .user_id = kUserId1},
          Birthday{
              .person = "person4",
              .m = BirthdayMonth{2},
              .d = BirthdayDay{15},
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2022-02-15T15:00:00+0300"),
              .user_id = kUserId1},
          Birthday{
              .person = "person5",
              .m = BirthdayMonth{2},
              .d = BirthdayDay{12},
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2022-02-12T15:00:00+0300"),
              .user_id = kUserId1},
          Birthday{.person = "person6",
                   .m = BirthdayMonth{2},
                   .d = BirthdayDay{16},
                   .notification_enabled = false,
                   .last_notification_time = std::nullopt,
                   .user_id = kUserId1},
          Birthday{.person = "person7",
                   .m = BirthdayMonth{2},
                   .d = BirthdayDay{17},
                   .notification_enabled = true,
                   .last_notification_time = std::nullopt,
                   .user_id = kUserId1},
          Birthday{.person = "person8",
                   .m = BirthdayMonth{3},
                   .d = BirthdayDay{16},
                   .notification_enabled = true,
                   .last_notification_time = std::nullopt,
                   .user_id = kUserId1},
          Birthday{.person = "person9",
                   .m = BirthdayMonth{1},
                   .d = BirthdayDay{16},
                   .notification_enabled = true,
                   .last_notification_time = std::nullopt,
                   .user_id = kUserId1},
      },
      moscow_timezone, local_day);
  EXPECT_EQ(result.size(), 1);
  EXPECT_EQ(result[kUserId1].celebrate_today,
            std::vector<std::string>({"person1", "person2"}));
  EXPECT_EQ(result[kUserId1].forgotten,
            std::vector<std::string>{"person4 on 15.02"});
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

  auto result = FindBirthdaysToNotify(
      {
          Birthday{
              .person = "person1",
              .m = BirthdayMonth{2},
              .d = BirthdayDay{1},
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2022-02-01T15:00:00+0300"),
              .user_id = kUserId1},
          Birthday{
              .person = "person2",
              .m = BirthdayMonth{1},
              .d = BirthdayDay{31},
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2022-01-31T15:00:00+0300"),
              .user_id = kUserId1},
          Birthday{
              .person = "person3",
              .m = BirthdayMonth{1},
              .d = BirthdayDay{28},
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2022-01-28T15:00:00+0300"),
              .user_id = kUserId1},
      },
      moscow_timezone, local_day);
  EXPECT_EQ(result[kUserId1].celebrate_today,
            std::vector<std::string>({"person1"}));
  EXPECT_EQ(result[kUserId1].forgotten,
            std::vector<std::string>{"person2 on 31.01"});
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

  auto result = FindBirthdaysToNotify(
      {
          Birthday{
              .person = "person1",
              .m = BirthdayMonth{1},
              .d = BirthdayDay{1},
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2022-01-01T15:00:00+0300"),
              .user_id = kUserId1},
          Birthday{
              .person = "person2",
              .m = BirthdayMonth{12},
              .d = BirthdayDay{31},
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2021-12-31T15:00:00+0300"),
              .user_id = kUserId1},
          Birthday{
              .person = "person3",
              .m = BirthdayMonth{12},
              .d = BirthdayDay{28},
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2021-12-28T15:00:00+0300"),
              .user_id = kUserId1},
          Birthday{
              .person = "person4",
              .m = BirthdayMonth{12},
              .d = BirthdayDay{31},
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2022-12-31T15:00:00+0300"),
              .user_id = kUserId1},
      },
      moscow_timezone, local_day);
  EXPECT_EQ(result[kUserId1].celebrate_today,
            std::vector<std::string>({"person1"}));
  EXPECT_EQ(result[kUserId1].forgotten,
            std::vector<std::string>{"person2 on 31.12"});
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

  auto result = FindBirthdaysToNotify(
      {
          Birthday{
              .person = "person1",
              .m = BirthdayMonth{1},
              .d = BirthdayDay{1},
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2022-01-01T15:00:00+0300"),
              .user_id = kUserId1},
          Birthday{
              .person = "person2",
              .m = BirthdayMonth{1},
              .d = BirthdayDay{2},
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2022-01-02T15:00:00+0300"),
              .user_id = kUserId1},
      },
      vladivostok_timezone, local_day);
  EXPECT_EQ(result[kUserId1].celebrate_today,
            std::vector<std::string>({"person2"}));
  EXPECT_EQ(result[kUserId1].forgotten,
            std::vector<std::string>{"person1 on 01.01"});
}

UTEST(FindBirthdaysToNotify, SplitUsers) {
  userver::utils::datetime::MockNowSet(userver::utils::datetime::Stringtime(
      "2023-02-16T20:30:00+0300", "Europe/Moscow"));

  cctz::time_zone moscow_timezone;
  ASSERT_TRUE(cctz::load_time_zone("Europe/Moscow", &moscow_timezone));

  auto local_day = cctz::civil_day(
      cctz::convert(userver::utils::datetime::Now(), moscow_timezone));
  ASSERT_EQ(local_day.year(), 2023);
  ASSERT_EQ(local_day.month(), 2);
  ASSERT_EQ(local_day.day(), 16);

  auto result = FindBirthdaysToNotify(
      {
          Birthday{
              .person = "person1",
              .m = BirthdayMonth{2},
              .d = BirthdayDay{16},
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2022-02-16T15:00:00+0300"),
              .user_id = kUserId1},
          Birthday{.person = "person2",
                   .m = BirthdayMonth{2},
                   .d = BirthdayDay{16},
                   .notification_enabled = true,
                   .last_notification_time = std::nullopt,
                   .user_id = kUserId2},
          Birthday{
              .person = "person3",
              .m = BirthdayMonth{2},
              .d = BirthdayDay{16},
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2023-02-16T15:00:00+0300"),
              .user_id = kUserId1},
          Birthday{
              .person = "person4",
              .m = BirthdayMonth{2},
              .d = BirthdayDay{15},
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2022-02-15T15:00:00+0300"),
              .user_id = kUserId2},
          Birthday{
              .person = "person5",
              .m = BirthdayMonth{2},
              .d = BirthdayDay{12},
              .notification_enabled = true,
              .last_notification_time = userver::utils::datetime::Stringtime(
                  "2022-02-12T15:00:00+0300"),
              .user_id = kUserId1},
          Birthday{.person = "person6",
                   .m = BirthdayMonth{2},
                   .d = BirthdayDay{16},
                   .notification_enabled = false,
                   .last_notification_time = std::nullopt,
                   .user_id = kUserId2},
          Birthday{.person = "person7",
                   .m = BirthdayMonth{2},
                   .d = BirthdayDay{17},
                   .notification_enabled = true,
                   .last_notification_time = std::nullopt,
                   .user_id = kUserId1},
          Birthday{.person = "person8",
                   .m = BirthdayMonth{3},
                   .d = BirthdayDay{16},
                   .notification_enabled = true,
                   .last_notification_time = std::nullopt,
                   .user_id = kUserId2},
          Birthday{.person = "person9",
                   .m = BirthdayMonth{1},
                   .d = BirthdayDay{16},
                   .notification_enabled = true,
                   .last_notification_time = std::nullopt,
                   .user_id = kUserId1},
      },
      moscow_timezone, local_day);
  EXPECT_EQ(result.size(), 2);
  EXPECT_EQ(result[kUserId1].celebrate_today,
            std::vector<std::string>({"person1"}));
  EXPECT_EQ(result[kUserId1].forgotten, std::vector<std::string>{});
  EXPECT_EQ(result[kUserId2].celebrate_today,
            std::vector<std::string>({"person2"}));
  EXPECT_EQ(result[kUserId2].forgotten,
            std::vector<std::string>{"person4 on 15.02"});
}
