// Тесты разбора ответов: кусок настоящего JSON с api.rsdn.org на входе,
// модель на выходе. Ответы взяты с живого сервера 6 сентября 2026 и
// урезаны -- в них важно не содержание, а форма: какие поля есть, каких
// нет и как записано время.
//
// Сети здесь нет ни одного запроса. Проверять разбор на живом форуме
// значило бы проверять заодно и погоду; для живого форума есть проба
// (forum-probe).

#include <gtest/gtest.h>

#include <string>

import besedka.forum;
import wxl.json;
import wxl.text;

using namespace besedka;
using namespace std::chrono;

namespace {

/// Разбор куска JSON: тесту нужно и дерево, и документ, которым оно живо.
class parsed {
public:
    explicit parsed(std::string_view source) : root_(document_.load(std::string(source))) {}

    const wxl::json::value& operator*() const noexcept { return root_; }

private:
    wxl::json::document document_;
    const wxl::json::value& root_;
};

/// Момент времени в UTC, собранный по частям, -- то, с чем сравнивается
/// прочитанное.
system_clock::time_point utc(int y, unsigned mo, unsigned d, int h, int mi, int s, int ms = 0) {
    return time_point_cast<system_clock::duration>(
        sys_days{year{y} / month{mo} / day{d}} + hours{h} + minutes{mi} + seconds{s} +
        milliseconds{ms});
}

TEST(forum, time_comes_back_in_utc) {
    // Смещение вычитается: 16:11 по Москве -- это 13:11 по Гринвичу.
    EXPECT_EQ(forum::readTimestamp(u8"2026-08-29T16:11:36.023+03:00"),
              utc(2026, 8, 29, 13, 11, 36, 23));

    // Семь знаков дробной части сервер шлёт в /service/info; лишние знаки
    // дочитываются и выбрасываются.
    EXPECT_EQ(forum::readTimestamp(u8"2026-09-06T09:43:30.6053529+00:00"),
              utc(2026, 9, 6, 9, 43, 30, 605));

    // Без дробной части и с Z вместо смещения.
    EXPECT_EQ(forum::readTimestamp(u8"2026-01-02T03:04:05Z"), utc(2026, 1, 2, 3, 4, 5));

    // Западное смещение прибавляет.
    EXPECT_EQ(forum::readTimestamp(u8"2026-01-02T03:04:05-05:00"), utc(2026, 1, 2, 8, 4, 5));

    // Дробная часть в один знак -- это десятые.
    EXPECT_EQ(forum::readTimestamp(u8"2026-01-02T03:04:05.5Z"), utc(2026, 1, 2, 3, 4, 5, 500));
}

TEST(forum, a_time_that_does_not_read_is_the_epoch_and_not_a_refusal) {
    EXPECT_EQ(forum::readTimestamp(u8""), system_clock::time_point{});
    EXPECT_EQ(forum::readTimestamp(u8"вчера"), system_clock::time_point{});
    EXPECT_EQ(forum::readTimestamp(u8"2026-13-45T99:99:99Z"), system_clock::time_point{});
}

TEST(forum, the_showcase_is_read_the_way_the_server_writes_it) {
    const parsed answer(R"json([
        {"id":1,"code":"rsdn","name":"Обсуждение сайта",
         "description":"Как наш с вами сайт сделать ещё лучше :o)",
         "forumGroup":{"id":3,"name":"Сайт","sortOrder":500},
         "isInTop":true,"isSiteSubject":true,"isService":false,"isRated":true,
         "rateLimit":5,"isWriteAllowed":true},
        {"id":3,"code":"winapi","name":"WIN API","description":"",
         "forumGroup":{"id":10,"name":"Программирование :: Microsoft Windows","sortOrder":120},
         "isInTop":true,"isSiteSubject":true,"isService":false,"isRated":true,
         "rateLimit":32656,"isWriteAllowed":false}
    ])json");

    const std::vector<forum::ForumDescription> forums = forum::readForums(*answer);

    ASSERT_EQ(forums.size(), 2u);

    EXPECT_EQ(forums[0].id, 1);
    EXPECT_EQ(forums[0].code, L"rsdn");
    EXPECT_EQ(forums[0].name, L"Обсуждение сайта");
    EXPECT_EQ(forums[0].group.name, L"Сайт");
    EXPECT_EQ(forums[0].group.sortOrder, 500);
    EXPECT_TRUE(forums[0].isWriteAllowed);

    EXPECT_EQ(forums[1].code, L"winapi");
    EXPECT_EQ(forums[1].description, L"");
    EXPECT_FALSE(forums[1].isWriteAllowed);
    EXPECT_EQ(forums[1].group.name, L"Программирование :: Microsoft Windows");
}

