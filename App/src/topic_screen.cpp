#include "topic_screen.h"

#include <algorithm>
#include <chrono>
#include <format>

import besedka.app;

namespace besedka::app {

using namespace wxl;
using namespace wxl::dsl;

namespace {

// Числом, а не знаком в кавычках: знак этот из области частного
// использования, и в исходнике на его месте стоит пустой прямоугольник.
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

}  // namespace

TopicScreen::TopicScreen() {
    topics_ = StackPanel{
        hAlign.stretch,
        spacing = kListSpacing,
        Margin{kListPadding, 8, kListPadding, kListPadding},
    };

    title_ = TextBlock{
        column = 0,
        fontSize = 22,
        FontWeight{600},
        foreground = brushes.Text.FillColor.Primary,
        vAlign.center,
        textTrimming.characterEllipsis,
    };

    counter_ = TextBlock{
        column = 1,
        fontSize = 12,
        vAlign.center,
        Margin{16, 0, 0, 0},
        foreground = brushes.Text.FillColor.Tertiary,
    };

    root_ = Grid{
        isTabStop = true,

        rowDefinitions = L"auto,*",

        Grid{
            row = 0,
            Margin{kListPadding, 20, kListPadding, 4},
            columnDefinitions = L"*,auto",
            columnSpacing = 12,

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
    forumId_ = forum.id;

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
        foreground = brushes.SystemFillColor.Critical,
    });
}

void TopicScreen::show(const forum::MessagePage& page) {
    shown_ = page;

    counter_.value().text(std::format(L"тем: {}", page.total));

    topics_.value().children().clear();

    // «Сейчас» и пояс -- один раз на список, а не на строку: полусотне строк
    // незачем спрашивать часы полсотни раз.
    const auto now = std::chrono::system_clock::now();
    const std::chrono::time_zone& zone = *std::chrono::current_zone();

    for (const forum::Message& topic : shown_.items)
        topics_.value().children().append(topicRow(topic.info, now, zone));
}

Button TopicScreen::topicRow(const forum::MessageInfo& topic,
                             const std::chrono::system_clock::time_point now,
                             const std::chrono::time_zone& zone) {
    const int id = topic.id;

    auto head = TextBlock{
        std::wstring(topic.subject),
        fontSize = kTitleSize,
        FontWeight{500},
        foreground = brushes.Text.FillColor.Primary,
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
            foreground = brushes.Text.FillColor.Tertiary,
            textTrimming.characterEllipsis,
            vAlign.center,
        },
        FontIcon{
            column = 1,
            glyph = glyph_of(kAnswers),
            fontSize = kAnswersSide,
            vAlign.center,
            Margin{8, 0, 4, 0},
            foreground = brushes.Text.FillColor.Tertiary,
            toolTip = L"Ответов в теме",
        },
        TextBlock{
            column = 2,
            std::format(L"{}", topic.answersCount),
            fontSize = kLabelSize,
            width = kAnswersWidth,
            foreground = brushes.Text.FillColor.Tertiary,
            vAlign.center,
        },
        TextBlock{
            column = 3,
            relativeDate(topic.createdOn, now, zone),
            fontSize = kLabelSize,
            width = kDateWidth,
            textAlignment.end,
            foreground = brushes.Text.FillColor.Tertiary,
            vAlign.center,
        },
    };

    return Button{
        hAlign.stretch,
        horizontalContentAlignment = HorizontalAlignment::Stretch,
        Padding{kCardPaddingX, kCardPaddingY},
        background = brushes.Card.BackgroundFillColor.Default,
        borderBrush = brushes.Card.StrokeColorDefault,
        BorderThickness{1},
        CornerRadius{4},
        automationName = std::wstring(topic.subject),
        content = StackPanel{head, line},
        // Слева по теме щёлкают один раз, справа -- два: там одиночный щелчок
        // принадлежит содержимому. Взаимоисключающе, потому что XAML шлёт
        // DoubleTapped вслед за Click.
        onClick =
            [this, id](Object const&, RoutedEventArgs&) {
                if (!secondary_) openById(id);
            },
        onDoubleTapped =
            [this, id](Object const&, DoubleTappedRoutedEventArgs&) {
                if (secondary_) openById(id);
            },
    };
}

void TopicScreen::openById(const int32_t id) {
    if (!onOpen) return;

    // Ищется заново, а не запоминается в замыкании: список пересобирается
    // целиком при каждом показе.
    const auto found = std::ranges::find_if(
        shown_.items, [id](const forum::Message& message) { return message.info.id == id; });

    if (found != shown_.items.end()) onOpen(found->info);
}

}  // namespace besedka::app
