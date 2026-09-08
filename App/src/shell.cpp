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

// Порог двухстраничного показа -- в логических пикселях, а не в физических:
// вопрос, на который он отвечает, это «хватает ли места на две страницы
// текста», а текст меряется в логических. Разбор -- в docs/decisions.md.
constexpr double kTwoPageWidth = 1200;

// Граница между страницами. Шесть логических -- полоска, которую видно и в
// которую попадают мышью, не отнимая заметной ширины ни у одной из страниц.
constexpr double kSplitterWidth = 6;

// Пределы доли: за ними у одной из страниц остаётся полоса, в которой не
// помещается ни строка текста, ни заголовок с кнопкой возврата.
constexpr double kSplitLower = 0.2;
constexpr double kSplitUpper = 0.8;

double clamped(double value, double lower, double upper) {
    return value < lower ? lower : (value > upper ? upper : value);
}

/// Колонка шириной в долю. Star, а не пиксели: доли складываются в единицу, и
/// сетка сама раздаёт им ширину, сколько бы её ни было.
ColumnDefinition starColumn(double share) {
    ColumnDefinition column;

    column.width(GridLength{share, GridUnitType::Star});

    return column;
}

ColumnDefinition pixelColumn(double width) {
    ColumnDefinition column;

    column.width(GridLength{width, GridUnitType::Pixel});

    return column;
}

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
        background = brushes.Text.FillColor.Disabled,
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
                foreground = brushes.Text.FillColor.Primary,
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

    // ---- граница половин ----
    //
    // Обычный Border, а не контрол библиотеки: с тех пор как в проекции есть
    // захват указателя, вся тяга -- это три обработчика ниже. Заведётся
    // второе приложение, которому нужна граница, -- переедет в wxl вместе с
    // видом и курсором; до тех пор это тридцать строк на месте, а не новая
    // машинерия в библиотеке.
    splitter_ = Border{
        column = 1,
        background = brushes.DividerStrokeColorDefault,
        visibility = Visibility::Collapsed,
        toolTip = L"Граница страниц: потяните, чтобы изменить ширину",

        onPointerPressed =
            [this](Object const&, PointerRoutedEventArgs& args) {
                // Захват: без него полоска в шесть пикселей теряет мышь на
                // первом же быстром движении, и тяга обрывается на середине.
                // Берётся у своего же поля, а не у отправителя: полоска --
                // часть каркаса и живёт столько же, сколько он.
                if (!splitter_.value().capturePointer(args.pointer())) return;

                dragging_ = true;
            },

        onPointerMoved =
            [this](Object const&, PointerRoutedEventArgs& args) {
                if (!dragging_) return;

                if (width_ <= 0) return;

                // Доля считается от указателя, а не складыванием сдвигов:
                // накопленная сумма разъезжается с рукой на каждом
                // подрезанном пределом движении.
                splitFraction(args.getCurrentPoint(host_.value()).position().x / width_);

            },

        onPointerReleased =
            [this](Object const&, PointerRoutedEventArgs& args) {
                if (!dragging_) return;

                splitter_.value().releasePointerCapture(args.pointer());

                dragging_ = false;

                if (onSplitChanged) onSplitChanged(fraction_);
            },

        // Захват пропадает и сам: окно потеряло активацию, касание отменили.
        // Тяга, которая ждала бы только отпускания, осталась бы зажатой.
        onPointerCaptureLost =
            [this](Object const&, PointerRoutedEventArgs&) {
                if (!std::exchange(dragging_, false)) return;

                if (onSplitChanged) onSplitChanged(fraction_);
            },
    };

    host_ = Grid{
        row = 1,
        splitter_.value(),
    };

    root_ = Grid{

        rowDefinitions = L"auto,*,auto,auto",

        topBar_.value(),
        host_.value(),
        tabsBar_.value(),
        statusBar_.value(),
    };
}

void Shell::showSplash(const UIElement& page) {
    splash_ = true;
    stack_.clear();
    stack_.push_back({page, {}});

    relayout();
}