TEST(forum, a_page_of_topics_carries_its_place_in_the_whole) {
    const parsed answer(R"json({"items":[
        {"id":9133148,"forumID":1,"topicID":9133148,
         "author":{"id":73,"displayName":"VladD2",
                   "gravatarHash":"5ba1a0d1b82754225263f039b0dcc19c","role":"Admin"},
         "subject":"Вроде починил неотображение картинок",
         "createdOn":"2026-08-29T16:11:36.023+03:00","updatedOn":"2026-08-31T21:24:25.11+03:00",
         "isTopic":true,"parentID":0,"answersCount":3}
    ],"total":6839,"offset":0})json");

    const forum::MessagePage page = forum::readMessagePage(*answer);

    ASSERT_EQ(page.items.size(), 1u);
    EXPECT_EQ(page.total, 6839);
    EXPECT_EQ(page.offset, 0);

    const forum::MessageInfo& topic = page.items[0].info;

    EXPECT_EQ(topic.id, 9133148);
    EXPECT_EQ(topic.forumId, 1);
    EXPECT_EQ(topic.topicId, topic.id);
    EXPECT_EQ(topic.parentId, 0);
    EXPECT_TRUE(topic.isTopic);
    EXPECT_EQ(topic.answersCount, 3);
    EXPECT_EQ(topic.subject, L"Вроде починил неотображение картинок");
    EXPECT_EQ(topic.author.displayName, L"VladD2");
    EXPECT_EQ(topic.author.role, L"Admin");
    EXPECT_EQ(topic.createdOn, utc(2026, 8, 29, 13, 11, 36, 23));
}

TEST(forum, a_thread_arrives_with_its_bodies_in_one_answer) {
    // Тела приходят прямо в списке, когда их попросили: так дерево темы
    // читается одним запросом, а не одним на список и сотней на сообщения.
    const parsed answer(R"json({"items":[
        {"id":9134665,"forumID":1,"topicID":9133148,"parentID":9133148,
         "author":{"id":81729,"displayName":"Stanislaw K","role":"User"},
         "subject":"Re: Вроде починил","createdOn":"2026-08-31T12:30:33.9+03:00",
         "isTopic":false,"answersCount":0,
         "body":{"isFormatted":false,"text":"VD>банилка\r\n\r\nЯ одного вычислил"}}
    ],"total":4,"offset":0})json");

    const forum::MessagePage page = forum::readMessagePage(*answer);

    ASSERT_EQ(page.items.size(), 1u);
    EXPECT_EQ(page.items[0].info.parentId, 9133148);
    EXPECT_EQ(page.items[0].body, L"VD>банилка\r\n\r\nЯ одного вычислил");
}

TEST(forum, a_message_brings_the_markup_of_its_author) {
    const parsed answer(R"json({"id":9133148,"forumID":1,"topicID":9133148,
        "author":{"id":73,"displayName":"VladD2","role":"Admin"},
        "subject":"Тема","createdOn":"2026-08-29T16:11:36.023+03:00",
        "isTopic":true,"parentID":0,"answersCount":3,
        "body":{"isFormatted":false,"text":"Первая строка.\r\n\r\n[q]цитата[/q]"}})json");

    const forum::Message message = forum::readMessage(*answer);

    EXPECT_EQ(message.info.id, 9133148);
    EXPECT_FALSE(message.isFormatted);
    EXPECT_EQ(message.body, L"Первая строка.\r\n\r\n[q]цитата[/q]");
}

TEST(forum, a_message_without_a_body_is_a_message_without_a_body) {
    // Тот же ответ без `body` -- так отвечает список. Разбор не должен от
    // этого отказываться: поля обмена появляются и исчезают.
    const parsed answer(R"json({"id":7,"forumID":1,"topicID":7,"subject":"Без тела",
        "createdOn":"2026-01-02T03:04:05Z","isTopic":true,"parentID":0})json");

    const forum::Message message = forum::readMessage(*answer);

    EXPECT_EQ(message.info.id, 7);
    EXPECT_TRUE(message.body.empty());
    EXPECT_FALSE(message.isFormatted);
    EXPECT_EQ(message.info.author.id, 0);
    EXPECT_TRUE(message.info.author.displayName.empty());
}

TEST(forum, the_server_introduces_itself) {
    const parsed answer(
        R"json({"name":"RSDN Api Service","serverTime":"2026-09-06T09:43:30.6053529+00:00",
            "serverVersion":"2.0.1","serverBuildDate":"2026-04-19T14:03:23.6597752+00:00"})json");

    const forum::ServiceInfo info = forum::readServiceInfo(*answer);

    EXPECT_EQ(info.name, L"RSDN Api Service");
    EXPECT_EQ(info.serverVersion, L"2.0.1");
    EXPECT_EQ(info.serverTime, utc(2026, 9, 6, 9, 43, 30, 605));
}

TEST(forum, an_account_is_read_whole) {
    const parsed answer(R"json({"id":42,"login":"кто-то","email":"a@b.c",
        "displayName":"Кто-то","gravatarHash":"deadbeef","role":"User"})json");

    const forum::Account account = forum::readAccount(*answer);

    EXPECT_EQ(account.id, 42);
    EXPECT_EQ(account.login, L"кто-то");
    EXPECT_EQ(account.displayName, L"Кто-то");
    EXPECT_EQ(account.role, L"User");
}

TEST(forum, addresses_are_built_the_way_the_server_expects_them) {
    const forum::Api api;

    EXPECT_EQ(api.url(L"forums", {}), L"https://api.rsdn.org/forums");

    EXPECT_EQ(api.url(L"messages", {{L"forumID", L"1"}, {L"limit", L"50"}}),
              L"https://api.rsdn.org/messages?forumID=1&limit=50");

    // Свой сервер -- для того, чтобы однажды поговорить с заглушкой, а не
    // с форумом.
    const forum::Api local(L"http://localhost:8080");

    EXPECT_EQ(local.url(L"service/info", {}), L"http://localhost:8080/service/info");
}

}  // namespace
