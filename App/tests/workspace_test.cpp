// Рабочее место: запуск читает настройки, о битом файле говорит словами и
// оставляет его рядом копией.
//
// Корутины рабочего места ждут операций wxl, а те живут на петле sta_loop:
// тест поднимает её один раз на процесс (как тесты самой wxl) и крутит до
// конца каждой корутины. Разбор идёт на этом же потоке, в STA-пуле.

#include <gtest/gtest.h>

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>

import besedka.app;
import wxl.async;

using namespace besedka::app;
using wxl::async::sta_loop;
using wxl::async::task;

namespace {

/// Петля операций wxl -- одна на процесс: её каналы живут столько же, сколько
/// процесс, и второй раз не стартуют. Потому глобальным окружением, а не в
/// каждом тесте.
class StaLoopEnvironment : public ::testing::Environment {
public:
    void SetUp() override { sta_loop::start("besedka.app tests: I/O"); }
    void TearDown() override { sta_loop::stop(); }
};

/// Регистрируется до main(), где регистрируют глобальное окружение.
::testing::Environment* const staLoop = ::testing::AddGlobalTestEnvironment(new StaLoopEnvironment);

/// Временный каталог на один тест: рабочее место живёт в нём, после теста он
/// стирается. Имя -- с номером процесса: два прогона рядом не пересекутся.
struct Sandbox {
    std::filesystem::path root;

    explicit Sandbox(const char* name)
        : root(std::filesystem::temp_directory_path() /
               (L"besedka-app-tests-" + std::to_wstring(::GetCurrentProcessId())) / name) {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
        std::filesystem::create_directories(root);
    }

    ~Sandbox() {
        std::error_code ignored;
        std::filesystem::remove_all(root.parent_path(), ignored);
    }
};

template <class T>
T run(task<T> work) {
    sta_loop::run_until([&] { return work.done(); });
    return work.result();
}

void run(task<> work) {
    sta_loop::run_until([&] { return work.done(); });
    work.result();
}

/// Байты файла, прочитанные обычным способом: проверка того, что записало
/// рабочее место.
std::string onDisk(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void putOnDisk(const std::filesystem::path& file, std::string_view bytes) {
    std::filesystem::create_directories(file.parent_path());
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

bool holds(const std::wstring& text, std::wstring_view piece) {
    return text.find(piece) != std::wstring::npos;
}

}  // namespace

/// Пустой каталог -- первый запуск: умолчания, ни одного слова читателю.
TEST(workspace, first_start_reads_defaults_says_nothing_writes_nothing) {
    Sandbox box("empty");
    Workspace ws(box.root);

    const Started started = run(ws.start());

    EXPECT_TRUE(started.notices.empty());
    EXPECT_TRUE(ws.settings.windowPlacement.empty());
    EXPECT_DOUBLE_EQ(ws.settings.splitFraction, 0.5);
    EXPECT_FALSE(std::filesystem::exists(ws.settingsPath()));
}

/// Прочитанный файл ложится в настройки рабочего места, и слов о нём нет.
TEST(workspace, a_whole_file_is_read_into_the_settings) {
    Sandbox box("whole");
    Workspace ws(box.root);

    Settings written;
    written.windowPlacement = L"2,3,-1,-1,-1,-1,100,100,1380,960";
    written.splitFraction = 0.375;
    putOnDisk(ws.settingsPath(), formatSettings(written));

    const Started started = run(ws.start());

    EXPECT_TRUE(started.notices.empty());
    EXPECT_EQ(ws.settings.windowPlacement, written.windowPlacement);
    EXPECT_DOUBLE_EQ(ws.settings.splitFraction, written.splitFraction);
    EXPECT_FALSE(std::filesystem::exists(box.root / L"settings.xml.bad"));
}

/// Испорченный файл: одно слово с путём, копия .bad байт в байт, умолчания;
/// первая запись заменяет файл, копия остаётся одна и перезаписывается.
TEST(workspace, a_broken_settings_file_is_named_once_and_kept_beside) {
    Sandbox box("broken");

    const std::string brokenSettings = "<settings version=\"1\"><window placement=\"";

    {
        Workspace ws(box.root);
        putOnDisk(ws.settingsPath(), brokenSettings);

        const Started started = run(ws.start());

        ASSERT_EQ(started.notices.size(), 1u);
        EXPECT_EQ(started.notices[0].headline, L"Настройки не прочитаны");
        EXPECT_TRUE(holds(started.notices[0].details, ws.settingsPath().wstring()))
            << "сообщение о настройках называет файл";
        EXPECT_TRUE(ws.settings.windowPlacement.empty()) << "битые настройки -- умолчания";
        EXPECT_DOUBLE_EQ(ws.settings.splitFraction, 0.5);

        EXPECT_EQ(onDisk(box.root / L"settings.xml.bad"), brokenSettings) << "копия settings.xml.bad байт в байт";

        run(ws.saveSettings());

        Settings fresh;
        EXPECT_TRUE(readSettings(onDisk(ws.settingsPath()), fresh)) << "первая запись заменила файл целым";
        EXPECT_EQ(onDisk(box.root / L"settings.xml.bad"), brokenSettings) << "копия после замены на месте";
    }

    const std::string brokenAgain = "<settings version=\"1\"><layout split=\"0.";
    {
        Workspace ws(box.root);
        putOnDisk(ws.settingsPath(), brokenAgain);

        static_cast<void>(run(ws.start()));

        EXPECT_EQ(onDisk(box.root / L"settings.xml.bad"), brokenAgain) << "повторная порча -- копия перезаписана";
        EXPECT_FALSE(std::filesystem::exists(box.root / L"settings.xml.bad.bad")) << "копия одна, не множится";
    }
}
