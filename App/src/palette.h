#pragma once
// Палитра Беседки: все краски приложения по ролям, в одном месте.
//
// Экран не называет кисть WinUI по имени ресурса -- он называет роль:
// `palette.text`, `palette.card`, `palette.divider`. Что за ролью стоит,
// решается здесь и только здесь, поэтому подобрать прозрачность карточек над
// картинкой-задником, заменить кисть на акриловую или перекрасить цитаты --
// это одна строка ниже, а не поиск по десяти файлам.
//
// Роль -- тот же объект, что и путь `brushes.…`: превращается в кисть там,
// где её присваивают, и умеет форму с темой (`palette.text(ElementTheme::Dark)`).
// Роль, которой нужна не та тема, что у окна, несёт тему в себе (`Themed`).
//
// Порядок ролей -- от текста к поверхностям и к цветам, которые не кисти.

#include "pch.h"

namespace besedka::app {

/// Кисть ресурса в названной теме, а не в теме окна. Для того, что должно
/// выглядеть одинаково при любой теме, -- чернил на заставке над тёмной
/// картинкой.
template <class Key>
struct Themed {
    Key key;
    wxl::ElementTheme theme;

    operator wxl::Brush const&() const { return key(theme); }
};

struct Palette {
    // ---- текст ----
    static constexpr auto text = wxl::dsl::brushes.Text.FillColor.Primary;
    static constexpr auto textSecondary = wxl::dsl::brushes.Text.FillColor.Secondary;
    static constexpr auto textTertiary = wxl::dsl::brushes.Text.FillColor.Tertiary;
    static constexpr auto textDisabled = wxl::dsl::brushes.Text.FillColor.Disabled;

    /// Акцент: форум о сайте, автор сообщения.
    static constexpr auto accent = wxl::dsl::brushes.Accent.TextFillColor.Primary;
    static constexpr auto accentSecondary = wxl::dsl::brushes.Accent.TextFillColor.Secondary;

    // ---- состояния ----
    static constexpr auto success = wxl::dsl::brushes.SystemFillColor.Success;
    static constexpr auto critical = wxl::dsl::brushes.SystemFillColor.Critical;

    // ---- поверхности ----
    //
    // Все они лежат над картинкой-задником, и сколько её сквозь них видно,
    // решается тут. Ресурсы WinUI считают, что под ними ровный фон, отсюда
    // их прозрачность: карточка тёмной темы -- пять процентов белого, и
    // текст на ней читался бы прямо с фотографии.

    /// Панели каркаса: верхняя и вкладки.
    static constexpr auto bar = wxl::dsl::brushes.Layer.FillColorDefault;
    static constexpr auto statusBar = wxl::dsl::brushes.SolidBackgroundFillColor.Secondary;

    /// Карточка-строка списка и панель пользователя.
    static constexpr auto card = wxl::dsl::brushes.Card.BackgroundFillColor.Default;

    /// Карточка сообщения: непрозрачная -- читают её, а не картинку под ней, --
    /// и чисто-белая в светлой теме (#FFFFFF; в тёмной #2C2C2C). Tertiary
    /// рядом -- #F9F9F9, и на белой странице это заметный серый.
    static constexpr auto messageCard = wxl::dsl::brushes.SolidBackgroundFillColor.Quarternary;

    static constexpr auto cardStroke = wxl::dsl::brushes.Card.StrokeColorDefault;

    /// Бейдж с кодом форума.
    static constexpr auto badge = wxl::dsl::brushes.Layer.FillColorDefault;

    /// Границы панелей и граница между страницами.
    static constexpr auto divider = wxl::dsl::brushes.DividerStrokeColorDefault;

    /// Кнопка-значок и строка витрины: своего фона нет, только отклик.
    static constexpr auto transparent = wxl::dsl::brushes.SubtleFillColor.Transparent;

    // ---- заставка ----
    //
    // Чернила на заставке -- всегда чернила тёмной темы, какая бы тема ни
    // была у окна: карточка заставки тёмная в любой теме, а обычная краска
    // текста светлой темы на ней пропадала бы совсем. Так же у jana: текст
    // заставки белый, без оглядки на тему.
    static constexpr Themed splashInk{wxl::dsl::brushes.Text.FillColor.Primary,
                                      wxl::ElementTheme::Dark};
    static constexpr Themed splashInkDim{wxl::dsl::brushes.Text.FillColor.Secondary,
                                         wxl::ElementTheme::Dark};

    // ---- цвета, а не кисти ----

    /// Чем окно закрашено, пока картинка задника не доехала: тёмная земля
    /// обеих картинок, так что подмена не мигает.
    static constexpr wxl::ARGB backdropTone{0xFF17120Eu};

    /// Цитаты по уровням -- те самые три зелёных, что у RSDN в CSS и у jana в
    /// MessageStyle: от тёмного к светлому. По ним в переписке видно не только
    /// что это цитата, но и чья.
    static constexpr wxl::ARGB quote[3] = {wxl::ARGB{0xFF137900}, wxl::ARGB{0xFF74B967},
                                           wxl::ARGB{0xFF9FD095}};
};

inline constexpr Palette palette{};

}  // namespace besedka::app
