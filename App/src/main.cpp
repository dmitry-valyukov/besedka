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

#include "CompositionWindow.h"
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

// Каким окно открывается в первый раз, когда запоминать ещё нечего.
constexpr int32_t kInitialWidth = 1280;
constexpr int32_t kInitialHeight = 860;

// Задники окна. Их два, и они не лежат рядом, а сменяют друг друга на одном и
// том же визуале сцены: заставка, пока читается витрина, и картинка чтения
// дальше. Ни одна из них не повторяется в дереве XAML.
constexpr wchar_t kSplashBackdrop[] = L"splash-screen.png";
constexpr wchar_t kForumBackdrop[] = L"forum.png";

// Размер картинки заставки -- по нему окно берёт свои пропорции, пока она на
// экране. Растягивается она UniformToFill, то есть в окне других пропорций
// края уезжают за рамку; в окне тех же пропорций видна вся. Сменится
// картинка -- сменятся и эти числа.
constexpr int kSplashImageWidth = 1254;
constexpr int kSplashImageHeight = 1254;

// Чем окно закрашено, пока картинка не доехала. Поверхности перенаправления у
// него нет вовсе, и неокрашенным оно сквозит на рабочий стол; тон -- тёмная
// земля обеих картинок, так что подмена не мигает.
constexpr ARGB kEmptyBackdrop{0xFF17120Eu};

// Сколько окно должно постоять смирно, прежде чем его место запишут. Тянуть
// рамку мышью -- это сотни событий в секунду, и запись на каждое из них была бы
// файлом, переписанным сотни раз ради одного числа.
constexpr auto kSaveQuiet = std::chrono::milliseconds(800);

