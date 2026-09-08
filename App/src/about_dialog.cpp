#include "about_dialog.h"

namespace besedka::app {

using namespace wxl;
using namespace wxl::dsl;

namespace {

/// Версия Беседки. Одно число в одном месте: пока сборка не проставляет его
/// сама, врать о нём двумя разными способами не хочется.
constexpr wchar_t kVersion[] = L"0.1";

constexpr double kCardWidth = 380;    // AboutDialog: width(420) минус поля
constexpr double kImageHeight = 200;  // заставка сверху, а не во весь фон

}  // namespace

AboutDialog::AboutDialog() {
    server_ = TextBlock{
        fontSize = 10,
        hAlign.center,
        Margin{0, 6, 0, 0},
        visibility = Visibility::Collapsed,
        foreground = brushes.Text.FillColor.Tertiary,
    };

    dialog_ = ContentDialog{
        closeButtonText = L"Закрыть",
        content = StackPanel{
            width = kCardWidth,

            // Заставка сверху, а не фоном во всю карточку: у jana поверх неё
            // лежит градиент, чтобы читались белые буквы, а здесь текст стоит
            // на своей подложке и градиент не нужен. Скруглённая рамка -- она
            // же и обрезает картинку, вылезающую при UniformToFill.
            Border{
                height = kImageHeight,
                CornerRadius{8},
                Image{
                    source = L"Assets/splash-screen.png",
                    stretch = Stretch::UniformToFill,
                    hAlign.center,
                    vAlign.center,
                },
            },

            TextBlock{
                L"Беседка",
                fontSize = 24,
                FontWeight{700},
                hAlign.center,
                Margin{0, 16, 0, 0},
                foreground = brushes.Text.FillColor.Primary,
            },
            TextBlock{
                L"Разговоры RSDN",
                fontSize = 13,
                hAlign.center,
                foreground = brushes.Text.FillColor.Tertiary,
            },

            HyperlinkButton{
                L"rsdn.org",
                navigateUri = L"https://rsdn.org",
                hAlign.center,
                Margin{0, 8, 0, 0},
                fontSize = 13,
            },

            TextBlock{
                std::wstring(L"v") + kVersion + L"  ·  © 2026",
                fontSize = 11,
                hAlign.center,
                foreground = brushes.Text.FillColor.Tertiary,
            },
            server_.value(),
        },
    };
}

void AboutDialog::show(const Window& host) { showDialog(dialog_.value(), host); }

void AboutDialog::setServerLine(const std::wstring_view said) {
    server_.value().text(said);
    server_.value().visibility(said.empty() ? Visibility::Collapsed : Visibility::Visible);
}

}  // namespace besedka::app
