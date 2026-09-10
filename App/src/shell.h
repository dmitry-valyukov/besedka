#pragma once
// Каркас окна: верхняя панель с «назад» и «вперёд», вкладки внизу, полоса
// состояния, а между ними -- середина: заставка или страницы.
//
// Порт `ui/MainWindow.kt` из jana вместе с тремя её частями: `MainTopAppBar`,
// `MainBottomBar` и `StatusBar`, -- с одним отступлением. У jana панели
// уходят, стоит открыть форум: у тех экранов свой заголовок со стрелкой
// назад. У нас навигация принадлежит каркасу -- «назад» и «вперёд» стоят на
// верхней панели, как у браузера, -- поэтому панели видны всегда, кроме
// заставки, а у экранов своих стрелок нет.
//
// Каркас ничего не решает: что стоит в середине, куда ведут кнопки, какую
// вкладку подсветить -- ему говорят (см. Navigator). Он рисует и сообщает о
// нажатиях.
//
// Часть кнопок пока ни к чему не ведёт, и это нормальное состояние порта:
// каркас ставится целиком, наполнение приходит по одному. Что за какой
// кнопкой должно появиться, написано у места.

#include <functional>
#include <string>
#include <string_view>

#include "pch.h"

import besedka.app;

namespace besedka::app {

/// Что известно про сервер. Порт `ui/components/ServerStatus.kt`: до первого
/// ответа неизвестно, дальше -- ответил или не ответил.
enum class ServerStatus { unknown, online, offline };

class Shell {
public:
    Shell();

    /// Корень, который отдаётся окну как содержимое. Окно получает его один
    /// раз: дальше меняется не содержимое окна, а середина каркаса.
    const wxl::UIElement& root() const { return root_.value(); }

    /// Заставка: одна на всё окно и без панелей. Жать «обновить» и
    /// переключать вкладки, пока не прочитан первый ответ, нечего; полоса
    /// состояния пересказывала бы своими словами то, что уже написано на
    /// карточке, и отрезала бы у картинки полосу снизу.
    void showSplash(const wxl::UIElement& splash);

    /// Страницы -- с панелями вокруг.
    void showPages(const wxl::UIElement& pages);

    void setCanGoBack(bool can);
    void setCanGoForward(bool can);

    /// Подсветить вкладку текущей страницы. `onTab` при этом не зовётся:
    /// это ответ на переход, а не его причина.
    void selectTab(Tab tab);

    /// Кружок у названия: отвечает сервер или нет.
    void setServerStatus(ServerStatus status);

    /// Идёт запрос -- вместо кнопки обновления колечко, как в jana.
    void setBusy(bool busy);

    /// Полоса внизу окна. У jana туда попадает адрес ссылки под указателем;
    /// у нас -- где мы и что стоит сказать, не заводя окна с сообщением.
    void setStatusText(std::wstring_view said);

    std::function<void()> onBack;
    std::function<void()> onForward;

    /// Ctrl с плюсом, минусом и нулём -- как в браузере.
    std::function<void()> onZoomIn;
    std::function<void()> onZoomOut;
    std::function<void()> onZoomReset;

    std::function<void()> onRefresh;
    std::function<void(Tab)> onTab;
    std::function<void()> onLogin;
    std::function<void()> onAbout;

private:
    /// Что стоит в середине. Сравнивать обёртки не с чем -- равенства у них
    /// нет, -- поэтому середина помнится родом, а не элементом.
    enum class Middle { none, splash, pages };

    void setMiddle(Middle what, const wxl::UIElement& element);

    Middle middle_ = Middle::none;

    /// Вкладку переставляем сами -- и её событие о выборе не должно
    /// превращаться в переход.
    bool selectingTab_ = false;

    wxl::Nullable<wxl::Grid> root_ = nullptr;
    wxl::Nullable<wxl::Border> topBar_ = nullptr;
    wxl::Nullable<wxl::Border> tabsBar_ = nullptr;
    wxl::Nullable<wxl::Border> statusBar_ = nullptr;
    wxl::Nullable<wxl::SelectorBar> tabs_ = nullptr;
    wxl::Nullable<wxl::TextBlock> status_ = nullptr;
    wxl::Nullable<wxl::Border> dot_ = nullptr;
    wxl::Nullable<wxl::Button> back_ = nullptr;
    wxl::Nullable<wxl::Button> forward_ = nullptr;
    wxl::Nullable<wxl::Button> refresh_ = nullptr;
    wxl::Nullable<wxl::ProgressRing> ring_ = nullptr;
    wxl::Nullable<wxl::Button> theme_ = nullptr;
};

}  // namespace besedka::app
