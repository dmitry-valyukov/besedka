#include "forum_screen.h"

#include <algorithm>
#include <format>

// Импорт последним, после всех обычных заголовков.
import wxl.text;

namespace besedka::app {

using namespace wxl;
using namespace wxl::dsl;

namespace {

// Значки из шрифта, а не картинками: Segoe Fluent Icons стоит в системе, и
// звезда с замком в нём те же самые, что во всех остальных приложениях
// Windows. Картинки пришлось бы носить с собой и рисовать под обе темы.
//
// У jana на месте первого значка своя картинка на каждый форум
// (ui/utils/ForumIconMapper.kt); своих картинок у нас нет, и один общий
// значок беседы честнее, чем чужой набор.
//
// Числом, а не знаком в кавычках: знаки эти из области частного
// использования, и в исходнике на их месте стоит пустой прямоугольник --
// по нему не видно ни что это, ни уцелел ли он при правке файла.
constexpr wchar_t kForum = 0xE8F2;   // беседа
constexpr wchar_t kStar = 0xE735;    // форум из первых
constexpr wchar_t kLock = 0xE72E;    // только для чтения

/// Знак шрифта строкой -- в таком виде его берёт FontIcon.
std::wstring glyph_of(wchar_t code) { return std::wstring(1, code); }

// Числа отсюда и ниже -- из самой jana (ui/components/ForumCard.kt,
// ForumGroupHeader.kt, screens/ForumListScreen.kt) и из шкалы Material 3,
// на которой она стоит. Единица у Compose и у WinUI одна и та же -- 1/96
// дюйма, -- так что dp и sp переносятся числом, без пересчёта.
constexpr double kRowPaddingX = 16;   // ForumCard: padding horizontal
constexpr double kRowPaddingY = 12;   // ForumCard: padding vertical
constexpr double kIconSide = 24;      // ForumCard: Icon size
constexpr double kIconGap = 16;       // ForumCard: Spacer width
constexpr double kStarSide = 16;      // ForumCard: "TOP" icon
constexpr double kLockSide = 14;      // ForumCard: "Read Only" icon
constexpr double kMarkGap = 6;        // ForumCard: horizontalArrangement spacedBy

constexpr double kTitleSize = 16;   // Material 3 titleMedium
constexpr double kGroupSize = 14;   // Material 3 titleSmall
constexpr double kBodySize = 12;    // Material 3 bodySmall
constexpr double kLabelSize = 11;   // Material 3 labelSmall

constexpr double kGroupPaddingX = 12;   // ForumGroupHeader: padding horizontal
constexpr double kGroupPaddingY = 10;   // ForumGroupHeader: padding vertical

constexpr double kListPadding = 16;   // ForumListScreen: contentPadding
constexpr double kListSpacing = 4;    // ForumListScreen: verticalArrangement

/// Служебные форумы притушены -- ровно как в jana, где у них alpha 0.4.
constexpr double kServiceOpacity = 0.4;

/// Верхняя граница показываемого ограничения оценок: то же условие, что у
/// jana, -- «R:32656» на каждой второй строке значит только, что предела
/// нет.
constexpr int kRateLimitShown = 30000;

}  // namespace

ForumScreen::ForumScreen() {
    groups_ = StackPanel{
        hAlign.stretch,
        spacing = kListSpacing,
        Margin{kListPadding, 8, kListPadding, kListPadding},
    };

    // Своего заголовка у витрины нет: имя, состояние сервера и обновление
    // живут на верхней панели каркаса -- как в jana, где ForumListScreen
    // отдаёт всё это Scaffold'у.
    root_ = Grid{
        isTabStop = true,

        ScrollViewer{
            content = groups_.value(),
        },
    };
}

void ForumScreen::show(const std::vector<forum::ForumDescription>& forums) {
    shown_ = forums;

    groups_.value().children().clear();

    // Группы в том порядке, который назначил им сервер: sortOrder для того и
    // прислан. Внутри группы порядок оставлен как есть -- сервер отдаёт
    // форумы по возрастанию идентификатора, то есть в порядке появления, и
    // старожилы форума знают их в этом порядке.
    std::vector<const forum::ForumGroup*> order;

    for (const forum::ForumDescription& forum : shown_) {
        const auto known = std::ranges::find_if(order, [&forum](const forum::ForumGroup* group) {
            return group->id == forum.group.id;
        });

        if (known == order.end()) order.push_back(&forum.group);
    }

    std::ranges::stable_sort(order, [](const forum::ForumGroup* left, const forum::ForumGroup* right) {
        return left->sortOrder < right->sortOrder;
    });

    for (const forum::ForumGroup* group : order) {
        auto inside = StackPanel{hAlign.stretch, Margin{0, 4, 0, 8}};

        int count = 0;

        for (const forum::ForumDescription& forum : shown_) {
            if (forum.group.id != group->id) continue;

            inside.children().append(forumRow(forum));
            ++count;
        }

        groups_.value().children().append(Expander{
            hAlign.stretch,
            // И содержимому тоже: у Expander оно по умолчанию стоит по
            // центру, отчего строки форумов собираются в колонку посреди
            // пустой ширины.
            horizontalContentAlignment = HorizontalAlignment::Stretch,
            Padding{kGroupPaddingX, kGroupPaddingY},
            // Заголовок группы -- имя и счёт: свёрнутая группа иначе не
            // говорит о себе ничего, кроме названия. Текстовым блоком, а не
            // строкой: заголовок Expander -- это Object, и блок даёт заодно
            // управление кеглем.
            header = TextBlock{
                std::format(L"{}    ({})", group->name, count),
                fontSize = kGroupSize,
                FontWeight{500},
            },
            content = inside,
        });
    }
}

Button ForumScreen::forumRow(const forum::ForumDescription& forum) {
    const int id = forum.id;
    const bool primary = forum.isSiteSubject;

    auto icon = FontIcon{
        column = 0,
        glyph = glyph_of(kForum),
        fontSize = kIconSide,
        vAlign.top,
        Margin{0, 2, kIconGap, 0},
        foreground = primary ? Brush(brushes.Accent.TextFillColor.Primary)
                             : Brush(brushes.Text.FillColor.Tertiary),
    };

    auto title = TextBlock{
        column = 0,
        std::wstring(forum.name),
        fontSize = kTitleSize,
        FontWeight{static_cast<std::uint16_t>(primary ? 700 : 500)},
        foreground = primary ? Brush(brushes.Accent.TextFillColor.Primary)
                             : Brush(brushes.Text.FillColor.Primary),
        textTrimming.characterEllipsis,
        vAlign.center,
    };

    // Пометки идут сразу за названием, а не по правому краю: у jana они в
    // одной строке с ним, через spacedBy(6), и от названия не отрываются.
    auto marks = StackPanel{
        column = 1,
        orientation = Orientation::Horizontal,
        spacing = kMarkGap,
        vAlign.center,
        Margin{kMarkGap, 0, 0, 0},
    };

    if (forum.isInTop)
        marks.children().append(FontIcon{
            glyph = glyph_of(kStar),
            fontSize = kStarSide,
            foreground = brushes.Accent.TextFillColor.Secondary,
            toolTip = L"Форум из первых",
        });

    if (!forum.isWriteAllowed)
        marks.children().append(FontIcon{
            glyph = glyph_of(kLock),
            fontSize = kLockSide,
            foreground = brushes.SystemFillColor.Critical,
            toolTip = L"Только для чтения",
        });

    // Нижняя строка: код форума в бейдже и, если оценки ограничены, предел.
    // Число тем у jana тут же справа, а у нас его нет -- оно приезжает с
    // синхронизацией, которой пока нет.
    auto badges = StackPanel{
        orientation = Orientation::Horizontal,
        vAlign.center,
        Margin{0, 8, 0, 0},

        Border{
            vAlign.center,
            Padding{4, 1},
            CornerRadius{4},
            background = brushes.Layer.FillColorDefault,
            borderBrush = brushes.Card.StrokeColorDefault,
            BorderThickness{1},
            // Ребёнок, а не содержимое: Border несёт ровно одного, и
            // свойство у него так и называется -- child.
            TextBlock{
                wxl::text::ascii_upper(std::wstring_view(forum.code)),
                fontSize = kLabelSize,
                FontWeight{900},
                foreground = brushes.Text.FillColor.Secondary,
            },
        },
    };

    if (forum.isRated && forum.rateLimit > 0 && forum.rateLimit <= kRateLimitShown)
        badges.children().append(TextBlock{
            std::format(L"R:{}", forum.rateLimit),
            fontSize = kLabelSize,
            vAlign.center,
            Margin{12, 0, 0, 0},
            foreground = brushes.Text.FillColor.Tertiary,
        });

    auto lines = StackPanel{
        column = 1,
        Grid{
            // Название по своей ширине, пометки сразу за ним: у jana это
            // `weight(1f, fill = false)`, то есть «не больше строки, но и
            // не шире, чем нужно». Звезда, ушедшая к правому краю, читалась
            // бы отдельной колонкой, а она -- часть названия.
            columnDefinitions = L"auto,*",
            title,
            marks,
        },
    };

    if (!forum.description.empty())
        lines.children().append(TextBlock{
            std::wstring(forum.description),
            fontSize = kBodySize,
            Margin{0, 2, 0, 0},
            foreground = brushes.Text.FillColor.Tertiary,
            textTrimming.characterEllipsis,
        });

    lines.children().append(badges);

    auto row = Button{
        hAlign.stretch,
        // Содержимое кнопки по умолчанию стоит по центру -- для строки это
        // значит текст посреди пустоты. Растянуть его надо явно, и это
        // horizontalContentAlignment, а не hAlign: тот про саму кнопку.
        horizontalContentAlignment = HorizontalAlignment::Stretch,
        Padding{kRowPaddingX, kRowPaddingY},
        background = brushes.SubtleFillColor.Transparent,
        BorderThickness{0},
        // Имя для доступности: содержимое строки -- панель, а не строка, и
        // без этого экранный диктор скажет «кнопка» и замолчит.
        automationName = std::wstring(forum.name),
        content = Grid{
            columnDefinitions = L"auto,*",
            icon,
            lines,
        },
        // Оба щелчка ведут в одно место, а выбирает между ними то, где стоит
        // страница: слева по строке щёлкают один раз, справа -- два, потому
        // что там одиночный принадлежит содержимому. Развести их надо именно
        // так, взаимоисключающе: XAML шлёт DoubleTapped вслед за Click, и
        // страница, слушающая оба разом, открыла бы форум дважды.
        onClick =
            [this, id](Object const&, RoutedEventArgs&) {
                if (!secondary_) openById(id);
            },
        onDoubleTapped =
            [this, id](Object const&, DoubleTappedRoutedEventArgs&) {
                if (secondary_) openById(id);
            },
    };

    if (forum.isService) row.opacity(kServiceOpacity);

    return row;
}

void ForumScreen::openById(const int32_t id) {
    if (!onOpen) return;

    // Ищется заново, а не запоминается в замыкании: витрина пересобирается
    // целиком при каждом показе, и описание, скопированное в обработчик,
    // пережило бы тот список, из которого взято.
    const auto found = std::ranges::find_if(
        shown_, [id](const forum::ForumDescription& forum) { return forum.id == id; });

    if (found != shown_.end()) onOpen(*found);
}

}  // namespace besedka::app
