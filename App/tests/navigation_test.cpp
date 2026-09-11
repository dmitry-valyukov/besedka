// Переходы без экранов: маршрут, история и план страниц. Всё, что каркас
// делает с деревом, решается здесь -- и здесь же воспроизводится сценарий,
// который ронял приложение: правая страница, переезжающая налево, не
// должна ни уходить из дерева, ни попадать в него вторично.

#include <gtest/gtest.h>

#include <vector>

import besedka.app;
import besedka.forum;

using namespace besedka;
using namespace besedka::app;

namespace {

Route topicsOf(const int forumId) { return TopicsRoute{forum::ForumDescription{.id = forumId}}; }

Route messagesOf(const int topicId) { return MessagesRoute{{}, forum::MessageInfo{.id = topicId}}; }

std::vector<Screen> screensOf(const std::vector<PagePlan::Slot>& shown) {
    std::vector<Screen> screens;

    for (const PagePlan::Slot& slot : shown) screens.push_back(slot.screen);

    return screens;
}

}  // namespace

// ---- маршрут ----

TEST(route, every_route_names_its_screen_and_tab) {
    EXPECT_EQ(screenOf(ForumsRoute{}), Screen::forums);
    EXPECT_EQ(screenOf(topicsOf(3)), Screen::topics);
    EXPECT_EQ(screenOf(messagesOf(7)), Screen::messages);
    EXPECT_EQ(screenOf(WatchedRoute{}), Screen::watched);
    EXPECT_EQ(screenOf(OutboxRoute{}), Screen::outbox);

    // Темы и сообщения -- это глубина вкладки форумов.
    EXPECT_EQ(tabOf(ForumsRoute{}), Tab::forums);
    EXPECT_EQ(tabOf(topicsOf(3)), Tab::forums);
    EXPECT_EQ(tabOf(messagesOf(7)), Tab::forums);
    EXPECT_EQ(tabOf(WatchedRoute{}), Tab::watched);
    EXPECT_EQ(tabOf(OutboxRoute{}), Tab::outbox);
}

TEST(route, a_tab_opens_its_own_root) {
    EXPECT_EQ(screenOf(rootOf(Tab::forums)), Screen::forums);
    EXPECT_EQ(screenOf(rootOf(Tab::watched)), Screen::watched);
    EXPECT_EQ(screenOf(rootOf(Tab::outbox)), Screen::outbox);
}

TEST(route, same_route_means_the_same_page_not_the_same_kind) {
    EXPECT_TRUE(sameRoute(ForumsRoute{}, ForumsRoute{}));
    EXPECT_TRUE(sameRoute(topicsOf(3), topicsOf(3)));
    EXPECT_FALSE(sameRoute(topicsOf(3), topicsOf(4)));
    EXPECT_TRUE(sameRoute(messagesOf(7), messagesOf(7)));
    EXPECT_FALSE(sameRoute(messagesOf(7), messagesOf(8)));
    EXPECT_FALSE(sameRoute(topicsOf(3), messagesOf(3)));
    EXPECT_FALSE(sameRoute(ForumsRoute{}, WatchedRoute{}));
}

TEST(route, the_title_is_what_the_page_is_about) {
    EXPECT_EQ(titleOf(TopicsRoute{forum::ForumDescription{.name = L"C/C++"}}), L"C/C++");
    EXPECT_EQ(titleOf(MessagesRoute{{}, forum::MessageInfo{.subject = L"Модули"}}), L"Модули");
    EXPECT_EQ(titleOf(WatchedRoute{}), L"Избранное");
    EXPECT_EQ(titleOf(OutboxRoute{}), L"Исходящие");
}

