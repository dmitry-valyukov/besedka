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
//
// Настройки пишет цикл-корутина writeSettings, которую держит приложение
// (`DeferredWrite`): перемена отодвигает запись, закрытие окна будит цикл на
// последнюю, а держатель докручивает его, умирая вместе с приложением. Запись
// токена не берёт и доходит до конца; сбой записи цикл говорит читателю и
// ждёт дальше -- следующая перемена запишет снова.

// Свои заголовки со стандартными внутри -- до всего, что тянет import
// wxl.core: заголовок, включённый после импорта, MSVC уже не принимает.
#include <windows.h>

#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <memory>
#include <utility>

#include "CompositionWindow.h"
#include "WaitableTimer.h"
#include "ZoomEffect.h"
#include "deferred_write.h"
#include "navigator.h"
#include "shell.h"
#include "palette.h"

// Последним из своих: он кончается импортами.
#include "notices.h"

import besedka.app;
import besedka.forum;
import wxl.async;

using namespace wxl;
using namespace wxl::dsl;
using namespace std::chrono_literals;

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

// Пауза, после которой место окна, граница страниц и масштаб попадают на диск.
// Окно таскают и границу двигают непрерывно, а писать на каждый пиксель
// незачем: рука, отпустившая рамку, за это время её не схватит снова.
constexpr auto kSaveQuiet = 800ms;

/// Каталог рядом с исполняемым файлом. Путь без схемы XAML разрешает именно
/// оттуда, и ресурсы туда же кладёт сборка.
std::filesystem::path exeDirectory() {
    wchar_t path[MAX_PATH] = {};

    ::GetModuleFileNameW(nullptr, path, MAX_PATH);

    return std::filesystem::path(path).parent_path();
}

/// Приложение целиком: поля по значению, в порядке жизни (строятся сверху
/// вниз, умирают снизу вверх). Одно выделение из STA-пула, счётчик без
/// interlocked -- поток один. Создаётся `make_refcounted<Besedka>()`; нигде не
/// передаётся по значению, обработчики окна и каркаса держат голый `this` --
/// сильная ссылка из своего же поля была бы кольцом. Окно -- после каркаса и
/// переходов: в его скобках уже подписаны обработчики, которые их зовут. После
/// окна -- сообщения (им нужно окно), цикл записи настроек (ему нужна очередь
/// окна, а о сбое он говорит через сообщения; обработчики окна будят его, но
/// зовутся не раньше, чем приложение построено), подписки и задача запуска.
/// Конструктор диска не трогает и сценариев не запускает: окно невидимо,
/// настройки -- умолчания; остальное делает запуск (start).
struct Besedka final : wxl::core::sta_refcounted, private wxl::core::noncopyable {
    std::filesystem::path assets = exeDirectory() / L"Assets";

    /// Общий источник отмены: от него -- дочерние источники сценариев и
    /// сообщений, под токенами которых те ждут. Отменяет его закрытие окна,
    /// один раз на жизнь приложения: обратно отмену не взять.
    wxl::async::cancellation_source stop;

    /// Рабочее место: настройки и их файл. Пустое: его наполнит запуск, и
    /// наполнит асинхронно -- в этом потоке к диску не обращаются.
    Workspace workspace{dataDirectory()};

    forum::Api api;
    Shell shell;
    Navigator navigator{api, shell, assets, stop.token()};

    /// Масштаб окна -- эффект wxl: клавиши, Ctrl+колесо, жест масштабирования и
    /// flyout с числом. Сохранённый масштаб ставит модели запуск, прочитав
    /// настройки, -- до показа окна, так что окно открывается уже в нём.
    ZoomEffect zoom;

    /// Тема XAML показанного экрана. Диалог -- не часть дерева экрана и его
    /// темы не наследует: сообщения берут её отсюда. Тема у Беседки пока одна,
    /// системная, и экраны своей не задают: все они в теме окна, и диалогу она
    /// та же -- умолчание. Потому поле ни за чем не следует: следовать не за
    /// чем, пока у экранов нет своих тем.
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

