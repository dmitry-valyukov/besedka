module besedka.app;

import std;
import besedka.forum;

namespace besedka::app {

namespace {

/// Набор лямбд как один вызываемый объект -- обычный приём для std::visit.
template <class... F>
struct overloaded : F... {
    using F::operator()...;
};

}  // namespace

Screen screenOf(const Route& route) {
    return std::visit(overloaded{
                          [](const ForumsRoute&) { return Screen::forums; },
                          [](const WatchedRoute&) { return Screen::watched; },
                          [](const OutboxRoute&) { return Screen::outbox; },
                          [](const TopicsRoute&) { return Screen::topics; },
                          [](const MessagesRoute&) { return Screen::messages; },
                      },
                      route);
}

Tab tabOf(const Route& route) {
    switch (screenOf(route)) {
        case Screen::watched: return Tab::watched;
        case Screen::outbox: return Tab::outbox;
        case Screen::forums:
        case Screen::topics:
        case Screen::messages: break;
    }

    return Tab::forums;
}

Route rootOf(const Tab tab) {
    switch (tab) {
        case Tab::watched: return WatchedRoute{};
        case Tab::outbox: return OutboxRoute{};
        case Tab::forums: break;
    }

    return ForumsRoute{};
}

bool sameRoute(const Route& left, const Route& right) {
    if (left.index() != right.index()) return false;

    return std::visit(overloaded{
                          [&right](const TopicsRoute& mine) {
                              return mine.forum.id == std::get<TopicsRoute>(right).forum.id;
                          },
                          [&right](const MessagesRoute& mine) {
                              return mine.topic.id == std::get<MessagesRoute>(right).topic.id;
                          },
                          // У витрины и заглушек нет параметров: один род --
                          // одна и та же страница.
                          [](const auto&) { return true; },
                      },
                      left);
}

std::wstring titleOf(const Route& route) {
    return std::visit(overloaded{
                          [](const ForumsRoute&) { return std::wstring(L"Форумы"); },
                          [](const WatchedRoute&) { return std::wstring(L"Избранное"); },
                          [](const OutboxRoute&) { return std::wstring(L"Исходящие"); },
                          [](const TopicsRoute& topics) { return topics.forum.name; },
                          [](const MessagesRoute& messages) { return messages.topic.subject; },
                      },
                      route);
}

}  // namespace besedka::app