TEST(route, the_path_leads_from_the_forum_to_the_topic) {
    const forum::ForumDescription winapi{.id = 3, .name = L"WinAPI"};

    using Path = std::vector<std::wstring>;

    EXPECT_EQ(pathOf(ForumsRoute{}), Path{L"Форумы"});
    EXPECT_EQ(pathOf(WatchedRoute{}), Path{L"Избранное"});
    EXPECT_EQ(pathOf(TopicsRoute{winapi}), Path{L"WinAPI"});
    EXPECT_EQ(pathOf(MessagesRoute{winapi, forum::MessageInfo{.subject = L"UB в сетевом API"}}),
              (Path{L"WinAPI", L"UB в сетевом API"}));
}

// ---- история ----

TEST(history, starts_empty_with_nowhere_to_go) {
    const History history;

    EXPECT_TRUE(history.empty());
    EXPECT_FALSE(history.canGoBack());
    EXPECT_FALSE(history.canGoForward());
    EXPECT_TRUE(history.tail(2).empty());
}

TEST(history, push_makes_the_new_entry_current) {
    History history;

    history.push(ForumsRoute{});
    history.push(topicsOf(3));

    EXPECT_EQ(history.size(), 2u);
    EXPECT_EQ(history.position(), 1u);
    EXPECT_EQ(screenOf(history.current()), Screen::topics);
    EXPECT_TRUE(history.canGoBack());
    EXPECT_FALSE(history.canGoForward());
}

TEST(history, back_and_forward_walk_the_entries) {
    History history;

    history.push(ForumsRoute{});
    history.push(topicsOf(3));
    history.push(messagesOf(7));

    history.back();
    EXPECT_EQ(screenOf(history.current()), Screen::topics);
    EXPECT_TRUE(history.canGoForward());

    history.back();
    EXPECT_EQ(screenOf(history.current()), Screen::forums);
    EXPECT_FALSE(history.canGoBack());

    history.forward();
    history.forward();
    EXPECT_EQ(screenOf(history.current()), Screen::messages);
    EXPECT_FALSE(history.canGoForward());
}

TEST(history, a_new_push_forgets_the_forward_tail) {
    History history;

    history.push(ForumsRoute{});
    history.push(topicsOf(3));
    history.push(messagesOf(7));

    history.back();
    history.back();
    history.push(WatchedRoute{});

    // Было [витрина, темы 3, сообщения 7], стоим на витрине; после нового
    // перехода -- [витрина, избранное], и вперёд идти некуда.
    EXPECT_EQ(history.size(), 2u);
    EXPECT_EQ(history.position(), 1u);
    EXPECT_EQ(screenOf(history.current()), Screen::watched);
    EXPECT_FALSE(history.canGoForward());
}

TEST(history, the_tail_is_what_two_pages_show) {
    History history;

    history.push(ForumsRoute{});

    // Одна запись -- одна страница, сколько бы ни просили.
    EXPECT_EQ(history.tail(2).size(), 1u);

    history.push(topicsOf(3));
    history.push(messagesOf(7));

    {
        const std::span<const Route> shown = history.tail(2);

        ASSERT_EQ(shown.size(), 2u);
        EXPECT_EQ(screenOf(shown[0]), Screen::topics);
        EXPECT_EQ(screenOf(shown[1]), Screen::messages);
    }

    // Хвост считается от текущей записи, а не от конца: после «назад» видны
    // витрина и темы, а сообщения ждут «вперёд».
    history.back();

    {
        const std::span<const Route> shown = history.tail(2);

        ASSERT_EQ(shown.size(), 2u);
        EXPECT_EQ(screenOf(shown[0]), Screen::forums);
        EXPECT_EQ(screenOf(shown[1]), Screen::topics);
    }

    EXPECT_EQ(history.tail(1).size(), 1u);
    EXPECT_EQ(screenOf(history.tail(1)[0]), Screen::topics);
}

TEST(history, choosing_on_the_left_page_is_a_step_back_and_a_new_step) {
    // Слева стоит витрина, справа темы форума 3. Выбор другого форума на
    // витрине -- это не переход из тем, а переход из витрины: сначала назад
    // к ней, потом новый шаг. Иначе рядом легли бы две записи тем.
    History history;

    history.push(ForumsRoute{});
    history.push(topicsOf(3));

    history.back();
    history.push(topicsOf(4));

    EXPECT_EQ(history.size(), 2u);
    EXPECT_EQ(screenOf(history.at(0)), Screen::forums);
    EXPECT_EQ(screenOf(history.at(1)), Screen::topics);
    EXPECT_TRUE(sameRoute(history.current(), topicsOf(4)));
}

