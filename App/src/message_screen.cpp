#include "message_screen.h"

#include <chrono>
#include <format>
#include <map>

namespace besedka::app {

using namespace wxl;
using namespace wxl::dsl;

namespace {

// Стрелка «назад» из Segoe Fluent Icons. Кодовой точкой, а не самим знаком:
// он из области частного использования, и в исходнике на его месте видно
// пустой прямоугольник -- то есть ровно то, что увидит и читающий код.
constexpr std::wstring_view kBack = L"";

/// Отступ на одну ступень ответа. Глубже пятой ступени не отступаем: на
/// узкой теме в двадцать ответов подряд текст иначе съезжает в столбик
/// шириной в слово.
constexpr double kStep = 24.0;
constexpr int kMaxSteps = 5;

std::wstring dateText(std::chrono::system_clock::time_point moment) {
    if (moment == std::chrono::system_clock::time_point{}) return {};

    const std::chrono::zoned_time local{std::chrono::current_zone(),
                                        std::chrono::floor<std::chrono::seconds>(moment)};

    return std::format(L"{:%d.%m.%Y %H:%M}", local);
}

}  // namespace

MessageScreen::MessageScreen() {
    messages_ = StackPanel{Margin{32, 8, 32, 32}};

    title_ = TextBlock{
        column = 1,
        fontSize = 20,
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
                toolTip = L"Вернуться к темам",
                content = FontIcon{glyph = kBack, fontSize = 14},
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
    const double indent = kStep * std::min(depth, kMaxSteps);

    auto head = Grid{
        columnDefinitions = L"auto,*,auto",
        columnSpacing = 8,

        TextBlock{
            column = 0,
            std::wstring(message.info.author.displayName),
            fontSize = 13,
            FontWeight{600},
            foreground = brushes.accentTextFillColorPrimary,
        },
        TextBlock{
            column = 1,
            std::wstring(message.info.subject),
            fontSize = 12,
            foreground = brushes.textFillColorTertiary,
            textTrimming.characterEllipsis,
            vAlign.center,
        },
        TextBlock{
            column = 2,
            dateText(message.info.createdOn),
            fontSize = 12,
            foreground = brushes.textFillColorTertiary,
            vAlign.center,
        },
    };

    // Вот оно, ради чего всё: разметка автора, разобранная нашим
    // wxl::RsdnBlock. Тело сервер не трогал -- в нём и «VD>» цитаты, и
    // «:shuffle:», и код в теге языка.
    auto body = RsdnBlock{
        isTextSelectionEnabled = true,
        Margin{0, 8, 0, 0},
    };

    if (!baseDirectory_.empty()) body.baseDirectory(baseDirectory_);

    body.rsdn(message.body);

    // Карточка библиотечная: у сообщения ровно тот же вид, что у карточки
    // WinUI, и своего здесь только отступ по глубине ответа.
    return Built<Card>{
        hAlign.stretch,
        Margin{indent, 4, 0, 4},
        Padding{14, 10},
        StackPanel{head, body},
    };
}

}  // namespace besedka::app
