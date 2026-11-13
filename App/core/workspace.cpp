module;

#include <windows.h>

module besedka.app;

import std;
import wxl.core;

namespace besedka::app {

using wxl::async::async_file;
using wxl::async::last_error;
using wxl::async::task;

namespace {

/// Путь Беседки, переведённый в путь wxl. Строится в потоке вызова -- там, где
/// STA-пул, -- и уезжает в операцию копией.
wxl::core::path poolPath(const std::filesystem::path& system) {
    return wxl::core::path(std::wstring_view(system.native()));
}

/// Файла нет -- обычное дело для настроек при первом запуске; всё остальное --
/// настоящая ошибка, у которой есть имя.
bool absent(const last_error& failure) {
    return failure.code().value() == ERROR_FILE_NOT_FOUND || failure.code().value() == ERROR_PATH_NOT_FOUND;
}

/// Копия испорченного файла -- рядом, под тем же именем с «.bad».
std::filesystem::path brokenCopyOf(std::filesystem::path file) {
    file += L".bad";
    return file;
}

}  // namespace

Workspace::Workspace(std::filesystem::path directory) : directory_(std::move(directory)) {}

// ---- где что лежит --------------------------------------------------------

std::filesystem::path Workspace::settingsPath() const {
    return directory_ / L"settings.xml";
}

// ---- запуск ---------------------------------------------------------------

task<Started> Workspace::start() {
    Started started;

    // Нет файла -- первый запуск, молча; не читается -- слово читателю, а
    // запуск идёт дальше с умолчаниями; прочитан, но не разобран -- слово и
    // копия рядом. Без токена: из этих настроек потом пишется всё, и
    // недочитанные легли бы на диск умолчаниями.
    const auto read = [&](const std::filesystem::path& file, std::wstring whatFailed) -> task<std::string> {
        try {
            co_return co_await async_file::read_all(poolPath(file));
        } catch (const last_error& failure) {
            if (!absent(failure))
                started.notices.push_back({std::move(whatFailed), file.wstring() + L"\n\n" + reasonOf(failure)});
            co_return std::string{};
        }
    };

    std::string settingsXmlText = co_await read(settingsPath(), L"Не удалось прочитать настройки");

    // Сам файл заменит первая же запись настроек: окно встаёт на место по
    // умолчанию, когда откроется витрина, и это место пишется само, -- так
    // что сообщение читатель увидит над файлом, который вот-вот перепишут, о
    // чём оно и говорит. Байты у копии свои, в памяти, так что порядок двух
    // записей безразличен.
    if (!readSettings(settingsXmlText, settings)) {
        started.notices.push_back({L"Настройки не прочитаны",
                                   settingsPath().wstring() +
                                       L"\n\nФайл испорчен: взяты умолчания, и они же лягут в него с "
                                       L"первой записью настроек -- место окна пишется само, когда "
                                       L"откроется витрина. Испорченный остаётся рядом копией "
                                       L"(settings.xml.bad)."});
        co_await keepBrokenCopy(settingsPath(), std::move(settingsXmlText));
    }

    co_return started;
}

// ---- запись ---------------------------------------------------------------

task<> Workspace::saveSettings() {
    std::string xml = formatSettings(settings);

    co_await async_file::write_all(poolPath(settingsPath()), std::move(xml));
}

// ---- общее ----------------------------------------------------------------

task<> Workspace::keepBrokenCopy(std::filesystem::path file, std::string bytes) {
    co_await async_file::write_all(poolPath(brokenCopyOf(std::move(file))), std::move(bytes));
}

}  // namespace besedka::app
