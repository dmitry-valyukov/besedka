// Беседка -- клиент форума RSDN на wxl.winui.
//
// Здесь нет ни wWinMain, ни поднятия Windows App Runtime, ни наследника
// Application, ни XAML: всё это делает wxl и потом зовёт эту функцию.
//
// Что здесь есть -- сборка приложения из каркаса и экранов и решения о том,
// что чем сменяется. Устройство то же, что у jana в `ui/MainWindow.kt`:
// каркас с верхней панелью, вкладками и полосой состояния, а в середине --
// экран. Экранов шесть: заставка, витрина форумов, две заглушки за
// соседними вкладками, темы форума и сообщения темы. Показ сообщения свой,
// через wxl::RsdnBlock.
//
// Запросы задаются отсюда, с потока интерфейса, и сюда же C++/WinRT
// возвращает ответ -- на том потоке он и разбирается. Почему иначе нельзя,
// написано в docs/decisions.md.

// Свои заголовки со стандартными внутри -- до всего, что тянет import
// wxl.core: заголовок, включённый после импорта, MSVC уже не принимает.
#include <windows.h>

#include <chrono>
#include <exception>
#include <filesystem>
#include <format>
#include <memory>
#include <string>

#include "WindowBackdrop.h"
#include "WindowPlacement.h"
#include "about_dialog.h"
#include "forum_screen.h"
#include "message_screen.h"
#include "settings.h"
#include "shell.h"
#include "splash_screen.h"
#include "stub_screens.h"
#include "topic_screen.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

using namespace besedka::app;

namespace forum = besedka::forum;

// Каким окно открывается в первый раз -- дальше его ставит запомненное место.
constexpr int32_t kInitialWidth = 1280;
constexpr int32_t kInitialHeight = 860;

// Сколько окно должно постоять смирно, прежде чем его место запишут. Тянуть
// рамку мышью -- это сотни событий в секунду, и запись на каждое из них была бы
// файлом, переписанным сотни раз ради одного числа.
constexpr auto kSaveQuiet = std::chrono::milliseconds(800);

/// Каталог рядом с исполняемым файлом. Путь без схемы XAML разрешает именно
/// оттуда, и ресурсы туда же кладёт сборка.
std::filesystem::path exeDirectory() {
    wchar_t path[MAX_PATH] = {};

    ::GetModuleFileNameW(nullptr, path, MAX_PATH);

    return std::filesystem::path(path).parent_path();
}

/// Отказ словами, которые можно показать. Транспорт называет причину сам
/// (сеть, сертификат, код ответа); всё прочее -- редкость, и о ней честнее
/// сказать «не вышло», чем выдумывать объяснение.
std::wstring reasonOf(const std::exception_ptr& why) {
    try {
        std::rethrow_exception(why);
    } catch (const forum::HttpError& refused) {
        // Ни проверки, ни перекодировки: текст починен там, где вошёл в
        // программу, и досюда доехал проверенным типом. Здесь он всего лишь
        // выходит наружу -- в обычную широкую строку, какую ждёт XAML.
        const std::wstring_view said = refused.said().wchars();

        return refused.status() == 0 ? std::format(L"Сервер недоступен: {}", said)
                                     : std::format(L"Сервер отказал: {}", said);
    } catch (const std::exception&) {
        return L"Не вышло поговорить с сервером.";
    }
}

}  // namespace