// ---- показ ----

TEST(pages, the_threshold_is_in_logical_pixels_and_exclusive) {
    EXPECT_FALSE(isWide(1200));
    EXPECT_TRUE(isWide(1201));
    EXPECT_EQ(pagesShown(800), 1u);
    EXPECT_EQ(pagesShown(1600), 2u);
}

TEST(pages, the_first_page_enters_an_empty_tree) {
    const std::vector<Screen> inTree;
    const std::vector<Route> wanted{ForumsRoute{}};

    const PagePlan plan = planPages(inTree, wanted);

    EXPECT_TRUE(plan.leaving.empty());
    EXPECT_EQ(plan.entering, std::vector<Screen>{Screen::forums});
    ASSERT_EQ(plan.shown.size(), 1u);
    EXPECT_EQ(plan.shown[0].screen, Screen::forums);
    EXPECT_FALSE(plan.shown[0].secondary);
}

TEST(pages, opening_deeper_moves_the_right_page_left_without_touching_it) {
    // Тот самый сценарий: [витрина | темы], двойной щелчок по теме. Витрина
    // уходит, сообщения приходят, а темы остаются в дереве и меняют только
    // сторону -- по ним в этот момент щёлкают.
    const std::vector<Screen> inTree{Screen::forums, Screen::topics};
    const std::vector<Route> wanted{topicsOf(3), messagesOf(7)};

    const PagePlan plan = planPages(inTree, wanted);

    EXPECT_EQ(plan.leaving, std::vector<Screen>{Screen::forums});
    EXPECT_EQ(plan.entering, std::vector<Screen>{Screen::messages});

    ASSERT_EQ(plan.shown.size(), 2u);
    EXPECT_EQ(plan.shown[0].screen, Screen::topics);
    EXPECT_FALSE(plan.shown[0].secondary);
    EXPECT_EQ(plan.shown[1].screen, Screen::messages);
    EXPECT_TRUE(plan.shown[1].secondary);
}

TEST(pages, going_back_brings_the_page_before_and_drops_the_last) {
    const std::vector<Screen> inTree{Screen::topics, Screen::messages};
    const std::vector<Route> wanted{ForumsRoute{}, topicsOf(3)};

    const PagePlan plan = planPages(inTree, wanted);

    EXPECT_EQ(plan.leaving, std::vector<Screen>{Screen::messages});
    EXPECT_EQ(plan.entering, std::vector<Screen>{Screen::forums});
    EXPECT_EQ(screensOf(plan.shown), (std::vector<Screen>{Screen::forums, Screen::topics}));
    EXPECT_FALSE(plan.shown[0].secondary);
    EXPECT_TRUE(plan.shown[1].secondary);
}

TEST(pages, choosing_on_the_left_changes_nothing_in_the_tree) {
    // [витрина | темы 3], одиночный щелчок по форуму 4 на витрине: в дереве те
    // же два экрана, темы лишь наполняются другим форумом.
    const std::vector<Screen> inTree{Screen::forums, Screen::topics};
    const std::vector<Route> wanted{ForumsRoute{}, topicsOf(4)};

    const PagePlan plan = planPages(inTree, wanted);

    EXPECT_TRUE(plan.leaving.empty());
    EXPECT_TRUE(plan.entering.empty());
    EXPECT_EQ(screensOf(plan.shown), (std::vector<Screen>{Screen::forums, Screen::topics}));
}

