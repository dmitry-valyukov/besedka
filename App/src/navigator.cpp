#include "navigator.h"

#include <chrono>
#include <format>
#include <span>
#include <utility>

#include "stub_screens.h"

namespace besedka::app {

using namespace wxl;

namespace {

/// Сколько тем и сообщений просить за раз. Постраничности пока нет (см.
/// CLAUDE.md, «Что сделать дальше»), берётся первая порция.
constexpr int kTopicsPerPage = 50;
constexpr int kMessagesPerTopic = 200;

template <class... F>
struct overloaded : F... {
    using F::operator()...;
};

}  // namespace

Navigator::Navigator(forum::Api& api, Shell& shell, const std::filesystem::path& assets)
    : api_(api), shell_(shell), watched_(watchedScreen()), outbox_(outboxScreen()) {
    messages_.setBaseDirectory(assets.wstring());

    host_.setScreen(Screen::forums, forums_.root());
    host_.setScreen(Screen::topics, topics_.root());
    host_.setScreen(Screen::messages, messages_.root());
    host_.setScreen(Screen::watched, watched_);
    host_.setScreen(Screen::outbox, outbox_);

    host_.onSplitChanged = [this](const double fraction) {
        if (onSplitChanged) onSplitChanged(fraction);
    };

    splash_.onRetry = [this] { loadShowcase(); };

    forums_.onOpen = [this](const forum::ForumDescription& forum) {
        open(Screen::forums, TopicsRoute{forum});
    };

    topics_.onOpen = [this](const forum::MessageInfo& topic) {
        open(Screen::topics, MessagesRoute{topic});
    };

    shell_.onBack = [this] { back(); };
    shell_.onForward = [this] { forward(); };
    shell_.onRefresh = [this] { reload(); };
    shell_.onTab = [this](const Tab tab) { navigate(rootOf(tab)); };
}

void Navigator::start() { loadShowcase(); }

void Navigator::navigate(Route route) {
    // Та же страница -- не переход: щелчок по вкладке, на которой стоим.
    if (!history_.empty() && sameRoute(history_.current(), route)) return;

    history_.push(std::move(route));

    moved();
}

void Navigator::open(const Screen from, Route to) {
    const std::span<const Route> shown = history_.tail(pagesShown(width_));

    if (shown.size() == 2 && screenOf(shown[0]) == from) history_.back();

    navigate(std::move(to));
}

void Navigator::back() {
    if (!history_.canGoBack()) return;

    history_.back();

    moved();
}

void Navigator::forward() {
    if (!history_.canGoForward()) return;

    history_.forward();

    moved();
}

void Navigator::reload() {
    if (history_.empty()) {
        loadShowcase();
        return;
    }

    load(history_.current(), true);
}

void Navigator::setWidth(const double logical) {
    if (logical == width_) return;

    const bool was = isWide(width_);

    width_ = logical;

    // Перекладка только на смене способа показа: тянущий рамку окна шлёт
    // новый размер на каждый пиксель.
    if (!history_.empty() && isWide(width_) != was) show();
}

void Navigator::splitFraction(const double value) { host_.splitFraction(value); }

void Navigator::moved() {
    show();

    const Route& current = history_.current();

    shell_.setCanGoBack(history_.canGoBack());
    shell_.setCanGoForward(history_.canGoForward());
    shell_.selectTab(tabOf(current));
    shell_.setStatusText(titleOf(current));
}

void Navigator::show() {
    const std::span<const Route> shown = history_.tail(pagesShown(width_));

    // Содержимое -- до дерева: экран, который ещё не в дереве, наполняется
    // дешевле.
    for (const Route& route : shown) load(route, false);

    const PagePlan plan = planPages(host_.inTree(), shown);

    host_.setWide(isWide(width_));
    host_.apply(plan);

    // Где страница встала, ей говорят всегда: слева выбирают одиночным
    // щелчком, справа двойным, и сторона у одной и той же страницы меняется,
    // даже когда сама она с места не двинулась.
    for (const PagePlan::Slot& slot : plan.shown) {
        switch (slot.screen) {
            case Screen::forums: forums_.setSecondary(slot.secondary); break;
            case Screen::topics: topics_.setSecondary(slot.secondary); break;
            case Screen::messages:
            case Screen::watched:
            case Screen::outbox: break;  // в них не выбирают
        }
    }

    shell_.showPages(host_.root());
}

void Navigator::load(const Route& route, const bool again) {
    std::visit(
        overloaded{
            [this, again](const ForumsRoute&) {
                if (again) loadShowcase();
            },

            [this, again](const TopicsRoute& topics) {
                const int forumId = topics.forum.id;

                if (!again && topics_.shows(forumId)) return;

                topics_.setForum(topics.forum);

                // Ответ мог обогнать другой: пока ехали темы одного форума,
                // открыли другой. Приехавшее не для того форума, что на
                // экране, просто выбрасывается.
                api_.topics(forumId, kTopicsPerPage)
                    .when_succeeded([this, forumId](const forum::MessagePage& page) noexcept {
                        if (topics_.shows(forumId)) topics_.show(page);
                    })
                    .when_failed([this, forumId](const std::exception_ptr& why) noexcept {
                        if (topics_.shows(forumId)) topics_.setError(reasonOf(why));

                        shell_.setServerStatus(ServerStatus::offline);
                    });
            },

            [this, again](const MessagesRoute& messages) {
                const int topicId = messages.topic.id;

                if (!again && messages_.shows(topicId)) return;

                messages_.setTopic(messages.topic);

                // Тела приезжают вместе со списком: одна поездка на всю тему.
                api_.answers(topicId, kMessagesPerTopic)
                    .when_succeeded([this, topicId](const forum::MessagePage& page) noexcept {
                        if (messages_.shows(topicId)) messages_.show(page);
                    })
                    .when_failed([this, topicId](const std::exception_ptr& why) noexcept {
                        if (messages_.shows(topicId)) messages_.setError(reasonOf(why));

                        shell_.setServerStatus(ServerStatus::offline);
                    });
            },

            // За заглушками ничего нет -- и читать нечего.
            [](const WatchedRoute&) {},
            [](const OutboxRoute&) {},
        },
        route);
}

void Navigator::loadShowcase() {
    // Пока истории нет, показывать нечего, кроме заставки: первый запуск и
    // «Ещё раз» после отказа ведут сюда же. Когда витрина уже была, она
    // просто перечитывается на месте -- через заставку на «обновить» ходить
    // незачем.
    const bool first = history_.empty();

    if (first) {
        splash_.setStatus(L"Читаю список форумов…");
        shell_.showSplash(splash_.root());

        if (onSplash) onSplash();
    }

    shell_.setBusy(true);
    shell_.setStatusText(L"Соединяюсь с api.rsdn.org…");

    // Запрос уходит с этого потока, и продолжения приходят на него же:
    // C++/WinRT возвращает корутину в апартамент, из которого её начали.
    // Поэтому внутри обработчика можно и трогать XAML, и разбирать ответ.
    api_.forums()
        .when_succeeded([this](const std::vector<forum::ForumDescription>& list) noexcept {
            forums_.show(list);

            shell_.setBusy(false);
            shell_.setServerStatus(ServerStatus::online);

            // Витрина есть -- значит окну пора принять свой рабочий вид, и
            // ровно один раз: раньше содержимого нет, а без содержимого окно
            // рабочего размера -- это пустая рамка.
            if (history_.empty()) {
                if (onShowcase) onShowcase();

                navigate(ForumsRoute{});
            }

            shell_.setStatusText(std::format(L"Форумов на сервере: {}", list.size()));
        })
        .when_failed([this](const std::exception_ptr& why) noexcept {
            shell_.setBusy(false);
            shell_.setServerStatus(ServerStatus::offline);

            if (history_.empty())
                splash_.setError(reasonOf(why));
            else
                shell_.setStatusText(reasonOf(why));
        });
}

void Navigator::showAbout(const UIElement& host) {
    about_.setServerLine({});
    about_.show(host);

    api_.serviceInfo()
        .when_succeeded([this](const forum::ServiceInfo& info) noexcept {
            about_.setServerLine(
                std::format(L"API v{} [{:%d.%m.%Y}]", info.serverVersion,
                            std::chrono::floor<std::chrono::days>(info.serverBuildDate)));
        })
        .when_failed([](const std::exception_ptr&) noexcept {});
}

}  // namespace besedka::app