wxl::Teardown wxl_launched() {
    auto window = Window{
        title = L"Беседка",
        minSize = {820, 560},
    };

    auto settings = std::make_shared<Settings>(loadSettings());

    // Умолчание ставится всегда, и лишь потом накрывается запомненным:
    // placement молча ничего не делает, когда разбирать нечего, -- и это
    // правильно, но своё умолчание к тому моменту должно быть уже на месте.
    window.appWindow().resize({kInitialWidth, kInitialHeight});

    if (!settings->windowPlacement.empty()) window.placement(settings->windowPlacement);

    // ---- запоминание места окна ----
    //
    // Таймер очереди интерфейса, а не сон и не поток: каждое движение окна
    // отодвигает запись, и пишется она один раз, когда рука отпустила рамку.
    auto saveTimer = window.dispatcherQueue().createTimer();

    saveTimer.interval(kSaveQuiet);
    saveTimer.isRepeating(false);

    const auto rememberWindow = [window, settings] {
        // Текст непрозрачный, и приложение его не читает: wxl выдала --
        // приложение донесло до файла. Приведение нужно потому, что строка
        // wxl держит char16_t, а настройки -- обычную wchar_t; на Windows это
        // один и тот же тип по размеру и по смыслу.
        settings->windowPlacement = std::wstring(
            reinterpret_cast<const wchar_t*>(wxl::window_placement(window).c_str()));

        saveSettings(*settings);
    };

    saveTimer.add_onTick([saveTimer, rememberWindow](Object const&, Object const&) {
        saveTimer.stop();
        rememberWindow();
    });

    // Changed приходит и на перемещение, и на изменение размера, и на смену
    // представления -- то есть на всё, что запоминается.
    window.appWindow().add_onChanged([saveTimer](Object const&, AppWindowChangedEventArgs&) {
        saveTimer.stop();
        saveTimer.start();
    });

    // Закрытие -- последний шанс: таймер после него уже не тикнет.
    window.add_onClosed([saveTimer, rememberWindow](Object const&, WindowEventArgs&) {
        saveTimer.stop();
        rememberWindow();
    });

    // Задник окна -- картинка, и она же видна сквозь страницу: своей заливки
    // у страниц нет, красят себя только полосы и карточки. Заодно это лечит
    // просвет при быстрой растяжке: остров XAML отстаёт от рамки на
    // такт-другой, и без задника там видна поверхность окна, стёртая белой
    // кистью WinUI. wxl забирает её себе и рисует картинку сам,
    // синхронно.
    //
    // Картинок две: заставка, пока читается витрина, и своя для чтения
    // форума. Меняется прямо на ходу -- wxl перерисовывает задник по вызову.
    const std::filesystem::path assets = exeDirectory() / L"Assets";

    const auto showBackdrop = [window, assets](const wchar_t* name) {
        wxl::window_backdrop_image(window, (assets / name).c_str());
    };

    showBackdrop(L"splash-screen.png");

    auto api = std::make_shared<forum::Api>();

    auto shell = std::make_shared<Shell>();
    auto splash = std::make_shared<SplashScreen>();
    auto forums = std::make_shared<ForumScreen>();
    auto topics = std::make_shared<TopicScreen>();
    auto messages = std::make_shared<MessageScreen>();
    auto about = std::make_shared<AboutDialog>();

    messages->setBaseDirectory((exeDirectory() / L"Assets").wstring());

    // Заглушки соседних вкладок строятся один раз: они ничего не показывают,
    // и меняться им не от чего.
    const UIElement watched = watchedScreen();
    const UIElement outbox = outboxScreen();

    // ---- витрина форумов ----
    //
    // Запрос уходит с этого потока, и продолжения приходят на него же:
    // C++/WinRT возвращает корутину в апартамент, из которого её начали.
    // Поэтому внутри обработчика можно и трогать XAML, и разбирать ответ.
    const auto loadForums = [api, splash, forums, shell, showBackdrop] {
        splash->setStatus(L"Читаю список форумов…");

        // Заставка -- и на экране, и задником: обе картинки одна и та же, и
        // в просвете при растяжке не видно шва.
        showBackdrop(L"splash-screen.png");

        // Панели на время заставки убираются: жать «обновить» и переключать
        // вкладки, пока не прочитан первый ответ, нечего. У jana на этом
        // месте пустой Scaffold с одним колечком посередине.
        shell->setChromeVisible(false);
        shell->setContent(splash->root());
        shell->setBusy(true);
        shell->setStatusText(L"Соединяюсь с api.rsdn.org…");

        api->forums()
            .when_succeeded([forums, shell, showBackdrop](
                                const std::vector<forum::ForumDescription>& list) noexcept {
                forums->show(list);

                // Читаем форум -- и задник становится своим для чтения.
                showBackdrop(L"forum.png");

                shell->setBusy(false);
                shell->setServerStatus(ServerStatus::online);
                shell->setChromeVisible(true);
                shell->setContent(forums->root());
                shell->setStatusText(std::format(L"Форумов на сервере: {}", list.size()));
            })
            .when_failed([splash, shell](const std::exception_ptr& why) noexcept {
                splash->setError(reasonOf(why));

                shell->setBusy(false);
                shell->setServerStatus(ServerStatus::offline);
                shell->setStatusText(L"Сервер не ответил");
            });
    };

    splash->onRetry = loadForums;
    shell->onRefresh = loadForums;

    // ---- вкладки ----
    //
    // За двумя из трёх пока заглушки, и это то же самое, что у jana:
    // WatchedScreen и OutboxScreen там ровно такие же.
    shell->onTab = [forums, shell, watched, outbox](const Tab chosen) {
        switch (chosen) {
            case Tab::forums: shell->setContent(forums->root()); break;
            case Tab::watched: shell->setContent(watched); break;
            case Tab::outbox: shell->setContent(outbox); break;
        }
    };

    // ---- «О программе» ----
    //
    // Версия сервера спрашивается при каждом показе, а не один раз при
    // запуске: диалог открывают редко, а запрос этот дешевле любого другого
    // в приложении.
    shell->onAbout = [api, about, window] {
        about->setServerLine({});
        about->show(window);

        api->serviceInfo()
            .when_succeeded([about](const forum::ServiceInfo& info) noexcept {
                about->setServerLine(
                    std::format(L"API v{} [{:%d.%m.%Y}]", info.serverVersion,
                                std::chrono::floor<std::chrono::days>(info.serverBuildDate)));
            })
            .when_failed([](const std::exception_ptr&) noexcept {});
    };

    // ---- вход ----
    //
    // Кнопка стоит на своём месте с самого начала, а войти ей пока некуда:
    // один POST на /connect/token -- работа после хранилища, и дверь для
    // него в транспорте уже есть.
    shell->onLogin = [shell] {
        shell->setStatusText(L"Вход ещё не сделан: за кнопкой будет POST на /connect/token.");
    };

    // ---- темы форума ----
    forums->onOpen = [api, topics, shell](const forum::ForumDescription& forum) {
        topics->setForum(forum);

        // Панели уходят: у экрана тем свой заголовок со стрелкой назад, и
        // две полосы подряд читались бы как одна сломанная. Так же и у jana.
        shell->setChromeVisible(false);
        shell->setContent(topics->root());
        shell->setStatusText(forum.name);

        api->topics(forum.id, 50)
            .when_succeeded(
                [topics](const forum::MessagePage& page) noexcept { topics->show(page); })
            .when_failed([topics, shell](const std::exception_ptr& why) noexcept {
                topics->setError(reasonOf(why));
                shell->setServerStatus(ServerStatus::offline);
            });
    };

    topics->onBack = [forums, shell] {
        shell->setChromeVisible(true);
        shell->setContent(forums->root());
        shell->setStatusText({});
    };

    // ---- сообщения темы ----
    //
    // Тела приезжают вместе со списком: одна поездка на всю тему.
    topics->onOpen = [api, messages, shell](const forum::MessageInfo& topic) {
        messages->setTopic(topic);

        shell->setContent(messages->root());
        shell->setStatusText(topic.subject);

        api->answers(topic.id, 200)
            .when_succeeded(
                [messages](const forum::MessagePage& page) noexcept { messages->show(page); })
            .when_failed([messages, shell](const std::exception_ptr& why) noexcept {
                messages->setError(reasonOf(why));
                shell->setServerStatus(ServerStatus::offline);
            });
    };

    messages->onBack = [topics, shell] {
        shell->setContent(topics->root());
        shell->setStatusText({});
    };

    window.content(shell->root());
    window.activate();

    loadForums();

    // Обработчик держит окно и экраны живыми ровно столько, сколько живёт
    // приложение.
    return [window, api, shell, splash, forums, topics, messages, about](Reason) {};
}
