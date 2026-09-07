#include "message_screen.h"

#include <chrono>
#include <format>
#include <map>

namespace besedka::app {

using namespace wxl;
using namespace wxl::dsl;

namespace {

// Числом, а не знаком в кавычках: знак этот из области частного
// использования, и в исходнике на его месте стоит пустой прямоугольник.
constexpr wchar_t kBack = 0xE72B;

std::wstring glyph_of(wchar_t code) { return std::wstring(1, code); }

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

/// Цвета цитат по уровням -- те самые, что у RSDN в CSS и у jana в
/// MessageStyle: от тёмно-зелёного к светлому. По ним в переписке видно не
/// только что это цитата, но и чья.
constexpr std::uint32_t kQuote1 = 0xFF137900;
constexpr std::uint32_t kQuote2 = 0xFF74B967;
constexpr std::uint32_t kQuote3 = 0xFF9FD095;

std::wstring dateText(std::chrono::system_clock::time_point moment) {
    if (moment == std::chrono::system_clock::time_point{}) return {};

    const std::chrono::zoned_time local{std::chrono::current_zone(),
                                        std::chrono::floor<std::chrono::seconds>(moment)};

    return std::format(L"{:%d.%m.%Y %H:%M}", local);
}

}  // namespace

MessageScreen::MessageScreen() {
    messages_ = StackPanel{
        hAlign.stretch,
        spacing = kListSpacing,
        Margin{kListPadding, 8, kListPadding, kListPadding},
    };

    title_ = TextBlock{
        column = 1,
        fontSize = 18,
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
            Margin{16, 20, 16, 4},
            columnDefinitions = L"auto,*,auto",
            columnSpacing = 12,

            Button{
                column = 0,
                vAlign.center,
                toolTip = L"Вернуться к темам",
                content = FontIcon{glyph = glyph_of(kBack), fontSize = 14},
                onClick = [this](Object const&, RoutedEventArgs&) { if (onBack) onBack(); },
            },
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
        foreground = brushes.systemFillColorCritical,
    });
}

void MessageScreen::show(const forum::MessagePage& page) {
    counter_.value().text(std::format(L"сообщений: {}", page.total));

    messages_.value().children().clear();

    // Глубина ответа -- длина цепочки родителей внутри страницы. Сервер
    // отдаёт сообщения в порядке появления, поэтому родитель уже посчитан к
    // тому времени, как доходит очередь до ребёнка, и второй проход не
    // нужен.
    std::map<int, int> depthOf;

    for (const forum::Message& message : page.items) {
        const auto parent = depthOf.find(message.info.parentId);
        const int depth = parent == depthOf.end() ? 0 : parent->second + 1;

        depthOf.emplace(message.info.id, depth);

        messages_.value().children().append(messageCard(message, depth));
    }
}

UIElement MessageScreen::messageCard(const forum::Message& message, const int depth) {
    const double indent = kIndentStep * std::min(depth, kMaxSteps);

    auto head = Grid{
        columnDefinitions = L"auto,*,auto",
        columnSpacing = 8,

        TextBlock{
            column = 0,
            std::wstring(message.info.author.displayName),
            fontSize = kAuthorSize,
            FontWeight{600},
            foreground = brushes.accentTextFillColorPrimary,
        },
        TextBlock{
            column = 1,
            std::wstring(message.info.subject),
            fontSize = kLabelSize,
            foreground = brushes.textFillColorTertiary,
            textTrimming.characterEllipsis,
            vAlign.center,
        },
        TextBlock{
            column = 2,
            dateText(message.info.createdOn),
            fontSize = kLabelSize,
            foreground = brushes.textFillColorTertiary,
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

    // Вид цитат -- как на самом RSDN: три оттенка зелёного по уровням.
    // Числа взяты из MessageStyle у jana, а туда -- из CSS сайта.
    body.theme(HtmlTheme{
        .quoteMargin = {12, 2, 0, 2},
        .quoteColor = {ARGB{kQuote1}, ARGB{kQuote2}, ARGB{kQuote3}},
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
        background = brushes.cardBackgroundFillColorDefault,
        borderBrush = brushes.cardStrokeColorDefault,
        BorderThickness{1},
        StackPanel{head, body},
    };
}

}  // namespace besedka::app
