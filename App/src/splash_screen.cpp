#include "splash_screen.h"

namespace besedka::app {

using namespace wxl;
using namespace wxl::dsl;

namespace {

// Чернила на заставке -- всегда чернила тёмной темы, какая бы тема ни была у
// окна. Экран карточки (wxl::OverlayCard) тёмный в любой теме: он для того и
// сделан, чтобы картинка была видна сквозь него. Обычная краска текста в
// светлой теме почти чёрная, и на этом экране надпись пропадала совсем --
// ровно так, как это выглядело у первой Беседки.
//
// Форма вызова кисти с темой -- то, ради чего она есть: кисть, присвоенная
// свойству, это значение, а не ссылка, так что тему для неё выбирают в
// момент присвоения. Так же поступает и образец: у jana текст заставки
// белый, Color.White, без оглядки на тему.
Brush const& ink() { return brushes.textFillColorPrimary(ElementTheme::Dark); }
Brush const& dimInk() { return brushes.textFillColorSecondary(ElementTheme::Dark); }

}  // namespace

SplashScreen::SplashScreen() {
    status_ = TextBlock{
        L"Здравствуйте!",
        fontSize = 15,
        foreground = dimInk(),
        textWrapping.wrap,
        Margin{0, 10, 0, 0},
    };

    // Колечко, а не полоса: сколько осталось, никто не знает -- сервер
    // отвечает целиком и сразу. Красится оно тоже светлым: своей краской
    // ProgressRing берёт цвет подсветки системы, а он на тёмном экране
    // карточки читается ничем не лучше тёмного текста.
    ring_ = ProgressRing{
        isActive = true,
        width = 28,
        height = 28,
        hAlign.left,
        Margin{0, 16, 0, 0},
        foreground = ink(),
    };

    retry_ = Button{
        L"Ещё раз",
        hAlign.left,
        Margin{0, 16, 0, 0},
        visibility = Visibility::Collapsed,
        onClick = [this](Object const&, RoutedEventArgs&) { if (onRetry) onRetry(); },
    };

    auto card = Built<OverlayCard>{
        hAlign.right,
        vAlign.bottom,
        Margin{0, 0, 24, 24},
        width = 380,
        StackPanel{
            TextBlock{
                L"Беседка RSDN",
                fontSize = 34,
                FontWeight{700},
                foreground = ink(),
            },
            status_.value(),
            ring_.value(),
            retry_.value(),
        },
    };

    // Картинка и карточка -- дети одной ячейки: порядок объявления и есть
    // порядок по глубине, так что карточка ложится поверх заставки.
    //
    // UniformToFill, а не Fill: пропорции заставки сохраняются, лишнее
    // срезается краями окна. Растянутая по обеим осям картинка выдаёт себя
    // сразу, какой бы формы ни было окно.
    root_ = Grid{
        Image{
            source = L"Assets/splash-screen.png",
            stretch = Stretch::UniformToFill,
            hAlign.center,
            vAlign.center,
        },
        card,
    };
}

void SplashScreen::setStatus(const std::wstring_view said) {
    status_.value().text(said);

    ring_.value().visibility(Visibility::Visible);
    ring_.value().isActive(true);
    retry_.value().visibility(Visibility::Collapsed);
}

void SplashScreen::setError(const std::wstring_view said) {
    status_.value().text(said);

    // Колечко убирается совсем, а не останавливается: крутящееся и
    // остановленное колечко отличаются только тем, чего на снимке не видно.
    ring_.value().isActive(false);
    ring_.value().visibility(Visibility::Collapsed);
    retry_.value().visibility(Visibility::Visible);
}

}  // namespace besedka::app
