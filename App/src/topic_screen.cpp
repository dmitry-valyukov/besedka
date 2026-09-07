#include "topic_screen.h"

#include <algorithm>
#include <chrono>
#include <format>

namespace besedka::app {

using namespace wxl;
using namespace wxl::dsl;

namespace {

// Числом, а не знаком в кавычках: знак этот из области частного
// использования, и в исходнике на его месте стоит пустой прямоугольник.
constexpr wchar_t kBack = 0xE72B;      // назад
constexpr wchar_t kAnswers = 0xE8BD;   // ответов

std::wstring glyph_of(wchar_t code) { return std::wstring(1, code); }

// Числа -- из jana (ui/components/TopicCard.kt, screens/TopicListScreen.kt)
// и из шкалы Material 3, на которой она стоит.
constexpr double kCardPaddingX = 12;   // TopicCard: padding horizontal
constexpr double kCardPaddingY = 8;    // TopicCard: padding vertical
constexpr double kListPadding = 16;    // TopicListScreen: contentPadding
constexpr double kListSpacing = 8;     // TopicListScreen: verticalArrangement

constexpr double kTitleSize = 14;   // Material 3 titleSmall
constexpr double kLabelSize = 11;   // Material 3 labelSmall

constexpr double kAnswersSide = 12;    // TopicCard: ic_chat size
constexpr double kAnswersWidth = 24;   // TopicCard: фиксированная ширина числа
constexpr double kDateWidth = 80;      // TopicCard: фиксированная ширина даты

/// Дата так, как её читают: время сервера в UTC, а человек живёт в своём
/// поясе. Перевод делает current_zone() -- база часовых поясов у Windows
/// своя и обновляется вместе с ней.
///
/// Подробность зависит от давности, как в jana (ui/utils/DateUtils.kt):
/// сегодняшнее сообщение -- одно время, этого года -- день с месяцем,
/// прошлогоднее -- одна дата. Число, повторяющее сегодняшнее у каждой из
/// полусотни строк, ничего не говорит; час говорит.
std::wstring dateText(std::chrono::system_clock::time_point moment) {
    using namespace std::chrono;

    if (moment == system_clock::time_point{}) return {};

    const zoned_time local{current_zone(), floor<seconds>(moment)};
    const zoned_time now{current_zone(), floor<seconds>(system_clock::now())};

    const year_month_day then{floor<days>(local.get_local_time())};
    const year_month_day today{floor<days>(now.get_local_time())};

    if (then == today) return std::format(L"{:%H:%M}", local);

    if (then.year() == today.year()) return std::format(L"{:%d.%m %H:%M}", local);

    return std::format(L"{:%d.%m.%y}", local);
}

}  // namespace

TopicScreen::TopicScreen() {
    topics_ = StackPanel{
        hAlign.stretch,
        spacing = kListSpacing,
        Margin{kListPadding, 8, kListPadding, kListPadding},
    };

    title_ = TextBlock{
        column = 1,
        fontSize = 22,
        FontWeight{600},
        foreground = brushes.textFillColorPrimary,
        vAlign.center,
        textTrimming.characterEllipsis,
    };

    counter_ = TextBlock{
        column = 2,
        fontSize = 12,
        vAlign.center,
        Margin{16, 0, 0, 0},
        foreground = brushes.textFillColorTertiary,
    };

    root_ = Grid{
        isTabStop = true,

        rowDefinitions = L"auto,*",

        Grid{
            row = 0,
            Margin{kListPadding, 20, kListPadding, 4},
            columnDefinitions = L"auto,*,auto",
            columnSpacing = 12,

            Button{
                column = 0,
                vAlign.center,
                toolTip = L"Вернуться к форумам",
                content = FontIcon{glyph = glyph_of(kBack), fontSize = 14},
                onClick = [this](Object const&, RoutedEventArgs&) { if (onBack) onBack(); },
            },
            title_.value(),
            counter_.value(),
        },

        ScrollViewer{
            row = 1,
            content = topics_.value(),
        },
    };
}

void TopicScreen::setForum(const forum::ForumDescription& forum) {
    title_.value().text(forum.name);
    counter_.value().text(L"читаю темы…");

    topics_.value().children().clear();
}

void TopicScreen::setError(const std::wstring_view said) {
    counter_.value().text({});

    topics_.value().children().clear();
    topics_.value().children().append(TextBlock{
        std::wstring(said),
        fontSize = 14,
        Margin{0, 24, 0, 0},
        textWrapping.wrap,
        foreground = brushes.systemFillColorCritical,
    });
}

void TopicScreen::show(const forum::MessagePage& page) {
    shown_ = page;

    counter_.value().text(std::format(L"тем: {}", page.total));

    topics_.value().children().clear();

    for (const forum::Message& topic : shown_.items)
        topics_.value().children().append(topicRow(topic.info));
}

Button TopicScreen::topicRow(const forum::MessageInfo& topic) {
    const int id = topic.id;

    auto head = TextBlock{
        std::wstring(topic.subject),
        fontSize = kTitleSize,
        FontWeight{500},
        foreground = brushes.textFillColorPrimary,
        textWrapping.wrap,
        maxLines = 2,
        textTrimming.characterEllipsis,
    };

    // Нижняя строка: слева автор, справа ответы и дата, обе -- в колонках
    // постоянной ширины, чтобы правый край не плясал от строки к строке.
    // Так же сделано у jana, и по той же причине.
    auto line = Grid{
        Margin{0, 4, 0, 0},
        columnDefinitions = L"*,auto,auto,auto",

        TextBlock{
            column = 0,
            std::wstring(topic.author.displayName),
            fontSize = kLabelSize,
            foreground = brushes.textFillColorTertiary,
            textTrimming.characterEllipsis,
            vAlign.center,
        },
        FontIcon{
            column = 1,
            glyph = glyph_of(kAnswers),
            fontSize = kAnswersSide,
            vAlign.center,
            Margin{8, 0, 4, 0},
            foreground = brushes.textFillColorTertiary,
            toolTip = L"Ответов в теме",
        },
        TextBlock{
            column = 2,
            std::format(L"{}", topic.answersCount),
            fontSize = kLabelSize,
            width = kAnswersWidth,
            foreground = brushes.textFillColorTertiary,
            vAlign.center,
        },
        TextBlock{
            column = 3,
            dateText(topic.createdOn),
            fontSize = kLabelSize,
            width = kDateWidth,
            textAlignment.end,
            foreground = brushes.textFillColorTertiary,
            vAlign.center,
        },
    };

    return Button{
        hAlign.stretch,
        horizontalContentAlignment = HorizontalAlignment::Stretch,
        Padding{kCardPaddingX, kCardPaddingY},
        background = brushes.cardBackgroundFillColorDefault,
        borderBrush = brushes.cardStrokeColorDefault,
        BorderThickness{1},
        CornerRadius{4},
        automationName = std::wstring(topic.subject),
        content = StackPanel{head, line},
        onClick =
            [this, id](Object const&, RoutedEventArgs&) {
                if (!onOpen) return;

                const auto found = std::ranges::find_if(
                    shown_.items,
                    [id](const forum::Message& message) { return message.info.id == id; });

                if (found != shown_.items.end()) onOpen(found->info);
            },
    };
}

}  // namespace besedka::app
