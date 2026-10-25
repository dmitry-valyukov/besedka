// Беседка -- клиент форума RSDN на wxl.ui.
//
// Здесь нет ни wWinMain, ни поднятия Windows App Runtime, ни наследника
// Application, ни XAML: всё это делает wxl и потом зовёт эту функцию.
//
// Что здесь есть -- сборка приложения из частей и то, что касается самого
// окна: задник, место, пропорции под заставку. Переходы между экранами -- в
// Navigator, панели -- в Shell, экраны -- каждый в своём файле, а решения,
// которые можно проверить без окна, -- в модуле besedka.app.
//
// Запросы задаются отсюда, с потока интерфейса, и сюда же C++/WinRT
// возвращает ответ -- на том потоке он и разбирается, потому что дерево JSON
// живёт в пуле STA.
//
// Запуск -- сценарий, корутина wxl.async (runStart): файл настроек читает
// рабочее место операцией wxl, а поток интерфейса тем временем крутит цикл
// сообщений. Сбой, которого сценарий сам не поймал, -- сообщение читателю
// (reported). Отмена -- деревом источников: у приложения один общий источник
// `stop`, а ждущий заводит свой, дочерний от него; общий отменяет закрытие
// окна.

// Свои заголовки со стандартными внутри -- до всего, что тянет import
// wxl.core: заголовок, включённый после импорта, MSVC уже не принимает.
#include <windows.h>

#include <cstdint>
#include <exception>
#include <filesystem>
#include <memory>
#include <utility>

#include "CompositionWindow.h"
#include "ZoomEffect.h"
#include "navigator.h"
#include "settings_writer.h"
#include "shell.h"
#include "palette.h"

// Последним из своих: он кончается импортами.
#include "notices.h"

import besedka.app;
import besedka.forum;
import wxl.async;

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
constexpr Extent kSplashPicture{1254, 1254};

/// Каталог рядом с исполняемым файлом. Путь без схемы XAML разрешает именно
/// оттуда, и ресурсы туда же кладёт сборка.
std::filesystem::path exeDirectory() {
    wchar_t path[MAX_PATH] = {};

    ::GetModuleFileNameW(nullptr, path, MAX_PATH);

    return std::filesystem::path(path).parent_path();
}

/// Приложение целиком. Один объект на куче: обработчики окна и каркаса держат
/// `this`, а не по указателю на каждого, и порядок жизни задан порядком
/// полей. Окно -- после всего, что зовут его обработчики: в его скобках уже
/// подписаны обработчики, которые зовут настройки, каркас и переходы, и всё
/// это к тому времени построено. После окна -- сообщения (им нужно окно) и
/// задача запуска. Конструктор диска не трогает и сценариев не запускает:
/// окно невидимо, настройки -- умолчания; остальное делает запуск (start).
struct Besedka {
    std::filesystem::path assets = exeDirectory() / L"Assets";

    /// Общий источник отмены: от него -- дочерние источники сценариев и
    /// сообщений, под токенами которых те ждут. Отменяет его закрытие окна,
    /// один раз на жизнь приложения: обратно отмену не взять.
    wxl::async::cancellation_source stop;

    /// Рабочее место: настройки и их файл. Пустое: его наполнит запуск, и
    /// наполнит асинхронно -- в этом потоке к диску не обращаются.
    Workspace workspace{dataDirectory()};

    SettingsWriter settings{DispatcherQueue::getForCurrentThread(), workspace};
    forum::Api api;
    Shell shell;
    Navigator navigator{api, shell, assets};

    // Пока место не восстановлено, ничего и не запоминается: иначе первым же
    // делом на месте окна читателя оказались бы пропорции заставки,
    // поставленные нами самими.
    bool restored = false;

    /// Масштаб окна -- эффект wxl: клавиши, Ctrl+колесо, жест масштабирования и
    /// flyout с числом. Сохранённый масштаб ставит модели запуск, прочитав
    /// настройки, -- до показа окна, так что окно открывается уже в нём.
    ZoomEffect zoom;

    /// Тема XAML показанного экрана. Диалог -- не часть дерева экрана и его
    /// темы не наследует: сообщения берут её отсюда. Тема у Беседки пока
    /// только системная, и экраны своей не задают: все они в теме окна, и
    /// диалогу она та же -- умолчание.
    wxl::core::observable<wxl::ElementTheme> screenTheme{wxl::ElementTheme::Default};

