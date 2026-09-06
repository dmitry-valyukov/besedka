#include "topic_screen.h"

#include <algorithm>
#include <chrono>
#include <format>

namespace besedka::app {

using namespace wxl;
using namespace wxl::dsl;

namespace {

// Стрелка «назад» из Segoe Fluent Icons: шрифт стоит в системе, и стрелка в
// нём та же, что во всех остальных приложениях Windows.
constexpr std::wstring_view kBack = L"";

/// Дата так, как её читают: время сервера в UTC, а человек живёт в своём
/// поясе. Перевод делает current_zone() -- база часовых поясов у Windows
/// своя и обновляется вместе с ней.
std::wstring dateText(std::chrono::system_clock::time_point moment) {
    if (moment == std::chrono::system_clock::time_point{}) return {};

    const std::chrono::zoned_time local{std::chrono::current_zone(),
                                        std::chrono::floor<std::chrono::seconds>(moment)};

    return std::format(L"{:%d.%m.%Y %H:%M}", local);
}

}  // namespace

TopicScreen::TopicScreen() {
    topics_ = StackPanel{Margin{32, 8, 32, 32}};

    title_ = TextBlock{
        column = 1,
        fontSize = 24,
        FontWeight{600},
        foreground = brushes.textFillColorPrimary,
        vAlign.center,
        textTrimming.characterEllipsis,
    };

    counter_ = TextBlock{
        column = 2,
        fontSize = 13,
        vAlign.center,
        Margin{16, 0, 0, 0},
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
            columnSpacing = 12,

            Button{
                column = 0,
                vAlign.center,
                toolTip = L"Вернуться к форумам",
                content = FontIcon{glyph = kBack, fontSize = 14},
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
        fontSize = 15,
        FontWeight{600},
        foreground = brushes.textFillColorPrimary,
        textWrapping.wrap,
        maxLines = 2,
        textTrimming.characterEllipsis,
    };

    auto line = Grid{
        Margin{0, 6, 0, 0},
        columnDefinitions = L"*,auto,auto",
        columnSpacing = 12,

        TextBlock{
            column = 0,
            std::wstring(topic.author.displayName),
            fontSize = 12,
            foreground = brushes.textFillColorTertiary,
            textTrimming.characterEllipsis,
            vAlign.center,
        },
        // Словом, а не значком: у jana тут иконка с числом, но по-русски
        // «ответов: 295» читается с одного взгляда и не требует догадки о
        // том, что означает картинка.
        TextBlock{
            column = 1,
            std::format(L"ответов: {}", topic.answersCount),
            fontSize = 12,
            foreground = brushes.textFillColorTertiary,
            vAlign.center,
        },
        TextBlock{
            column = 2,
            dateText(topic.createdOn),
            fontSize = 12,
            foreground = brushes.textFillColorTertiary,
            vAlign.center,
        },
    };

    return Button{
        hAlign.stretch,
        horizontalContentAlignment = HorizontalAlignment::Stretch,
        Margin{0, 2},
        Padding{12, 10},
        background = brushes.cardBackgroundFillColorDefault,
        borderBrush = brushes.cardStrokeColorDefault,
        BorderThickness{1},
        CornerRadius{6},
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
