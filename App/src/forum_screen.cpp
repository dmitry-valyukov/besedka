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
constexpr std::wstring_view kStar = L"";   // форум из первых
constexpr std::wstring_view kLock = L"";   // только для чтения

/// Служебные форумы притушены -- как в jana, где у них alpha 0.4. Они не
/// про разговоры, но и прятать их незачем: там лежат объявления.
constexpr double kServiceOpacity = 0.45;

}  // namespace

ForumScreen::ForumScreen() {
    groups_ = StackPanel{Margin{32, 8, 32, 32}};

    counter_ = TextBlock{
        column = 1,
        fontSize = 13,
        vAlign.center,
        foreground = brushes.textFillColorTertiary,
    };

    root_ = Grid{
        isTabStop = true,
        background = brushes.solidBackgroundFillColorBase,
        rowDefinitions = L"auto,*",

        Grid{
            row = 0,
            Margin{32, 28, 32, 8},
            columnDefinitions = L"auto,*,auto",
            columnSpacing = 16,

            TextBlock{
                column = 0,
                L"Форумы",
                fontSize = 28,
                FontWeight{600},
                foreground = brushes.textFillColorPrimary,
                vAlign.center,
            },
            counter_.value(),
            Button{
                column = 2,
                L"Обновить",
                vAlign.center,
                onClick = [this](Object const&, RoutedEventArgs&) { if (onRefresh) onRefresh(); },
            },
        },

        ScrollViewer{
            row = 1,
            content = groups_.value(),
        },
    };
}

void ForumScreen::show(const std::vector<forum::ForumDescription>& forums) {
    shown_ = forums;

    groups_.value().children().clear();

    counter_.value().text(std::format(L"{} на сервере", shown_.size()));

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
            Margin{0, 4},
            // Заголовок группы -- имя и счёт: свёрнутая группа иначе не
            // говорит о себе ничего, кроме названия. Текстовым блоком, а не
            // строкой: заголовок Expander -- это Object, и блок даёт заодно
            // управление кеглем.
            header = TextBlock{
                std::format(L"{}    ({})", group->name, count),
                fontSize = 14,
                FontWeight{600},
            },
            content = inside,
        });
    }
}

Button ForumScreen::forumRow(const forum::ForumDescription& forum) {
    const int id = forum.id;

    auto title = TextBlock{
        column = 1,
        std::wstring(forum.name),
        fontSize = 15,
        foreground = brushes.textFillColorPrimary,
        textTrimming.characterEllipsis,
        vAlign.center,
    };

    // Форум о самом сайте выделен -- как в jana, где он идёт цветом темы и
    // полужирным. Ставится после постройки, а не тернарником внутри неё:
    // ресурсная кисть -- свой тип на каждый ресурс, и две разные в одном
    // тернарнике не сходятся.
    if (forum.isSiteSubject) {
        title.fontWeight(FontWeight{600});
        title.foreground(brushes.accentTextFillColorPrimary);
    }

    // Значки идут за названием и только когда есть что сказать: строка «в
    // первых» и «только чтение» -- это две редкие пометки, а не два поля,
    // которые всегда на месте.
    auto marks = StackPanel{
        column = 2,
        orientation = Orientation::Horizontal,
        spacing = 6,
        vAlign.center,
        Margin{8, 0, 0, 0},
    };

    if (forum.isInTop)
        marks.children().append(FontIcon{
            glyph = kStar,
            fontSize = 12,
            toolTip = L"Форум из первых",
        });

    if (!forum.isWriteAllowed)
        marks.children().append(FontIcon{
            glyph = kLock,
            fontSize = 12,
            toolTip = L"Только для чтения",
        });

    // Код форума -- в бейдже: он же кусок адреса на сайте (rsdn.org/forum/cpp),
    // и по нему форум узнают быстрее, чем по названию.
    auto code = Border{
        column = 0,
        vAlign.center,
        Margin{0, 0, 12, 0},
        Padding{6, 2},
        CornerRadius{4},
        background = brushes.layerFillColorDefault,
        borderBrush = brushes.cardStrokeColorDefault,
        BorderThickness{1},
        // Ребёнок, а не содержимое: Border несёт ровно одного, и свойство у
        // него так и называется -- child.
        TextBlock{
            wxl::text::ascii_upper(std::wstring_view(forum.code)),
            fontSize = 11,
            FontWeight{700},
            foreground = brushes.textFillColorSecondary,
        },
    };

    auto lines = StackPanel{
        Grid{
            columnDefinitions = L"auto,*,auto",
            code,
            title,
            marks,
        },
    };

    if (!forum.description.empty())
        lines.children().append(TextBlock{
            std::wstring(forum.description),
            fontSize = 12,
            Margin{52, 2, 0, 0},
            foreground = brushes.textFillColorTertiary,
            textTrimming.characterEllipsis,
        });

    auto row = Button{
        hAlign.stretch,
        // Содержимое кнопки по умолчанию стоит по центру -- для строки это
        // значит текст посреди пустоты. Растянуть его надо явно, и это
        // horizontalContentAlignment, а не hAlign: тот про саму кнопку.
        horizontalContentAlignment = HorizontalAlignment::Stretch,
        Margin{0, 2},
        Padding{12, 8},
        background = brushes.subtleFillColorTransparent,
        BorderThickness{0},
        content = lines,
        onClick =
            [this, id](Object const&, RoutedEventArgs&) {
                if (!onOpen) return;

                const auto found = std::ranges::find_if(
                    shown_, [id](const forum::ForumDescription& forum) { return forum.id == id; });

                if (found != shown_.end()) onOpen(*found);
            },
    };

    if (forum.isService) row.opacity(kServiceOpacity);

    return row;
}

}  // namespace besedka::app
