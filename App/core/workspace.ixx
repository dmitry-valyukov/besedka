// Рабочее место: настройки и файл, где они лежат, -- и запуск, который их
// читает.
//
// Окна здесь нет: приложение держит рабочее место полем и зовёт его запуск, а
// что при этом не удалось, получает словами (`Started::notices`) и показывает
// само. Файл читается и пишется операциями wxl, а не в потоке вызова: у
// приложения это поток интерфейса, и диск в нём не трогают.

export module besedka.app:workspace;

import std;
import wxl.async;

import :notice;
import :settings;

export namespace besedka::app {

/// Чем кончился запуск: настройки уже в полях рабочего места, а здесь -- что
/// при этом пошло не так.
struct Started {
    std::vector<Notice> notices;
};

/// Владеет настройками; каталог данных -- путь, который ему дали.
class Workspace {
public:
    /// @param directory каталог данных: у приложения -- `dataDirectory()`, у
    ///        тестов -- свой временный.
    explicit Workspace(std::filesystem::path directory);

    Workspace(const Workspace&) = delete;
    Workspace& operator=(const Workspace&) = delete;

    // ---- где что лежит ----

    std::filesystem::path settingsPath() const;   ///< settings.xml

    // ---- модели ----

    Settings settings;   ///< то, чем Беседка открывается

    // ---- запуск ----

    /// Читает настройки. Нет файла -- первый запуск, молча; файл не читается
    /// или не разбирается -- слово читателю в `notices`; испорченный остаётся
    /// рядом копией `.bad`, а первая же запись заменит его.
    wxl::async::task<Started> start();

    // ---- запись ----

    /// Текст настроек собирается в момент вызова, до ожидания: на диск уходит
    /// решённое, а не то, что читатель поменяет за время записи. Зовёт её один
    /// писатель на файл: у Беседки её цикл записи.
    wxl::async::task<> saveSettings();

    // ---- общее ----

    /// Испорченный файл -- копией рядом, под тем же именем с «.bad»: байты те,
    /// что прочли и не разобрали; копия одна, прежняя перезаписывается --
    /// истории порч хранить незачем.
    wxl::async::task<> keepBrokenCopy(std::filesystem::path file, std::string bytes);

private:
    std::filesystem::path directory_;
};

}  // namespace besedka::app