/// Окно под заставку: клиентская область в пропорциях картинки и настолько
/// большая, насколько её пускает экран.
///
/// Считается именно клиентская: картинка живёт в ней, а не в окне, и окно
/// пропорций картинки показало бы её кадрированной ровно на рамку. Рамку
/// вычитать не приходится: окно и спрашивают, и просят в клиентских единицах,
/// а свою рамку оно знает само.
void shapeForSplash(CompositionWindow& window) {
    const SizeInt32 room = window.maxClientSize();

    // Во всю рабочую область по высоте -- это и есть «максимально»: выше
    // только полноэкранный режим, а он для заставки был бы заявкой не по чину.
    double height = room.height;
    double width = height * kSplashImageWidth / kSplashImageHeight;

    // Картинка бывает и шире экрана -- тогда предел ставит ширина.
    if (width > room.width) {
        width = room.width;
        height = width * kSplashImageHeight / kSplashImageWidth;
    }

    window.centreWithClientSize({static_cast<int32_t>(width), static_cast<int32_t>(height)});
}

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
    // Своё окно верхнего уровня на композиторе, а не генерируемое wxl::Window.
    // Две причины, и обе про задник. Первая -- он тут вообще виден: XAML
    // приходит в это окно прозрачным островом, и картинка сцены стоит ЗА
    // страницей, а не только в просвете, который остров не успел закрасить.
    // Пока окно было обычным, единственным способом показать картинку под
    // содержимым была её вторая копия в дереве XAML -- та самая, которой
    // теперь нет. Вторая -- у окна нет поверхности перенаправления
    // (WS_EX_NOREDIRECTIONBITMAP), значит нечему и белеть при быстрой растяжке
    // за угол.
    //
    // Move-only (владеет HWND и островами), поэтому в shared_ptr: его держат
    // обработчики и Teardown.
    auto window = std::make_shared<CompositionWindow>(L"Беседка", SizeInt32{820, 560});

    auto settings = std::make_shared<Settings>(loadSettings());

    // Окно открывается под заставку -- пропорциями её картинки, а не своими
    // рабочими. Запомненное место ждёт витрины: пока читается список форумов,
    // на экране только картинка, и растягивать её в рабочее окно, чтобы через
    // секунду сменить содержимое, значит показать два разных окна подряд.
    shapeForSplash(*window);

    // Пока место не восстановлено, ничего и не запоминается: иначе первым же
    // делом на месте окна читателя оказались бы пропорции заставки, поставленные
    // нами самими.
    auto restored = std::make_shared<bool>(false);

    // ---- запоминание места окна ----
    //
    // Таймер очереди интерфейса, а не сон и не поток: каждое движение окна
    // отодвигает запись, и пишется она один раз, когда рука отпустила рамку.
    auto saveTimer = window->dispatcherQueue().createTimer();

    saveTimer.interval(kSaveQuiet);
    saveTimer.isRepeating(false);

    const auto rememberWindow = [window, settings, restored] {
        if (!*restored) return;

        // Текст непрозрачный, и приложение его не читает: окно выдало --
        // приложение донесло до файла.
        settings->windowPlacement = window->placement();

        saveSettings(*settings);
    };

    // Витрина открылась -- окно принимает свой рабочий вид. Умолчание и
    // запомненное разведены: `placement` молча ничего не делает, когда
    // разбирать нечего, поэтому первый запуск ставится по центру сам.
    const auto restoreWindow = [window, settings, restored] {
        if (std::exchange(*restored, true)) return;

        if (settings->windowPlacement.empty())
            window->centreWithClientSize({kInitialWidth, kInitialHeight});
        else
            window->placement(settings->windowPlacement);
    };

    saveTimer.add_onTick([saveTimer, rememberWindow](Object const&, Object const&) {
        saveTimer.stop();
        rememberWindow();
    });

    // Приходит и на перемещение, и на изменение размера, и на смену
    // представления -- то есть на всё, что запоминается.
    window->onGeometryChanged([saveTimer, restored] {
        if (!*restored) return;

        saveTimer.stop();
        saveTimer.start();
    });

    // Закрытие -- последний шанс: таймер после него уже не тикнет.
    window->onClosed([saveTimer, rememberWindow] {
        saveTimer.stop();
        rememberWindow();
    });

    // ---- задник ----
    //
    // Он один на всё приложение: единственный визуал сцены, под прозрачным
    // островом XAML. Картинок для него две -- заставка и чтение форума, -- но
    // лежат они не рядом, а по очереди: `background` ставит новую кисть на тот
    // же самый визуал, вытесняя прежнюю. Ни второго визуала, ни второй копии
    // картинки в дереве XAML нет и не будет: закрытый собою битмап -- это
    // мегабайты, висящие в памяти и в композиции зря, и две картинки, которые
    // надо держать в согласии руками.
    //
    // Загрузка асинхронная (Win2D через wxl::TextureCache): декод идёт на
    // потоках WinRT, интерфейс не подвисает, а уже прочитанная картинка отдаётся
    // из кэша -- возврат к заставке второй загрузки не стоит.
    const std::filesystem::path assets = exeDirectory() / L"Assets";

    const auto showBackdrop = [window, assets](const wchar_t* name) {
        window->backgroundAsync(assets / name);
    };

    // До первой картинки -- ровный тон: окно уже показано, а декод ещё идёт, и
    // незакрашенное окно на этом месте сквозило бы на рабочий стол.
    window->background(kEmptyBackdrop);

    showBackdrop(kSplashBackdrop);

    auto api = std::make_shared<forum::Api>();

    auto shell = std::make_shared<Shell>();
    auto splash = std::make_shared<SplashScreen>();
    auto forums = std::make_shared<ForumScreen>();
    auto topics = std::make_shared<TopicScreen>();
    auto messages = std::make_shared<MessageScreen>();
    auto about = std::make_shared<AboutDialog>();

    messages->setBaseDirectory((exeDirectory() / L"Assets").wstring());

    // ---- ширина, под которую раскладываются страницы ----
    //
    // Приходит от окна из WM_SIZE, то есть до того, как XAML возьмётся за
    // вёрстку. Спрашивать её у элемента через SizeChanged нельзя: то событие
    // приходит уже изнутри прохода вёрстки, а каркас на него снимает и
    // возвращает страницы.
    //
    // Делится на масштаб прямо здесь: окно отдаёт физические пиксели, а
    // каркасу нужны логические -- в них меряется текст, ради которого порог и
    // существует.
    window->onClientSizeChanged([shell](SizeInt32 client, float scale) {
        shell->setWidth(client.width / scale);
    });

    shell->setWidth(window->clientSize().width / window->rasterizationScale());

    // Граница между страницами встаёт туда, где её оставили, и запоминается
    // тем же отложенным таймером, что и место окна: тянуть её мышью -- это
    // сотни событий в секунду, а файл переписывается целиком.
    shell->splitFraction(settings->splitFraction);

    shell->onSplitChanged = [settings, saveTimer](const double fraction) {
        settings->splitFraction = fraction;

        saveTimer.stop();
        saveTimer.start();
    };

    // Заглушки соседних вкладок строятся один раз: они ничего не показывают,
    // и меняться им не от чего.
    const UIElement watched = watchedScreen();
    const UIElement outbox = outboxScreen();

    // ---- витрина форумов ----
    //
    // Запрос уходит с этого потока, и продолжения приходят на него же:
    // C++/WinRT возвращает корутину в апартамент, из которого её начали.
    // Поэтому внутри обработчика можно и трогать XAML, и разбирать ответ.
    const auto loadForums = [api, splash, forums, shell, showBackdrop, restoreWindow] {
        splash->setStatus(L"Читаю список форумов…");

        // Заставка возвращается задником: «Ещё раз» после отказа ведёт сюда
        // же, а к тому времени на окне может стоять картинка чтения.
        showBackdrop(kSplashBackdrop);

        // Панели на время заставки убираются: жать «обновить» и переключать
        // вкладки, пока не прочитан первый ответ, нечего. У jana на этом
        // месте пустой Scaffold с одним колечком посередине.
        //
        // Полоса состояния уходит вместе с ними, хотя на форуме и в теме
        // остаётся: на заставке она пересказывала бы своими словами то, что
        // уже написано на карточке, и отрезала бы у картинки полосу снизу.
        // Текст ей всё же говорится -- он понадобится, когда она вернётся.
        shell->showSplash(splash->root());
        shell->setBusy(true);
        shell->setStatusText(L"Соединяюсь с api.rsdn.org…");

        api->forums()
            .when_succeeded([forums, shell, showBackdrop, restoreWindow](
                                const std::vector<forum::ForumDescription>& list) noexcept {
                forums->show(list);

                // Витрина есть -- значит окну пора принять свой рабочий вид:
                // запомненное место ставится здесь и только здесь. Раньше
                // содержимого нет, а без содержимого окно рабочего размера --
                // это пустая рамка.
                restoreWindow();

                // Читаем форум -- и задник становится своим для чтения.
                showBackdrop(kForumBackdrop);

                shell->setBusy(false);
                shell->setServerStatus(ServerStatus::online);
                shell->showRoot({forums->root(), [forums](bool secondary) { forums->setSecondary(secondary); }});
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
    // Смена вкладки начинает стопку заново: вкладка -- это верхний уровень, и
    // открытое в прежней к ней не относится.
    shell->onTab = [forums, shell, watched, outbox](const Tab chosen) {
        switch (chosen) {
            case Tab::forums: shell->showRoot({forums->root(), [forums](bool secondary) { forums->setSecondary(secondary); }}); break;
            case Tab::watched: shell->showRoot({watched, {}}); break;
            case Tab::outbox: shell->showRoot({outbox, {}}); break;
        }
    };

    // ---- «О программе» ----
    //
    // Версия сервера спрашивается при каждом показе, а не один раз при
    // запуске: диалог открывают редко, а запрос этот дешевле любого другого
    // в приложении.
    shell->onAbout = [api, about, window] {
        about->setServerLine({});
        // Над содержимым окна, то есть над островом, в котором оно стоит.
        // Спрашиваем у окна, а не капчурим каркас: тот держит этот самый
        // обработчик, и ссылка на него отсюда замкнула бы владение в кольцо.
        about->show(window->content());

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

        // Кладётся на стопку: в одностраничном показе темы заменят витрину,
        // в двухстраничном встанут справа от неё. Панелями каркас
        // распоряжается сам -- они уходят, когда витрина перестаёт быть на
        // виду слева.
        shell->open({topics->root(), [topics](bool secondary) { topics->setSecondary(secondary); }});
        shell->setStatusText(forum.name);

        api->topics(forum.id, 50)
            .when_succeeded(
                [topics](const forum::MessagePage& page) noexcept { topics->show(page); })
            .when_failed([topics, shell](const std::exception_ptr& why) noexcept {
                topics->setError(reasonOf(why));
                shell->setServerStatus(ServerStatus::offline);
            });
    };

    topics->onBack = [shell] {
        shell->back();
        shell->setStatusText({});
    };

    // ---- сообщения темы ----
    //
    // Тела приезжают вместе со списком: одна поездка на всю тему.
    topics->onOpen = [api, messages, shell](const forum::MessageInfo& topic) {
        messages->setTopic(topic);

        shell->open({messages->root(), {}});
        shell->setStatusText(topic.subject);

        api->answers(topic.id, 200)
            .when_succeeded(
                [messages](const forum::MessagePage& page) noexcept { messages->show(page); })
            .when_failed([messages, shell](const std::exception_ptr& why) noexcept {
                messages->setError(reasonOf(why));
                shell->setServerStatus(ServerStatus::offline);
            });
    };

    messages->onBack = [shell] {
        shell->back();
        shell->setStatusText({});
    };

    window->content(shell->root());
    window->activate();

    loadForums();

    // Обработчик держит окно и экраны живыми ровно столько, сколько живёт
    // приложение.
    return [window, api, shell, splash, forums, topics, messages, about](Reason) {};
}
