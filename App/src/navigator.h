#pragma once
// Переходы: что происходит, когда читатель выбирает форум или тему, жмёт
// вкладку, «назад», «вперёд» или «обновить». Раньше всё это лежало
// лямбдами в main.cpp; здесь оно названо и собрано в одном месте.
//
// Решений сам не принимает: куда ведёт «назад», что видно на экране и что
// для этого сделать с деревом, считают History и planPages из besedka.app,
// и это проверено тестами. Навигатор применяет посчитанное -- к экранам, к
// каркасу и к серверу -- и потому сам тестом не покрыт: его работа в том,
// чтобы дёргать XAML и сеть, а не в том, чтобы думать.
//
// Экраны -- по одному на род маршрута -- живут здесь же, столько же, сколько
// приложение. Второго экземпляра ни у одного нет: экран, ушедший с экрана,
// остаётся в памяти со своим содержимым, и «вперёд» к нему ничего не стоит.
//
// Обещание, которое держат переходы (см. Screen в besedka.app): две
// соседние записи истории -- разного рода. Глубже витрины идут темы,
// глубже тем -- сообщения, вкладка открывает верхний уровень, а выбор на
// левой странице -- это шаг назад и новый шаг, а не шаг из правой.

#include <filesystem>
#include <functional>

#include "pch.h"

#include "about_dialog.h"
#include "forum_screen.h"
#include "message_screen.h"
#include "page_host.h"
#include "shell.h"
#include "splash_screen.h"
#include "topic_screen.h"

import besedka.app;
import besedka.forum;

namespace besedka::app {

class Navigator {
public:
    /// Каркас и сервер -- чужие и живут дольше; всё, что на панелях каркаса
    /// ведёт к переходу, навигатор подключает к себе сам.
    Navigator(forum::Api& api, Shell& shell, const std::filesystem::path& assets);

    Navigator(const Navigator&) = delete;
    Navigator& operator=(const Navigator&) = delete;

    /// Первый показ: заставка, пока читается витрина.
    void start();

    /// Новый переход: маршрут ложится в историю и показывается.
    void navigate(Route route);

    void back();
    void forward();

    /// «Обновить»: перечитать то, что на экране.
    void reload();

    /// Ширина, под которую раскладывать страницы, в логических пикселях.
    /// Приходит от окна из WM_SIZE -- то есть до того, как XAML возьмётся за
    /// вёрстку. У элемента её не спрашиваем: SizeChanged приходит уже изнутри
    /// прохода вёрстки, а перекладывать дерево оттуда нельзя.
    void setWidth(double logical);

    /// Граница между страницами: ставит запуск из настроек, двигает читатель.
    void splitFraction(double value);
    std::function<void(double)> onSplitChanged;

    /// Масштаб списков и сообщений, один на все экраны: ступенями с клавиш
    /// (см. zoomedIn в besedka.app) или любым числом от щипка.
    void zoomIn();
    void zoomOut();
    void zoomReset();
    void setZoom(double factor);
    std::function<void(double)> onZoomChanged;

    /// «О программе» над содержимым окна. Версию сервера спрашивает при
    /// каждом показе: диалог открывают редко, а запрос этот дешевле любого
    /// другого.
    void showAbout(const wxl::UIElement& host);

    /// На экране заставка -- окну пора её задник.
    std::function<void()> onSplash;

    /// Витрина открылась впервые -- окну пора рабочий вид и задник чтения.
    std::function<void()> onShowcase;

private:
    /// Выбор на странице `from`: если она стоит слева, то это шаг назад и
    /// новый шаг -- левая страница есть предыдущая запись истории, и то, что
    /// открыли из неё, идёт вслед за ней, а не за правой.
    void open(Screen from, Route to);

    /// История сменилась: показать и обновить панели.
    void moved();

    /// Та ли это страница, на которой стоим. Ответы сервера приходят и для
    /// левой страницы, а полоса состояния -- про текущую.
    bool isCurrent(const Route& route) const;

    /// Показать хвост истории: одна страница или две, по ширине.
    void show();

    /// Содержимое экрана под маршрут. `again` -- перечитать, даже если экран
    /// уже показывает именно это.
    void load(const Route& route, bool again);

    /// Витрина с сервера. Пока истории нет, идёт через заставку.
    void loadShowcase();

    forum::Api& api_;
    Shell& shell_;

    History history_;
    double width_ = 0;
    double zoom_ = kZoomDefault;

    PageHost host_;

    SplashScreen splash_;
    ForumScreen forums_;
    TopicScreen topics_;
    MessageScreen messages_;
    wxl::UIElement watched_;
    wxl::UIElement outbox_;
    AboutDialog about_;
};

}  // namespace besedka::app
