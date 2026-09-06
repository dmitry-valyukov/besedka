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
#include <string>
#include <vector>

#include "pch.h"

import besedka.forum;

namespace besedka::app {

class ForumScreen {
public:
    ForumScreen();

    const wxl::UIElement& root() const { return root_.value(); }

    /// Показывает витрину заново. Пересобирается целиком: сравнивать её со
    /// списком и править разницу было бы дороже во всех смыслах.
    void show(const std::vector<forum::ForumDescription>& forums);

    std::function<void(const forum::ForumDescription&)> onOpen;
    std::function<void()> onRefresh;

private:
    /// Одна строка витрины. Кнопка, потому что по форуму щёлкают, а всё,
    /// что кнопка умеет сама -- наведение, нажатие, фокус, клавиатура, --
    /// достаётся даром.
    wxl::Button forumRow(const forum::ForumDescription& forum);

    wxl::Nullable<wxl::Grid> root_ = nullptr;
    wxl::Nullable<wxl::StackPanel> groups_ = nullptr;
    wxl::Nullable<wxl::TextBlock> counter_ = nullptr;

    /// Форумы, по которым построена нынешняя витрина: строка кнопки держит
    /// не сам форум, а его место здесь.
    std::vector<forum::ForumDescription> shown_;
};

}  // namespace besedka::app
