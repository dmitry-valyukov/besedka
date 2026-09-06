#include "stub_screens.h"

#include <string>

namespace besedka::app {

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr wchar_t kWatched = 0xE734;  // звезда
constexpr wchar_t kOutbox = 0xE724;   // отправить

/// Одинаковая для обеих: большой значок, название и строчка о том, чего
/// ждать. Разница между ними -- только в этих трёх вещах.
UIElement emptyScreen(wchar_t code, std::wstring_view heading, std::wstring_view promise) {
    return Grid{
        StackPanel{
            hAlign.center,
            vAlign.center,
            width = 420,

            FontIcon{
                glyph = std::wstring(1, code),
                fontSize = 40,
                foreground = brushes.textFillColorDisabled,
            },
            TextBlock{
                std::wstring(heading),
                fontSize = 18,
                FontWeight{600},
                hAlign.center,
                Margin{0, 12, 0, 0},
                foreground = brushes.textFillColorSecondary,
            },
            TextBlock{
                std::wstring(promise),
                fontSize = 13,
                hAlign.center,
                Margin{0, 6, 0, 0},
                textAlignment.center,
                textWrapping.wrap,
                foreground = brushes.textFillColorTertiary,
            },
        },
    };
}

}  // namespace

UIElement watchedScreen() {
    return emptyScreen(kWatched, L"Избранное",
                       L"Здесь будут темы, за которыми вы следите. Появятся вместе с "
                       L"хранилищем: следить не за чем, пока прочитанное не запоминается.");
}

UIElement outboxScreen() {
    return emptyScreen(kOutbox, L"Исходящие",
                       L"Здесь будут ответы, написанные и ещё не ушедшие на сервер. "
                       L"Появятся вместе со входом: писать в форум можно только своим "
                       L"именем.");
}

}  // namespace besedka::app