void Shell::showRoot(Page page) {
    splash_ = false;
    stack_.clear();
    stack_.push_back(std::move(page));

    relayout();
}

void Shell::open(Page page) {
    splash_ = false;
    stack_.push_back(std::move(page));

    relayout();
}

void Shell::back() {
    // Нижняя страница -- верхний уровень вкладки, и снимать её некуда.
    if (stack_.size() < 2) return;

    stack_.pop_back();

    relayout();
}

void Shell::splitFraction(const double value) {
    const double wanted = clamped(value, kSplitLower, kSplitUpper);

    if (wanted == fraction_) return;

    fraction_ = wanted;

    // Не пересобирать раскладку целиком: при тяге это было бы снятие и
    // возвращение обеих страниц на каждое движение мыши. Меняются две
    // ширины, дети остаются на местах.
    const Collection<ColumnDefinition> columns = host_.value().columnDefinitions();

    if (columns.size() != 3) return;

    columns[0].width(GridLength{fraction_, GridUnitType::Star});
    columns[2].width(GridLength{1 - fraction_, GridUnitType::Star});
}

bool Shell::isWide() const { return width_ > kTwoPageWidth; }

void Shell::setWidth(const double logical) {
    if (logical == width_) return;

    const bool was = isWide();

    width_ = logical;

    // Пересборка только на смене способа показа: тянущий рамку окна шлёт
    // новый размер на каждый пиксель, а перекладывать страницы на каждый
    // пиксель значит снимать их с дерева и возвращать сотни раз подряд.
    if (isWide() != was) relayout();
}

void Shell::relayout() {
    wide_ = isWide();

    // Две страницы -- когда широко И есть что показать второй. Одной страницы
    // в стопке хватает на левую половину, а правая тогда прозрачна: сквозь
    // неё виден задник окна, и своего она не рисует ничего.
    const bool twoPages = wide_ && stack_.size() >= 2;

    const Collection<ColumnDefinition> columns = host_.value().columnDefinitions();
    const Collection<UIElement> children = host_.value().children();

    columns.clear();

    if (wide_) {
        columns.append(starColumn(fraction_));
        columns.append(pixelColumn(kSplitterWidth));
        columns.append(starColumn(1 - fraction_));
    } else {
        columns.append(starColumn(1));
    }

    children.clear();

    if (!stack_.empty()) {
        const Page& left = twoPages ? stack_[stack_.size() - 2] : stack_.back();

        Grid::setColumn(left.root.try_as<FrameworkElement>(), 0);
        children.append(left.root);

        // Слева выбирают одиночным щелчком -- это страница, по которой ходят.
        if (left.placed) left.placed(false);
    }

    // Граница показывается только когда ей есть что делить: полоска между
    // страницей и пустотой -- шов, за которым ничего нет.
    splitter_.value().visibility(twoPages ? Visibility::Visible : Visibility::Collapsed);
    children.append(splitter_.value());

    if (twoPages) {
        const Page& right = stack_.back();

        Grid::setColumn(right.root.try_as<FrameworkElement>(), 2);
        children.append(right.root);

        // Справа читают, и одиночный щелчок принадлежит содержимому.
        if (right.placed) right.placed(true);
    }

    updateChrome();
}

void Shell::updateChrome() {
    // Заставка -- без панелей вовсе: жать «обновить» и переключать вкладки,
    // пока не прочитан первый ответ, нечего.
    //
    // Дальше панели видны, пока слева стоит верхний уровень вкладки. В
    // одностраничном показе это стопка из одной страницы; в двухстраничном
    // слева стоит предпоследняя, значит из одной или двух.
    const bool visible = !splash_ && stack_.size() <= (wide_ ? 2u : 1u);

    const Visibility how = visible ? Visibility::Visible : Visibility::Collapsed;

    topBar_.value().visibility(how);
    tabsBar_.value().visibility(how);

    // Полоса состояния уходит только на заставке: там она повторяла бы
    // своими словами то, что уже сказано на карточке, и отрезала бы у
    // картинки полосу снизу.
    statusBar_.value().visibility(splash_ ? Visibility::Collapsed : Visibility::Visible);
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
