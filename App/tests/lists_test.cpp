// Что показывают списки: витрина по группам и глубины ответов.

#include <gtest/gtest.h>

#include <vector>

import besedka.app;
import besedka.forum;

using namespace besedka;
using namespace besedka::app;

namespace {

forum::ForumDescription forumIn(const int id, const int groupId, const int sortOrder) {
    forum::ForumDescription forum;

    forum.id = id;
    forum.group = forum::ForumGroup{.id = groupId, .sortOrder = sortOrder};

    return forum;
}

forum::Message reply(const int id, const int parentId) {
    forum::Message message;

    message.info.id = id;
    message.info.parentId = parentId;

    return message;
}

}  // namespace

TEST(lists, groups_follow_the_servers_sort_order_and_forums_their_arrival) {
    // Сервер отдаёт форумы вперемешку по группам; группа 10 стоит впереди
    // группы 3 по sortOrder, хотя встретилась позже.
    const std::vector<forum::ForumDescription> forums{
        forumIn(1, 3, 500),
        forumIn(2, 10, 120),
        forumIn(3, 3, 500),
        forumIn(4, 10, 120),
    };

    const std::vector<ShowcaseGroup> groups = groupShowcase(forums);

    ASSERT_EQ(groups.size(), 2u);
    EXPECT_EQ(groups[0].group.id, 10);
    EXPECT_EQ(groups[0].forums, (std::vector<std::size_t>{1, 3}));
    EXPECT_EQ(groups[1].group.id, 3);
    EXPECT_EQ(groups[1].forums, (std::vector<std::size_t>{0, 2}));
}

TEST(lists, groups_with_the_same_order_keep_their_arrival_order) {
    const std::vector<forum::ForumDescription> forums{
        forumIn(1, 7, 100),
        forumIn(2, 8, 100),
    };

    const std::vector<ShowcaseGroup> groups = groupShowcase(forums);

    ASSERT_EQ(groups.size(), 2u);
    EXPECT_EQ(groups[0].group.id, 7);
    EXPECT_EQ(groups[1].group.id, 8);
}

TEST(lists, reply_depth_is_the_length_of_the_parent_chain_within_the_page) {
    forum::MessagePage page;

    page.items = {
        reply(1, 0),   // корень темы
        reply(2, 1),   // ответ корню
        reply(3, 2),   // ответ ответу
        reply(4, 1),   // ещё один ответ корню
        reply(5, 99),  // родитель не на странице -- как корень
    };

    EXPECT_EQ(replyDepths(page), (std::vector<int>{0, 1, 2, 1, 0}));
}

TEST(lists, an_empty_page_has_no_depths) { EXPECT_TRUE(replyDepths(forum::MessagePage{}).empty()); }
