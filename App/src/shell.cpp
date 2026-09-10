// windows.h -- до своих заголовков: те ведут к import, а стандартный или
// системный заголовок после импорта MSVC уже не принимает.
#include <windows.h>

#include "shell.h"

#include <cstdint>
#include <utility>

namespace besedka::app {

using namespace wxl;
using namespace wxl::dsl;

namespace {

// Значки числом, а не знаком в кавычках: они из области частного
// использования, и в исходнике на их месте стоит пустой прямоугольник -- по
// нему не видно ни что это, ни уцелел ли он при правке файла.
//
// У jana на их месте свои картинки в ресурсах (ic_list, ic_bookmark,
// ic_outbox, ic_refresh, ic_info и три значка темы). Мы берём те же по
// смыслу знаки из Segoe Fluent Icons: шрифт стоит в системе, знает обе темы
// и рисуется в любом кегле.
constexpr wchar_t kBack = 0xE72B;         // назад
constexpr wchar_t kForward = 0xE72A;      // вперёд
constexpr wchar_t kTabForums = 0xE8FD;    // список
constexpr wchar_t kTabWatched = 0xE734;   // звезда
constexpr wchar_t kTabOutbox = 0xE724;    // отправить
constexpr wchar_t kRefresh = 0xE72C;      // обновить
constexpr wchar_t kInfo = 0xE946;         // о программе
constexpr wchar_t kThemeSystem = 0xE713;  // системная

std::wstring glyph_of(wchar_t code) { return std::wstring(1, code); }

// Числа из jana (`ui/components/MainTopAppBar.kt`, `MainBottomBar.kt`,
// `StatusBar.kt`) и из шкалы Material 3, на которой она стоит.
constexpr double kBarPaddingX = 12;    // TopAppBar: отступ по краям
constexpr double kBarPaddingY = 6;
constexpr double kTitleSize = 20;      // Material 3 titleLarge
constexpr double kStatusSize = 11;     // Material 3 labelSmall
constexpr double kIconSide = 18;       // IconButton: значок внутри
constexpr double kDotSide = 10;        // ServerStatusIndicator, уменьшённый
constexpr double kStatusHeight = 22;   // StatusBar: 16dp текста плюс отступы
constexpr double kPanelRadius = 20;    // UserPanel: RoundedCornerShape(24)
constexpr double kPanelHeight = 32;    // UserPanel: height(40) без отступов

/// Кнопка-значок верхней панели: сама по себе прозрачная, как IconButton у
/// Material, и обязана иметь подсказку -- значок без подписи себя не
/// объясняет. Подсказка идёт и в имя для доступности: содержимое кнопки --
/// знак шрифта, и диктор без этого сказал бы «кнопка» и замолчал.
Button iconButton(wchar_t code, std::wstring_view hint, std::function<void()> action) {
    return Button{
        vAlign.center,
        Padding{8, 4},
        Margin{2, 0, 2, 0},
        background = brushes.SubtleFillColor.Transparent,
        BorderThickness{0},
        toolTip = std::wstring(hint),
        automationName = std::wstring(hint),
        content = FontIcon{glyph = glyph_of(code), fontSize = kIconSide},
        onClick = [action = std::move(action)](Object const&,
                                               RoutedEventArgs&) { if (action) action(); },
    };
}

/// Alt зажат. У KeyRoutedEventArgs модификаторов нет, а спрашивать саму
/// Windows в .cpp можно.
bool altHeld() { return (::GetKeyState(VK_MENU) & 0x8000) != 0; }

}  // namespace

Shell::Shell() {
    // ---- верхняя панель ----
    //
    // Порт MainTopAppBar: слева «назад» и «вперёд», имя форума и кружок
    // состояния; справа -- обновление, тема, панель пользователя и
    // «О программе», ровно в этом порядке.
    back_ = iconButton(kBack, L"Назад", [this] { if (onBack) onBack(); });
    forward_ = iconButton(kForward, L"Вперёд", [this] { if (onForward) onForward(); });

    back_.value().isEnabled(false);
    forward_.value().isEnabled(false);

    dot_ = Border{
        column = 3,
        width = kDotSide,
        height = kDotSide,
        CornerRadius{kDotSide / 2},
        vAlign.center,
        Margin{16, 0, 0, 0},
        background = brushes.Text.FillColor.Disabled,
        toolTip = L"Сервер ещё не отвечал",
    };

    refresh_ = iconButton(kRefresh, L"Обновить", [this] { if (onRefresh) onRefresh(); });

    // Колечко занимает место кнопки, а не встаёт рядом: у jana оно ровно
    // такое же, size(24), и подменяет её на время запроса.
    ring_ = ProgressRing{
        width = 20,
        height = 20,
        vAlign.center,
        Margin{10, 0, 10, 0},
        isActive = false,
        visibility = Visibility::Collapsed,
    };

    // Кнопка темы стоит там же, где у jana, и показывает то, что есть:
    // приложение идёт за темой системы. Переключать её пока нечем, и это не
    // недосмотр, а работа в wxl -- присвоенная кисть темы не следует за
    // сменой темы, а шаблоны WinUI после флипа перекрашиваются не все.
    // Проверено на этом же окне, записано в M:\wxl\TODO.txt.
    theme_ = iconButton(kThemeSystem, L"Системная тема", [this] {
        setStatusText(L"Тема пока только системная: присвоенная кисть за сменой темы не "
                      L"идёт -- работа записана в wxl.");
    });

    auto about = iconButton(kInfo, L"О программе", [this] { if (onAbout) onAbout(); });

    // Панель пользователя. Пока в ней одна кнопка «Войти»: имя, картинка и
    // выход появятся вместе со входом, который есть в docs/decisions.md и
    // ещё не сделан.
    auto user = Border{
        vAlign.center,
        height = kPanelHeight,
        Margin{8, 0, 4, 0},
        CornerRadius{kPanelRadius},
        background = brushes.Card.BackgroundFillColor.Default,
        borderBrush = brushes.Card.StrokeColorDefault,
        BorderThickness{1},
        Button{
            L"Войти",
            vAlign.center,
            Padding{14, 2},
            CornerRadius{kPanelRadius},
            background = brushes.SubtleFillColor.Transparent,
            BorderThickness{0},
            fontSize = 13,
            onClick = [this](Object const&, RoutedEventArgs&) { if (onLogin) onLogin(); },
        },
    };

    topBar_ = Border{
        row = 0,
        background = brushes.Layer.FillColorDefault,
        borderBrush = brushes.DividerStrokeColorDefault,
        BorderThickness{0, 0, 0, 1},
        Padding{kBarPaddingX, kBarPaddingY},

        Grid{
            columnDefinitions = L"auto,auto,auto,auto,*,auto,auto,auto,auto",

            Grid{column = 0, back_.value()},
            Grid{column = 1, forward_.value()},

            // Имя форума, а не приложения: имя приложения написано в
            // заголовке окна, а здесь -- то же, что у jana, чей форум мы
            // читаем.
            TextBlock{
                column = 2,
                L"RSDN",
                fontSize = kTitleSize,
                FontWeight{600},
                vAlign.center,
                Margin{8, 0, 0, 0},
                foreground = brushes.Text.FillColor.Primary,
            },
            dot_.value(),
            Grid{
                column = 5,
                vAlign.center,
                refresh_.value(),
                ring_.value(),
            },
            Grid{column = 6, theme_.value()},
            Grid{column = 7, user},
            Grid{column = 8, about},
        },
    };

    // ---- вкладки внизу ----
    //
    // Порт MainBottomBar. У Material это NavigationBar во всю ширину со
    // значком над словом; у WinUI то же место занимает SelectorBar --
    // значок и слово в строку, полосой посередине. Ближе к платформе взять
    // нечего: NavigationView крепит свою панель к краю кадра, а не к низу
    // окна.
    tabs_ = SelectorBar{
        hAlign.center,
        onSelectionChanged =
            [this](Object const&, SelectorBarSelectionChangedEventArgs&) {
                if (selectingTab_ || !onTab) return;

                const SelectorBar bar = tabs_.value();
                const SelectorBarItem chosen = bar.selectedItem();

                if (!chosen) return;

                // Какая это по счёту -- узнаётся сравнением подписей: у
                // обёрток нет равенства, а подпись у трёх вкладок своя.
                const Collection<SelectorBarItem> all = bar.items();

                for (std::uint32_t at = 0; at < all.size(); ++at)
                    if (all[at].text() == chosen.text()) onTab(static_cast<Tab>(at));
            },
        SelectorBarItem{
            L"Форумы",
            icon = FontIcon{glyph = glyph_of(kTabForums)},
        },
        SelectorBarItem{
            L"Избранное",
            icon = FontIcon{glyph = glyph_of(kTabWatched)},
        },
        SelectorBarItem{
            L"Исходящие",
            icon = FontIcon{glyph = glyph_of(kTabOutbox)},
        },
    };

    selectTab(Tab::forums);

    tabsBar_ = Border{
        row = 2,
        background = brushes.Layer.FillColorDefault,
        borderBrush = brushes.DividerStrokeColorDefault,
        BorderThickness{0, 1, 0, 0},
        Padding{4, 4},
        tabs_.value(),
    };

    // ---- полоса состояния ----
    //
    // Порт StatusBar: у jana в ней адрес ссылки под указателем. Высота
    // держится и когда сказать нечего -- иначе окно дёргалось бы на каждое
    // слово.
    status_ = TextBlock{
        fontSize = kStatusSize,
        vAlign.center,
        textTrimming.characterEllipsis,
        foreground = brushes.Text.FillColor.Tertiary,
    };

    statusBar_ = Border{
        row = 3,
        height = kStatusHeight,
        background = brushes.SolidBackgroundFillColor.Secondary,
        borderBrush = brushes.DividerStrokeColorDefault,
        BorderThickness{0, 1, 0, 0},
        Padding{8, 0},
        status_.value(),
    };

    // Середина (строка 1) добавляется последней и последней же стоит в
    // коллекции: setMiddle снимает её с конца, не считая панелей.
    root_ = Grid{
        rowDefinitions = L"auto,*,auto,auto",

        // Клавиши браузера: «назад»/«вперёд» на клавиатуре и Alt со
        // стрелками. Событие поднимается сюда от того, у кого фокус, -- то
        // есть с любой страницы.
        onKeyDown =
            [this](Object const&, KeyRoutedEventArgs& args) {
                const VirtualKey key = args.key();

                if (key == VirtualKey::GoBack || (key == VirtualKey::Left && altHeld())) {
                    if (onBack) onBack();
                } else if (key == VirtualKey::GoForward || (key == VirtualKey::Right && altHeld())) {
                    if (onForward) onForward();
                } else {
                    return;
                }

                args.handled(true);
            },

        topBar_.value(),
        tabsBar_.value(),
        statusBar_.value(),
    };
}

void Shell::setMiddle(const Middle what, const UIElement& element) {
    if (middle_ == what) return;

    const Collection<UIElement> children = root_.value().children();

    if (middle_ != Middle::none) children.removeAtEnd();

    Grid::setRow(element.try_as<FrameworkElement>(), 1);
    children.append(element);

    middle_ = what;

    const Visibility chrome = what == Middle::pages ? Visibility::Visible : Visibility::Collapsed;

    topBar_.value().visibility(chrome);
    tabsBar_.value().visibility(chrome);
    statusBar_.value().visibility(chrome);
}

void Shell::showSplash(const UIElement& splash) { setMiddle(Middle::splash, splash); }

void Shell::showPages(const UIElement& pages) { setMiddle(Middle::pages, pages); }

void Shell::setCanGoBack(const bool can) { back_.value().isEnabled(can); }

void Shell::setCanGoForward(const bool can) { forward_.value().isEnabled(can); }

void Shell::selectTab(const Tab tab) {
    selectingTab_ = true;

    tabs_.value().selectedItem(tabs_.value().items()[static_cast<std::uint32_t>(tab)]);

    selectingTab_ = false;
}

void Shell::setServerStatus(const ServerStatus status) {
    switch (status) {
        case ServerStatus::online:
            dot_.value().background(brushes.SystemFillColor.Success);
            dot_.value().toolTip(L"Сервер отвечает");
            break;

        case ServerStatus::offline:
            dot_.value().background(brushes.SystemFillColor.Critical);
            dot_.value().toolTip(L"Сервер не отвечает");
            break;

        case ServerStatus::unknown:
            dot_.value().background(brushes.Text.FillColor.Disabled);
            dot_.value().toolTip(L"Сервер ещё не отвечал");
            break;
    }
}

void Shell::setBusy(const bool busy) {
    refresh_.value().visibility(busy ? Visibility::Collapsed : Visibility::Visible);

    // Колечко не просто прячется, а останавливается: остановленное и
    // крутящееся отличаются только тем, чего не видно на снимке, но анимация
    // невидимого элемента продолжает будить композитор.
    ring_.value().isActive(busy);
    ring_.value().visibility(busy ? Visibility::Visible : Visibility::Collapsed);
}

void Shell::setStatusText(const std::wstring_view said) { status_.value().text(said); }

}  // namespace besedka::app