    // Своё окно верхнего уровня на композиторе, а не генерируемое wxl::Window.
    // Две причины, и обе про задник. Первая -- он тут вообще виден: XAML
    // приходит в это окно прозрачным островом, и картинка сцены стоит ЗА
    // страницей, а не только в просвете, который остров не успел закрасить.
    // Вторая -- у окна нет поверхности перенаправления
    // (WS_EX_NOREDIRECTIONBITMAP), значит нечему и белеть при быстрой растяжке
    // за угол.
    CompositionWindow window{
        title = L"Беседка",
        minSize = {820, 560},

        // Системный заголовок уходит, и клиентская область поднимается под
        // него. Верхняя панель каркаса -- заголовок окна: окно ставит её
        // строкой над каркасом и рядом рисует кнопки окна её высоты. Первым
        // делом: и пропорции заставки, и первый показ окна считаются уже без
        // системного заголовка.
        extendsContentIntoTitleBar = true,
        titleBar = shell.titleBar(),

        // Масштаб -- всего окна, заголовок и кнопки окна растут вместе со
        // страницами.
        zoom,

        // Приходит и на перемещение, и на изменение размера, и на смену
        // представления -- то есть на всё, что запоминается.
        onGeometryChanged += [this] {
            if (restored) settings.scheduleSave();
        },

        // Закрытие отменяет сценарии и сообщения: ожидание под общим
        // токеном кончается отменой, открытое сообщение закрывается. И
        // закрытие -- последний шанс записать настройки: таймер после него
        // уже не тикнет.
        onClosed += [this] {
            stop.cancel();
            if (restored) settings.saveNow();
        },

        // Ширина, под которую раскладываются страницы. Приходит от окна из
        // WM_SIZE, то есть до того, как XAML возьмётся за вёрстку. Делится на
        // масштаб прямо здесь: окно отдаёт физические пиксели, а страницам
        // нужны логические -- в них меряется текст, ради которого порог и
        // существует. Масштаб окна в делителе уже есть.
        onClientSizeChanged +=
            [this](Object const&, ClientSize const& client) {
                navigator.setWidth(client.size.width / client.scale);
            },

        shell.root(),
    };

    /// Сообщения читателю -- и о сбоях запуска, и о сбоях сценариев
    /// (reported). Ответа открытого сообщения ждёт под своим источником,
    /// дочерним от общего: закрытие окна его закрывает, а пришедшие после --
    /// пишет в stderr и вывод отладчика.
    Notices notices{window, screenTheme, stop.token()};

    Besedka();

    /// Запуск: настройки, затем показ окна и первый экран. Сценарий: его
    /// задачу держит поле `starting`, вызов возвращается на первом ожидании.
    void start();

    /// Задник -- один на всё приложение, единственный визуал сцены под
    /// прозрачным островом XAML. `backgroundAsync` ставит новую кисть на тот
    /// же самый визуал, вытесняя прежнюю; загрузка асинхронная (Win2D через
    /// wxl::TextureCache), уже прочитанная картинка отдаётся из кэша.
    void showBackdrop(const wchar_t* name) { window.backgroundAsync(assets / name); }

    /// Окно под заставку: клиентская область в пропорциях картинки и
    /// настолько большая, насколько её пускает экран. Рамку вычитать не
    /// приходится: окно и спрашивают, и просят в клиентских единицах, а свою
    /// рамку оно знает само.
    void shapeForSplash();

    /// Витрина открылась -- окно принимает свой рабочий вид, и ровно один раз.
    /// Умолчание и запомненное разведены: `placement` молча ничего не делает,
    /// когда разбирать нечего, поэтому первый запуск ставится по центру сам.
    void restoreWindowOnce();

    /// Ведёт сценарий и читает, чем он кончился: отмена -- обычный конец,
    /// всякий другой сбой, которого сценарий сам не поймал, -- сообщением
    /// читателю. Задачи сценариев в полях -- это задачи этого чтения.
    wxl::async::task<> reported(wxl::async::task<> scenario);

    /// Сбой словами читателю. noexcept: сбою самого показа уйти некуда.
    void reportFailure(std::exception_ptr failure) noexcept;

    /// Тело запуска.
    wxl::async::task<> runStart();

    // Задачи сценариев -- последними: их тела трогают всё, что выше, и
    // уходят раньше всего. К разрушению приложения они кончились: закрытие
    // окна отменило их ожидания, и выход ждал тел до Teardown.

    wxl::async::task<> starting;   ///< запуск (start)
};