        // Место окна -- у него самого, пока оно есть: после Closed спросить
        // будет не у кого. Потом отмена: сценарий, который ждёт под токеном,
        // кончится на этом ожидании, открытое сообщение и «О программе»
        // закроются, а запись, которая уже идёт, токена не берёт и доходит до
        // конца. С общим источником отменён и источник цикла записи, так что
        // «сейчас» следом будит его на последнюю запись: он просыпается внутри
        // вызова, снимает настройки этого мига и уходит писать. Дожидается
        // цикла его держатель, умирая с приложением.
        onClosed += [this] {
            if (geometryWatch) windowMoved();
            stop.cancel();
            settingsWrite.signal();
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

    /// Цикл записи настроек (writeSettings): место окна, граница страниц,
    /// масштаб. После сообщений -- о сбое записи он говорит через них, и
    /// последнюю запись делает, умирая, раньше них. Стартует здесь же и сразу
    /// встаёт на ожидание таймера: лямбда только отдаёт держателю
    /// корутину-член, а до пробуждения цикл полей не трогает.
    DeferredWrite settingsWrite{window.dispatcherQueue(), kSaveQuiet, stop.token(),
                                [this](WaitableTimer& timer, wxl::async::cancellation_token token) {
                                    return writeSettings(timer, std::move(token));
                                }};

    Besedka();

    /// Запуск: настройки, затем показ окна и первый экран. Сценарий: его
    /// задачу держит поле `starting`, вызов возвращается на первом ожидании.
    void start();

    /// Задник -- один на всё приложение, единственный визуал сцены под
    /// прозрачным островом XAML. `backgroundAsync` ставит новую кисть на тот
    /// же самый визуал, вытесняя прежнюю; загрузка асинхронная (Win2D через
    /// wxl::TextureCache), уже прочитанная картинка отдаётся из кэша.
    ///
    /// Картинку ждёт кадр, которого ждёт и выход, -- под общим токеном:
    /// источник загрузки окно заводит дочерним от `stop`, и закрытие окна её
    /// отменяет.
    void showBackdrop(const wchar_t* name) { window.backgroundAsync(assets / name, stop.token()); }

    /// Окно под заставку: клиентская область в пропорциях картинки и
    /// настолько большая, насколько её пускает экран. Рамку вычитать не
    /// приходится: окно и спрашивают, и просят в клиентских единицах, а свою
    /// рамку оно знает само.
    void shapeForSplash();

    /// Витрина открылась -- окно принимает свой рабочий вид, и ровно один раз:
    /// второй раз его не ставят, раз место уже слушается (geometryWatch).
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

    /// Цикл записи настроек. Его держит settingsWrite.
    wxl::async::task<> writeSettings(WaitableTimer& timer, wxl::async::cancellation_token token);

    /// Окно сдвинулось или сменило размер: место снимается сейчас, а запись
    /// отодвигается. Снимается в миг перемены, а не в цикле записи: тот может
    /// проснуться и после Closed, когда окна уже нет. Его же зовёт закрытие
    /// окна -- последний раз, пока окно есть.
    void windowMoved() noexcept;

    /// Масштаб сменился -- запись настроек отодвигается.
    void zoomChanged(double factor) noexcept;

    // Подписки -- после цикла записи: их слушатели будят его, и держатели
    // снимаются раньше, чем он умирает.

    /// Место окна; появляется, когда окно поставлено на место из настроек
    /// (restoreWindowOnce): перемены до того -- наши, а не читателя, и первым же
    /// делом на месте окна читателя оказались бы пропорции заставки.
    using GeometryWatch = wxl::WindowWatch<wxl::EventKey::GeometryChanged, &Besedka::windowMoved>;
    std::unique_ptr<GeometryWatch> geometryWatch;

    /// Масштаб окна (поле модели эффекта); появляется, когда масштаб поставлен
    /// из настроек (start): прочитанное из файла -- не перемена, которую надо
    /// записать обратно.
    using ZoomWatch = wxl::core::change_watch<wxl::core::observable<double const>, &Besedka::zoomChanged>;
    std::unique_ptr<ZoomWatch> zoomWatch;

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

    // ---- граница страниц: запоминается отложенной записью ----
    //
    // Границу, прочитанную из настроек, ставит запуск; отсюда -- та, что
    // двигает читатель.
    navigator.onSplitChanged = [this](const double fraction) {
        workspace.settings.splitFraction = fraction;
        settingsWrite.restart();
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

    zoom.model().setZoomFactor(workspace.settings.zoom);
    navigator.splitFraction(workspace.settings.splitFraction);

    // Запись на диск начинает слушать масштаб только теперь: прочитанное из
    // файла -- не перемена, которую надо записать обратно. Запоминается
    // ступень, на которую встала модель.
    zoomWatch = std::make_unique<ZoomWatch>(zoom.model().zoomFactor(), *this);

    window.activate();
    navigator.start();
}

/// Настройки целиком. Текст рабочее место собирает в момент вызова, до
/// ожидания: «сейчас» будит ждущий цикл внутри своего вызова, и на диск уходят
/// настройки этого мига.
wxl::async::task<> Besedka::writeSettings(WaitableTimer& timer, wxl::async::cancellation_token token) {
    for (;;) {
        co_await timer;

        // Отмена посреди записи -- ещё один круг.
        bool const last = token.is_canceled();

        try {
            co_await workspace.saveSettings();
        } catch (const wxl::async::last_error& failure) {
            notices.post(noticeOf(L"Не удалось сохранить настройки", workspace.settingsPath().wstring(), failure));
        }

        if (last) co_return;
    }
}

void Besedka::windowMoved() noexcept {
    workspace.settings.windowPlacement = window.placement();
    settingsWrite.restart();
}

void Besedka::zoomChanged(const double factor) noexcept {
    workspace.settings.zoom = factor;
    settingsWrite.restart();
}

void Besedka::shapeForSplash() {
    const SizeInt32 room = window.maxClientSize();
    const Extent client = fitToPicture({room.width, room.height}, kSplashPicture);

    window.centreWithClientSize({client.width, client.height});
}

void Besedka::restoreWindowOnce() {
    if (geometryWatch) return;

    if (workspace.settings.windowPlacement.empty())
        window.centreWithClientSize({kInitialWidth, kInitialHeight});
    else
        window.placement(workspace.settings.windowPlacement);

    // Место окна запоминается с этих пор, когда окно стоит там, где его
    // оставили. Перемены выше -- наши, а не читателя: место, снятое с них,
    // затёрло бы в настройках запомненное прежде, чем его поставят.
    geometryWatch = std::make_unique<GeometryWatch>(window, *this);

    // Место, где окно встало, -- в настройки сразу, как всякая перемена: окно
    // могло подогнаться под сегодняшние мониторы, а испорченный файл
    // настроек этой записью и заменяется, о чём запуск уже сказал читателю.
    windowMoved();
}

}  // namespace

wxl::Teardown wxl_launched() {
    // Приложение рождается со счётчиком 1, и make_refcounted эту ссылку
    // принимает, а не добавляет свою.
    auto const app = wxl::core::make_refcounted<Besedka>();

    app->start();

    // Обработчик держит приложение до разборки wxl и отпускает до пула STA;
    // задачи сценариев приложение держит само, в своих полях, и они уходят
    // вместе с ним. Последнюю запись настроек будит Closed окна, пока окно
    // ещё есть, а дописать её приложение даёт, умирая: держатель цикла записи
    // докручивает его, когда обработчик Teardown приложение отпускает.
    return [app](TeardownReason) {};
}
