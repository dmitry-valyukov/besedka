// Даты и отказы словами. Моменты строятся в том же поясе, в каком читаются,
// поэтому тест не зависит от того, где стоит машина.

#include <gtest/gtest.h>

#include <chrono>
#include <exception>
#include <stdexcept>

import besedka.app;
import besedka.forum;
import wxl.core;

using namespace besedka;
using namespace besedka::app;
using namespace std::chrono;

namespace {

const time_zone& zone() { return *current_zone(); }

/// Момент, который в этом поясе читается как данная дата и время.
system_clock::time_point at(const int y, const unsigned mo, const unsigned d, const int h,
                            const int mi) {
    return zone().to_sys(local_days{year{y} / month{mo} / day{d}} + hours{h} + minutes{mi});
}

}  // namespace

TEST(format, todays_message_shows_only_its_time) {
    EXPECT_EQ(relativeDate(at(2026, 9, 10, 15, 4), at(2026, 9, 10, 18, 30), zone()), L"15:04");
}

TEST(format, this_years_message_shows_day_and_month) {
    EXPECT_EQ(relativeDate(at(2026, 4, 3, 9, 5), at(2026, 9, 10, 18, 30), zone()), L"03.04 09:05");
}

TEST(format, last_years_message_shows_a_date_only) {
    EXPECT_EQ(relativeDate(at(2025, 12, 24, 23, 59), at(2026, 9, 10, 18, 30), zone()), L"24.12.25");
}

TEST(format, an_unread_date_is_shown_as_nothing) {
    EXPECT_EQ(relativeDate({}, at(2026, 9, 10, 18, 30), zone()), L"");
    EXPECT_EQ(fullDate({}, zone()), L"");
}

TEST(format, a_message_carries_its_full_date) {
    EXPECT_EQ(fullDate(at(2026, 9, 10, 15, 4), zone()), L"10.09.2026 15:04");
}

TEST(format, a_refusal_is_told_in_words_by_its_kind) {
    const auto unreachable =
        std::make_exception_ptr(forum::HttpError(0, wxl::core::unicode::repaired(L"нет сети")));
    const auto refused = std::make_exception_ptr(
        forum::HttpError(503, wxl::core::unicode::repaired(L"сервер ответил 503")));
    const auto other = std::make_exception_ptr(std::runtime_error("whatever"));

    EXPECT_EQ(reasonOf(unreachable), L"Сервер недоступен: нет сети");
    EXPECT_EQ(reasonOf(refused), L"Сервер отказал: сервер ответил 503");
    EXPECT_EQ(reasonOf(other), L"Не вышло поговорить с сервером.");
}
