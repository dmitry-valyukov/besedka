#pragma once
// Витрина форумов: группы, а в них форумы.
//
// Порт ForumListScreen + ForumGroupHeader + ForumCard из jana
// (`ui/screens/ForumListScreen.kt`, `ui/components/`). Там группа -- липкий
// заголовок со стрелочкой, которая поворачивается; здесь Expander, у
// которого всё это своё и уже нарисовано. Порядок групп задаёт сервер полем
// sortOrder -- для того оно и есть.
//
// Список строится обычным циклом, а не повторителем с шаблоном: форумов
// столько, сколько прислал сервер, и это число известно только во время
// работы. Восемь десятков строк -- не тот счёт, ради которого заводят
// виртуализацию.

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "pch.h"
#include "zoom_view.h"

import besedka.forum;

namespace besedka::app {

class ForumScreen {
public:
    ForumScreen();

    const wxl::UIElement& root() const { return root_.value(); }

    /// Показывает витрину заново. Пересобирается целиком: сравнивать её со
    /// списком и править разницу было бы дороже во всех смыслах.
    void show(const std::vector<forum::ForumDescription>& forums);

    /// Страница стоит справа: выбирают в ней двойным щелчком, потому что
    /// одиночный принадлежит её содержимому. Ставит каркас, раскладывая стопку.
    void setSecondary(bool secondary) { secondary_ = secondary; }

    /// Масштаб списка; общий для всех списков, ставит навигатор.
    void setZoom(double factor);

    std::function<void(const forum::ForumDescription&)> onOpen;

    /// Масштаб сменили щипком или Ctrl+колесом прямо здесь.
    std::function<void(double)> onZoomChanged;

private:
    bool secondary_ = false;

    /// Прокрутка с масштабом вокруг списка групп. Появляется в конструкторе,
    /// когда есть что прокручивать.
    std::optional<ZoomView> list_;

    /// Одна строка витрины. Кнопка, потому что по форуму щёлкают, а всё,
    /// что кнопка умеет сама -- наведение, нажатие, фокус, клавиатура, --
    /// достаётся даром.
    wxl::Button forumRow(const forum::ForumDescription& forum);

    /// Открыть форум по идентификатору -- общее тело обоих щелчков.
    void openById(int32_t id);

    wxl::core::nullable<wxl::Grid> root_ = nullptr;
    wxl::core::nullable<wxl::StackPanel> groups_ = nullptr;

    /// Форумы, по которым построена нынешняя витрина: строка кнопки держит
    /// не сам форум, а его место здесь.
    std::vector<forum::ForumDescription> shown_;
};

}  // namespace besedka::app