Besedka::Besedka() {
    // Окно открывается под заставку -- пропорциями её картинки, а не своими
    // рабочими. Запомненное место ждёт витрины: пока читается список форумов,
    // на экране только картинка, и растягивать её в рабочее окно, чтобы через
    // секунду сменить содержимое, значит показать два разных окна подряд.
    shapeForSplash();

    // ---- место окна и граница страниц: запоминаются одним отложенным письмом ----
    //
    // Границу, прочитанную из настроек, ставит запуск; отсюда -- та, что
    // двигает читатель.
    settings.beforeSave = [this](Settings& saved) { saved.windowPlacement = window.placement(); };

    navigator.onSplitChanged = [this](const double fraction) {
        settings.settings().splitFraction = fraction;
        settings.scheduleSave();
    };

    // ---- задник ----
    //
    // До первой картинки -- ровный тон: окно уже показано, а декод ещё идёт, и
    // незакрашенное окно на этом месте сквозило бы на рабочий стол.
    window.background(palette.backdropTone);

    navigator.onSplash = [this] { showBackdrop(kSplashBackdrop); };

    navigator.onShowcase = [this] {
        restoreWindowOnce();
        showBackdrop(kForumBackdrop);
    };

    // Ширина под страницы до первого WM_SIZE, который придёт уже с подпиской.
    navigator.setWidth(window.clientSize().width / window.rasterizationScale());

    // ---- то, что на панелях ведёт не к переходу ----
    //
    // Над содержимым окна, то есть над островом, в котором оно стоит.
    shell.onAbout = [this] { navigator.showAbout(window.content()); };

    // Сообщение, пришедшее, пока «О программе» открыт, ждёт его закрытия:
    // XAML не держит двух диалогов разом.
    navigator.onAboutDismissed = [this] { notices.flush(); };

    // Кнопка стоит на своём месте с самого начала, а войти ей пока некуда:
    // один POST на /connect/token -- работа после хранилища, и дверь для
    // него в транспорте уже есть.
    shell.onLogin = [this] {
        shell.setStatusText(L"Вход ещё не сделан: за кнопкой будет POST на /connect/token.");
    };
}

void Besedka::start() {
    starting = reported(runStart());
}

wxl::async::task<> Besedka::reported(wxl::async::task<> scenario) {
    try {
        // Как своя, а не присоединением: ожидатель -- сама задача в параметре
        // кадра, а не временный рядом с ней, который разрушался бы раньше
        // параметра и уносил бы с собой пометку «её ждут».
        co_await std::move(scenario);
    } catch (const wxl::async::operation_canceled_exception&) {
        // Отмена -- обычный конец: её приносит закрытие окна.
    } catch (...) {
        // Через noexcept-показ: сбой самого показа уйти отсюда может только в
        // эту задачу, а её конца никто не читает, -- он останавливает процесс.
        reportFailure(std::current_exception());
    }
}

void Besedka::reportFailure(std::exception_ptr failure) noexcept {
    notices.post(noticeOf(std::move(failure)));
}

/// Окно строится невидимым, а показывается, когда прочитаны настройки:
/// масштаб, с которым оно открывается, лежит в них, и открыть его сначала в
/// одном, а потом переставить -- значит показать читателю прыжок. Ждать при
/// этом нечего: файл настроек читает рабочий поток, а этот тем временем уже
/// крутит цикл сообщений.
wxl::async::task<> Besedka::runStart() {
    // Настройки -- у рабочего места; что при этом не удалось, читатель узнаёт
    // словами, а запуск идёт дальше с умолчаниями.
    const Started started = co_await workspace.start();

    for (const Notice& notice : started.notices) notices.post(notice);

    zoom.model().setZoomFactor(settings.settings().zoom);
    navigator.splitFraction(settings.settings().splitFraction);

    // Запись на диск начинает слушать масштаб только теперь: прочитанное из
    // файла -- не перемена, которую надо записать обратно. Запоминается
    // ступень, на которую встала модель.
    zoom.model().zoomFactor().on_change([this](double const& factor) noexcept {
        settings.settings().zoom = factor;
        settings.scheduleSave();
    });

    window.activate();
    navigator.start();
}

void Besedka::shapeForSplash() {
    const SizeInt32 room = window.maxClientSize();
    const Extent client = fitToPicture({room.width, room.height}, kSplashPicture);

    window.centreWithClientSize({client.width, client.height});
}

void Besedka::restoreWindowOnce() {
    if (std::exchange(restored, true)) return;

    if (settings.settings().windowPlacement.empty())
        window.centreWithClientSize({kInitialWidth, kInitialHeight});
    else
        window.placement(settings.settings().windowPlacement);
}

}  // namespace

wxl::Teardown wxl_launched() {
    auto app = std::make_unique<Besedka>();

    app->start();

    // Обработчик держит приложение живым ровно столько, сколько живёт wxl;
    // задачи сценариев приложение держит само, в своих полях, и они уходят
    // вместе с ним.
    return [app = std::move(app)](TeardownReason) {};
}
