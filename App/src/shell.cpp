#include "shell.h"

#include <cstdint>

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
        background = brushes.subtleFillColorTransparent,
        BorderThickness{0},
        toolTip = std::wstring(hint),
        automationName = std::wstring(hint),
        content = FontIcon{glyph = glyph_of(code), fontSize = kIconSide},
        onClick = [action = std::move(action)](Object const&,
                                               RoutedEventArgs&) { if (action) action(); },
    };
}

}  // namespace

Shell::Shell() {
    // ---- верхняя панель ----
    //
    // Порт MainTopAppBar: слева имя форума и кружок состояния, справа --
    // обновление, тема, панель пользователя и «О программе», ровно в этом
    // порядке.
    dot_ = Border{
        column = 1,
        width = kDotSide,
        height = kDotSide,
        CornerRadius{kDotSide / 2},
        vAlign.center,
        Margin{16, 0, 0, 0},
        background = brushes.textFillColorDisabled,
        toolTip = L"Сервер ещё не отвечал",
    };

    refresh_ = iconButton(kRefresh, L"Обновить списки", [this] { if (onRefresh) onRefresh(); });

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
        background = brushes.cardBackgroundFillColorDefault,
        borderBrush = brushes.cardStrokeColorDefault,
        BorderThickness{1},
        Button{
            L"Войти",
            vAlign.center,
            Padding{14, 2},
            CornerRadius{kPanelRadius},
            background = brushes.subtleFillColorTransparent,
            BorderThickness{0},
            fontSize = 13,
            onClick = [this](Object const&, RoutedEventArgs&) { if (onLogin) onLogin(); },
        },
    };

    topBar_ = Border{
        row = 0,
        background = brushes.layerFillColorDefault,
        borderBrush = brushes.dividerStrokeColorDefault,
        BorderThickness{0, 0, 0, 1},
        Padding{kBarPaddingX, kBarPaddingY},

        Grid{
            columnDefinitions = L"auto,auto,*,auto,auto,auto,auto",

            // Имя форума, а не приложения: имя приложения написано в
            // заголовке окна, а здесь -- то же, что у jana, чей форум мы
            // читаем.
            TextBlock{
                column = 0,
                L"RSDN",
                fontSize = kTitleSize,
                FontWeight{600},
                vAlign.center,
                Margin{4, 0, 0, 0},
                foreground = brushes.textFillColorPrimary,
            },
            dot_.value(),
            Grid{
                column = 3,
                vAlign.center,
                refresh_.value(),
                ring_.value(),
            },
            Grid{column = 4, theme_.value()},
            Grid{column = 5, user},
            Grid{column = 6, about},
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
                if (!onTab) return;

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

    tabs_.value().selectedItem(tabs_.value().items()[0]);

    tabsBar_ = Border{
        row = 2,
        background = brushes.layerFillColorDefault,
        borderBrush = brushes.dividerStrokeColorDefault,
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
        foreground = brushes.textFillColorTertiary,
    };

    host_ = Grid{row = 1};

    root_ = Grid{

        rowDefinitions = L"auto,*,auto,auto",

        topBar_.value(),
        host_.value(),
        tabsBar_.value(),

        Border{
            row = 3,
            height = kStatusHeight,
            background = brushes.solidBackgroundFillColorSecondary,
            borderBrush = brushes.dividerStrokeColorDefault,
            BorderThickness{0, 1, 0, 0},
            Padding{8, 0},
            status_.value(),
        },
    };
}

void Shell::setContent(const UIElement& screen) {
    host_.value().children().clear();
    host_.value().children().append(screen);
}

void Shell::setChromeVisible(const bool visible) {
    const Visibility how = visible ? Visibility::Visible : Visibility::Collapsed;

    topBar_.value().visibility(how);
    tabsBar_.value().visibility(how);
}

void Shell::setServerStatus(const ServerStatus status) {
    switch (status) {
        case ServerStatus::online:
            dot_.value().background(brushes.systemFillColorSuccess);
            dot_.value().toolTip(L"Сервер отвечает");
            break;

        case ServerStatus::offline:
            dot_.value().background(brushes.systemFillColorCritical);
            dot_.value().toolTip(L"Сервер не отвечает");
            break;

        case ServerStatus::unknown:
            dot_.value().background(brushes.textFillColorDisabled);
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
