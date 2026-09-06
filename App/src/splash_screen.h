#pragma once
// Заставка: картинка на весь экран и карточка поверх неё.
//
// Порт SplashWindow из jana (`ui/SplashWindow.kt`), с одной разницей: там
// это отдельное окно, которое живёт, пока поднимается база, а здесь --
// содержимое того же окна. Второе окно завело бы вторую кнопку на панели
// задач и мигание при смене, а показать надо ровно то же самое: кто мы и
// что сейчас происходит.
//
// Карточка библиотечная, wxl::OverlayCard: она для страницы, под которой
// картинка, и своего у экрана здесь только место.

// Свои заголовки со стандартными внутри -- до pch.h: он ведёт к import
// wxl.core, а стандартный заголовок после импорта MSVC не принимает.
#include <functional>
#include <string>

#include "pch.h"

namespace besedka::app {

class SplashScreen {
public:
    SplashScreen();

    /// Корень, который отдаётся окну как содержимое.
    const wxl::UIElement& root() const { return root_.value(); }

    /// Что происходит прямо сейчас: «Читаю витрину форумов…».
    void setStatus(std::wstring_view said);

    /// Не вышло. Вместо колечка -- причина и кнопка «Ещё раз».
    void setError(std::wstring_view said);

    std::function<void()> onRetry;

private:
    wxl::Nullable<wxl::Grid> root_ = nullptr;
    wxl::Nullable<wxl::TextBlock> status_ = nullptr;
    wxl::Nullable<wxl::ProgressRing> ring_ = nullptr;
    wxl::Nullable<wxl::Button> retry_ = nullptr;
};

}  // namespace besedka::app