TEST(pages, a_narrow_window_swaps_the_only_page) {
    const std::vector<Screen> inTree{Screen::topics};
    const std::vector<Route> wanted{messagesOf(7)};

    const PagePlan plan = planPages(inTree, wanted);

    EXPECT_EQ(plan.leaving, std::vector<Screen>{Screen::topics});
    EXPECT_EQ(plan.entering, std::vector<Screen>{Screen::messages});
    ASSERT_EQ(plan.shown.size(), 1u);
    EXPECT_FALSE(plan.shown[0].secondary);
}

TEST(pages, widening_the_window_adds_the_page_before) {
    // Стопка та же; ширины стало хватать на две -- слева встаёт предыдущая.
    const std::vector<Screen> inTree{Screen::topics};
    const std::vector<Route> wanted{ForumsRoute{}, topicsOf(3)};

    const PagePlan plan = planPages(inTree, wanted);

    EXPECT_TRUE(plan.leaving.empty());
    EXPECT_EQ(plan.entering, std::vector<Screen>{Screen::forums});
    EXPECT_EQ(screensOf(plan.shown), (std::vector<Screen>{Screen::forums, Screen::topics}));
}

TEST(pages, narrowing_the_window_keeps_only_the_current_page) {
    const std::vector<Screen> inTree{Screen::forums, Screen::topics};
    const std::vector<Route> wanted{topicsOf(3)};

    const PagePlan plan = planPages(inTree, wanted);

    EXPECT_EQ(plan.leaving, std::vector<Screen>{Screen::forums});
    EXPECT_TRUE(plan.entering.empty());
    ASSERT_EQ(plan.shown.size(), 1u);
    EXPECT_EQ(plan.shown[0].screen, Screen::topics);
    EXPECT_FALSE(plan.shown[0].secondary);
}

TEST(pages, the_split_stays_within_its_limits) {
    EXPECT_DOUBLE_EQ(clampedSplit(0.5), 0.5);
    EXPECT_DOUBLE_EQ(clampedSplit(0.0), kSplitLower);
    EXPECT_DOUBLE_EQ(clampedSplit(1.0), kSplitUpper);
    EXPECT_DOUBLE_EQ(clampedSplit(kSplitUpper), kSplitUpper);
}

// ---- масштаб ----

TEST(zoom, steps_go_up_and_down_the_ladder_and_come_back_to_one) {
    EXPECT_DOUBLE_EQ(zoomedIn(1.0), 1.1);
    EXPECT_DOUBLE_EQ(zoomedIn(1.1), 1.25);
    EXPECT_DOUBLE_EQ(zoomedOut(1.0), 0.9);
    EXPECT_DOUBLE_EQ(zoomedOut(zoomedIn(1.0)), 1.0);
    EXPECT_DOUBLE_EQ(zoomedIn(zoomedOut(1.0)), 1.0);
}

TEST(zoom, the_ends_of_the_ladder_hold) {
    EXPECT_DOUBLE_EQ(zoomedIn(3.0), 3.0);
    EXPECT_DOUBLE_EQ(zoomedOut(0.5), 0.5);
    EXPECT_DOUBLE_EQ(clampedZoom(10.0), 3.0);
    EXPECT_DOUBLE_EQ(clampedZoom(0.1), 0.5);
    EXPECT_DOUBLE_EQ(clampedZoom(1.5), 1.5);
}

TEST(zoom, a_value_between_steps_goes_to_the_nearest_step_in_that_direction) {
    // После щипка масштаб может быть любым; клавиша ведёт на ступень.
    EXPECT_DOUBLE_EQ(zoomedIn(1.3), 1.5);
    EXPECT_DOUBLE_EQ(zoomedOut(1.3), 1.25);
}

TEST(zoom, a_float_from_the_scroll_viewer_is_not_a_step_above_itself) {
    // 1,1f -- это 1,10000002 в double; следующая ступень -- 1,25, а не 1,1.
    EXPECT_DOUBLE_EQ(zoomedIn(static_cast<double>(1.1f)), 1.25);
    EXPECT_DOUBLE_EQ(zoomedOut(static_cast<double>(0.9f)), 0.8);
}
