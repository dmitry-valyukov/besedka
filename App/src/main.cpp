// Беседка -- клиент форума RSDN на wxl.winui.
//
// Здесь нет ни wWinMain, ни поднятия Windows App Runtime, ни наследника
// Application, ни XAML: всё это делает wxl и потом зовёт эту функцию.
//
// Что здесь есть -- сборка приложения из частей и то, что касается самого
// окна: задник, место, пропорции под заставку. Переходы между экранами -- в
// Navigator, панели -- в Shell, экраны -- каждый в своём файле, а решения,
// которые можно проверить без окна, -- в модуле besedka.app. Как это сложено
// целиком -- docs/architecture.md.
//
// Запросы задаются отсюда, с потока интерфейса, и сюда же C++/WinRT
// возвращает ответ -- на том потоке он и разбирается. Почему иначе нельзя,
// написано в docs/decisions.md.

// Свои заголовки со стандартными внутри -- до всего, что тянет import
// wxl.core: заголовок, включённый после импорта, MSVC уже не принимает.
#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <utility>

#include "CompositionWindow.h"
#include "navigator.h"
#include "settings_writer.h"
#include "shell.h"
#include "palette.h"

import besedka.app;
import besedka.forum;

using namespace wxl;

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
/// полей -- переходы вместе с экранами разрушаются раньше каркаса, каркас
/// раньше окна.
struct Besedka {
    // Своё окно верхнего уровня на композиторе, а не генерируемое wxl::Window.
    // Две причины, и обе про задник. Первая -- он тут вообще виден: XAML
    // приходит в это окно прозрачным островом, и картинка сцены стоит ЗА
    // страницей, а не только в просвете, который остров не успел закрасить.
    // Вторая -- у окна нет поверхности перенаправления
    // (WS_EX_NOREDIRECTIONBITMAP), значит нечему и белеть при быстрой растяжке
    // за угол.
    CompositionWindow window{L"Беседка", SizeInt32{820, 560}};

    std::filesystem::path assets = exeDirectory() / L"Assets";

    SettingsWriter settings{window.dispatcherQueue(), loadSettings()};
    forum::Api api;
    Shell shell;
    Navigator navigator{api, shell, assets};

    // Пока место не восстановлено, ничего и не запоминается: иначе первым же
    // делом на месте окна читателя оказались бы пропорции заставки,
    // поставленные нами самими.
    bool restored = false;

    Besedka();

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
};

Besedka::Besedka() {
    // Окно открывается под заставку -- пропорциями её картинки, а не своими
    // рабочими. Запомненное место ждёт витрины: пока читается список форумов,
    // на экране только картинка, и растягивать её в рабочее окно, чтобы через
    // секунду сменить содержимое, значит показать два разных окна подряд.
    shapeForSplash();

    // ---- место окна и граница страниц: запоминаются одним отложенным письмом ----
    settings.beforeSave = [this](Settings& saved) { saved.windowPlacement = window.placement(); };

    // Приходит и на перемещение, и на изменение размера, и на смену
    // представления -- то есть на всё, что запоминается.
    window.onGeometryChanged([this] {
        if (restored) settings.scheduleSave();
    });

    // Закрытие -- последний шанс: таймер после него уже не тикнет.
    window.onClosed([this] {
        if (restored) settings.saveNow();
    });

    navigator.splitFraction(settings.settings().splitFraction);

    navigator.onSplitChanged = [this](const double fraction) {
        settings.settings().splitFraction = fraction;
        settings.scheduleSave();
    };

    // Масштаб из настроек -- до подписки: восстановленное записывать незачем,
    // а место окна в тот момент ещё заставочное.
    navigator.setZoom(settings.settings().zoom);

    navigator.onZoomChanged = [this](const double zoom) {
        settings.settings().zoom = zoom;
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

    // ---- ширина, под которую раскладываются страницы ----
    //
    // Приходит от окна из WM_SIZE, то есть до того, как XAML возьмётся за
    // вёрстку. Делится на масштаб прямо здесь: окно отдаёт физические пиксели,
    // а страницам нужны логические -- в них меряется текст, ради которого
    // порог и существует.
    window.onClientSizeChanged([this](const SizeInt32 client, const float scale) {
        navigator.setWidth(client.width / scale);
    });

    navigator.setWidth(window.clientSize().width / window.rasterizationScale());

    // ---- то, что на панелях ведёт не к переходу ----
    //
    // Над содержимым окна, то есть над островом, в котором оно стоит.
    shell.onAbout = [this] { navigator.showAbout(window.content()); };

    // Кнопка стоит на своём месте с самого начала, а войти ей пока некуда:
    // один POST на /connect/token -- работа после хранилища, и дверь для
    // него в транспорте уже есть.
    shell.onLogin = [this] {
        shell.setStatusText(L"Вход ещё не сделан: за кнопкой будет POST на /connect/token.");
    };

    window.content(shell.root());
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

    app->window.activate();
    app->navigator.start();

    // Обработчик держит приложение живым ровно столько, сколько живёт wxl.
    return [app = std::move(app)](Reason) {};
}
