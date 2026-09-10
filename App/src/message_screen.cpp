#include "message_screen.h"
#include "palette.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <vector>

import besedka.app;

namespace besedka::app {

using namespace wxl;
using namespace wxl::dsl;

namespace {

// Числа -- из jana (ui/components/MessageComponents.kt, MessageCard) и её
// же MessageStyle, собранного по CSS самого rsdn.org.
constexpr double kIndentStep = 10;    // MessageCard: padding start = depth * 10
constexpr double kCardGap = 4;        // MessageCard: padding top/bottom
constexpr double kCardRadius = 4;     // MessageCard: RoundedCornerShape
constexpr double kCardPaddingX = 10;  // MessageCard: padding horizontal
constexpr double kCardPaddingY = 6;   // MessageCard: padding vertical

constexpr double kListPadding = 8;   // TopicMessagesScreen: contentPadding
constexpr double kListSpacing = 4;   // TopicMessagesScreen: verticalArrangement

constexpr double kAuthorSize = 12;
constexpr double kLabelSize = 11;

/// Глубже этого отступ не растёт. У jana предела нет, но там колонка узкая
/// и ветки короткие; на форуме с перепиской в тридцать ответов подряд текст
/// иначе съезжает в столбик шириной в слово.
constexpr int kMaxSteps = 12;


}  // namespace

MessageScreen::MessageScreen() {
    messages_ = StackPanel{
        hAlign.stretch,
        spacing = kListSpacing,
        Margin{kListPadding, 8, kListPadding, kListPadding},
    };

    title_ = TextBlock{
        column = 0,
        fontSize = 18,
        FontWeight{600},
        foreground = palette.text,
        vAlign.center,
        textTrimming.characterEllipsis,
    };

    counter_ = TextBlock{
        column = 1,
        fontSize = 12,
        vAlign.center,
        Margin{16, 0, 0, 0},
        foreground = palette.textTertiary,
    };

    root_ = Grid{
        isTabStop = true,

        rowDefinitions = L"auto,*",

        Grid{
            row = 0,
            Margin{16, 20, 16, 4},
            columnDefinitions = L"*,auto",
            columnSpacing = 12,

            title_.value(),
            counter_.value(),
        },

        ScrollViewer{
            row = 1,
            content = messages_.value(),
        },
    };
}

void MessageScreen::setBaseDirectory(const std::wstring_view directory) {
    baseDirectory_ = directory;
}

void MessageScreen::setTopic(const forum::MessageInfo& topic) {
    topicId_ = topic.id;

    title_.value().text(topic.subject);
    counter_.value().text(L"читаю сообщения…");

    messages_.value().children().clear();
}

void MessageScreen::setError(const std::wstring_view said) {
    counter_.value().text({});

    messages_.value().children().clear();
    messages_.value().children().append(TextBlock{
        std::wstring(said),
        fontSize = 14,
        Margin{0, 24, 0, 0},
        textWrapping.wrap,
        foreground = palette.critical,
    });
}

void MessageScreen::show(const forum::MessagePage& page) {
    counter_.value().text(std::format(L"сообщений: {}", page.total));

    messages_.value().children().clear();

    const std::vector<int> depths = replyDepths(page);
    const std::chrono::time_zone& zone = *std::chrono::current_zone();

    for (std::size_t at = 0; at < page.items.size(); ++at)
        messages_.value().children().append(messageCard(page.items[at], depths[at], zone));
}

UIElement MessageScreen::messageCard(const forum::Message& message, const int depth,
                                     const std::chrono::time_zone& zone) {
    const double indent = kIndentStep * std::min(depth, kMaxSteps);

    auto head = Grid{
        columnDefinitions = L"auto,*,auto",
        columnSpacing = 8,

        TextBlock{
            column = 0,
            std::wstring(message.info.author.displayName),
            fontSize = kAuthorSize,
            FontWeight{600},
            foreground = palette.accent,
        },
        TextBlock{
            column = 1,
            std::wstring(message.info.subject),
            fontSize = kLabelSize,
            foreground = palette.textTertiary,
            textTrimming.characterEllipsis,
            vAlign.center,
        },
        TextBlock{
            column = 2,
            fullDate(message.info.createdOn, zone),
            fontSize = kLabelSize,
            foreground = palette.textTertiary,
            vAlign.center,
        },
    };

    // Вот оно, ради чего всё: разметка автора, разобранная нашим
    // wxl::RsdnBlock. Тело сервер не трогал -- в нём и «VD>» цитаты, и
    // «:shuffle:», и код в теге языка.
    auto body = RsdnBlock{
        isTextSelectionEnabled = true,
        Margin{0, 6, 0, 0},
    };

    // Цвета цитат -- из палитры, там и сказано, откуда они.
    body.theme(HtmlTheme{
        .quoteMargin = {12, 2, 0, 2},
        .quoteColor = {palette.quote[0], palette.quote[1], palette.quote[2]},
    });

    if (!baseDirectory_.empty()) body.baseDirectory(baseDirectory_);

    body.rsdn(message.body);

    // Рамка, а не библиотечная карточка: у jana сообщение обведено волосяной
    // линией со скруглением в четыре, без тени и без подъёма, -- а тень и
    // подъём у wxl::Card есть, и в списке из сотни сообщений подряд они
    // превращаются в рябь. Кисти при этом наши, ресурсные: жёстко заданный
    // светлый #E0E0E0 из jana в тёмной теме исчез бы.
    return Border{
        hAlign.stretch,
        Margin{indent, kCardGap, 0, kCardGap},
        Padding{kCardPaddingX, kCardPaddingY},
        CornerRadius{kCardRadius},
        background = palette.messageCard,
        borderBrush = palette.cardStroke,
        BorderThickness{1},
        StackPanel{head, body},
    };
}

}  // namespace besedka::app
