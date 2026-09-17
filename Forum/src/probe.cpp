// Проба дороги к api.rsdn.org: четыре запроса подряд -- сервер, витрина
// форумов, темы одного из них и тело первой темы.
//
// Проверяется здесь не вывод, а то, что вся дорога работает на живом
// сервере: HTTPS через Schannel внутри Windows.Web.Http (своей библиотеки
// TLS не нужно), чтение открыто без входа, ответ разбирает наш wxl::json, и
// модель получается та, которую ждёт интерфейс.
//
// Это средство проверки дороги, а не приложение и не его заготовка:
// приложение -- в `App/`. Здесь важно, что видно в консоли за одну секунду,
// когда сервер отвечает не то, что вчера.
//
// Апартамент STA с очередью сообщений заведён потому же, почему он у
// приложения: разбор ответа выделяет память из пула STA, а пул привязан к
// своему потоку -- значит, ответ обязан вернуться на тот же поток, с
// которого ушёл запрос.
//
// Запуск: build\x64\Forum\Debug\forum-probe.exe

#include <windows.h>

#include <cstdio>

#include <winrt/Windows.Foundation.h>

import std;
import besedka.forum;
import wxl.async;
import wxl.core;

namespace {

using namespace besedka;

std::string utf8(std::wstring_view text) {
    return std::string(wxl::core::unicode::repaired(text).to_utf8().chars());
}

/// Колонка шириной в символах, а не в байтах: printf считает байты, и от
/// кириллицы, у которой их по два на букву, таблица разъезжается.
std::string padded(std::wstring_view text, std::size_t width) {
    std::string bytes = utf8(text);

    const std::optional<wxl::core::u8_view> checked =
        wxl::core::unicode::checked(std::string_view(bytes));
    const std::size_t points =
        checked ? wxl::core::unicode::code_point_count(*checked) : bytes.size();

    if (points < width) bytes.append(width - points, ' ');

    return bytes;
}

/// Время по Гринвичу: модель держит его в UTC, а часовые пояса -- забота
/// интерфейса, которого тут ещё нет.
std::string when(std::chrono::system_clock::time_point moment) {
    return std::format("{:%d.%m.%Y %H:%M}", std::chrono::floor<std::chrono::seconds>(moment));
}

/// Первые строки текста -- столько, сколько влезает в консоль, не заслоняя
/// собой остальное.
std::wstring beginning(std::wstring_view text, std::size_t limit) {
    const std::size_t stop = std::min(limit, text.size());

    std::wstring shown(text.substr(0, stop));

    for (wchar_t& letter : shown)
        if (letter == L'\r' || letter == L'\n') letter = L' ';

    if (stop < text.size()) shown += L"…";

    return shown;
}

/// Шаги пробы, один за другим. Каждый начинается в продолжении
/// предыдущего, то есть на том же потоке STA.
class Probe {
public:
    void start() { askServiceInfo(); }

    int result() const noexcept { return failed_ ? 1 : 0; }

private:
    void askServiceInfo() {
        std::print("— сервер —\n");

        api_.serviceInfo()
            .when_succeeded([this](const forum::ServiceInfo& info) noexcept {
                std::print("{}, версия {}, собран {}\n\n", utf8(info.name),
                           utf8(info.serverVersion), when(info.serverBuildDate));
                askForums();
            })
            .when_failed([this](const std::exception_ptr& why) noexcept { give_up(why); });
    }

    void askForums() {
        std::print("— витрина —\n");

        api_.forums()
            .when_succeeded([this](const std::vector<forum::ForumDescription>& forums) noexcept {
                std::print("форумов: {}\n", forums.size());

                for (const forum::ForumDescription& forum : forums)
                    if (forum.isInTop)
                        std::print("{:5}  {} {} {}\n", forum.id, padded(forum.code, 14),
                                   padded(forum.name, 34), utf8(forum.group.name));

                std::print("\n");
                askTopics(forums.empty() ? 1 : forums.front().id);
            })
            .when_failed([this](const std::exception_ptr& why) noexcept { give_up(why); });
    }

    void askTopics(int forumId) {
        std::print("— темы форума {} —\n", forumId);

        api_.topics(forumId, 5)
            .when_succeeded([this](const forum::MessagePage& page) noexcept {
                std::print("всего тем: {}\n", page.total);

                for (const forum::Message& topic : page.items)
                    std::print("{:9}  {} {}  ответов: {}\n", topic.info.id,
                               when(topic.info.createdOn), padded(topic.info.subject, 46),
                               topic.info.answersCount);

                std::print("\n");

                if (page.items.empty()) {
                    done();
                    return;
                }

                askMessage(page.items.front().info.id);
            })
            .when_failed([this](const std::exception_ptr& why) noexcept { give_up(why); });
    }

    void askMessage(int id) {
        std::print("— сообщение {} —\n", id);

        api_.message(id)
            .when_succeeded([this](const forum::Message& message) noexcept {
                std::print("{}, {}\n{}\n{}\n\n", utf8(message.info.subject),
                           utf8(message.info.author.displayName),
                           message.isFormatted ? "тело: серверный HTML" : "тело: разметка автора",
                           utf8(beginning(message.body, 400)));
                done();
            })
            .when_failed([this](const std::exception_ptr& why) noexcept { give_up(why); });
    }

    void give_up(const std::exception_ptr& why) noexcept {
        failed_ = true;

        try {
            std::rethrow_exception(why);
        } catch (const forum::HttpError& refused) {
            std::print("не вышло: {} (код {})\n", refused.said().to_utf8().chars(),
                       refused.status());
        } catch (const std::exception& broken) {
            std::print("не вышло: {}\n", broken.what());
        }

        done();
    }

    void done() noexcept { ::PostQuitMessage(0); }

    forum::Api api_;
    bool failed_ = false;
};

}  // namespace

int main() {
    ::SetConsoleOutputCP(CP_UTF8);

    // STA, а не MTA: ответ должен вернуться на этот же поток, а вернуть его
    // сюда C++/WinRT может только через очередь сообщений апартамента.
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    Probe probe;

    probe.start();

    MSG message;

    while (::GetMessageW(&message, nullptr, 0, 0)) {
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }

    return probe.result();
}
