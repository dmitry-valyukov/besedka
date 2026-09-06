#pragma once
// Каркас окна: верхняя панель, вкладки внизу, полоса состояния, а между
// ними -- экран.
//
// Порт `ui/MainWindow.kt` вместе с тремя её частями: `MainTopAppBar`,
// `MainBottomBar` и `StatusBar`. Устройство то же самое, вплоть до правила
// показа: панели видны только на верхнем уровне, а стоит открыть форум или
// тему -- остаются экран и полоса состояния, потому что у тех экранов свой
// заголовок со стрелкой назад.
//
// Часть кнопок пока ни к чему не ведёт, и это нормальное состояние порта:
// каркас ставится целиком, наполнение приходит по одному. Что за какой
// кнопкой должно появиться, написано у места.

#include <functional>
#include <string>
#include <string_view>

#include "pch.h"

namespace besedka::app {

/// Что известно про сервер. Порт `ui/components/ServerStatus.kt`: до первого
/// ответа неизвестно, дальше -- ответил или не ответил.
enum class ServerStatus { unknown, online, offline };

/// Вкладки нижней панели, в порядке jana.
enum class Tab { forums, watched, outbox };

class Shell {
public:
    Shell();

    /// Корень, который отдаётся окну как содержимое. Окно получает его один
    /// раз: дальше меняется не содержимое окна, а середина каркаса.
    const wxl::UIElement& root() const { return root_.value(); }

    /// Экран в середине.
    void setContent(const wxl::UIElement& screen);

    /// Показывать ли панели. Ложь -- когда открыт форум или тема: у тех
    /// экранов свой заголовок, и две полосы подряд читались бы как одна
    /// сломанная.
    void setChromeVisible(bool visible);

    /// Кружок у названия: отвечает сервер или нет.
    void setServerStatus(ServerStatus status);

    /// Идёт запрос -- вместо кнопки обновления колечко, как в jana.
    void setBusy(bool busy);

    /// Полоса внизу окна. У jana туда попадает адрес ссылки под указателем;
    /// у нас пока -- то, что стоит сказать, не заводя окна с сообщением.
    void setStatusText(std::wstring_view said);

    std::function<void()> onRefresh;
    std::function<void(Tab)> onTab;
    std::function<void()> onLogin;
    std::function<void()> onAbout;

private:
    wxl::Nullable<wxl::Grid> root_ = nullptr;
    wxl::Nullable<wxl::Border> topBar_ = nullptr;
    wxl::Nullable<wxl::Grid> host_ = nullptr;
    wxl::Nullable<wxl::Border> tabsBar_ = nullptr;
    wxl::Nullable<wxl::SelectorBar> tabs_ = nullptr;
    wxl::Nullable<wxl::TextBlock> status_ = nullptr;
    wxl::Nullable<wxl::Border> dot_ = nullptr;
    wxl::Nullable<wxl::Button> refresh_ = nullptr;
    wxl::Nullable<wxl::ProgressRing> ring_ = nullptr;
    wxl::Nullable<wxl::Button> theme_ = nullptr;
};

}  // namespace besedka::app
