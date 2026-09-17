#pragma once
// Темы форума.
//
// Порт TopicListScreen с его TopicCard из jana: заголовок темы, под ним
// автор, число ответов и дата. Граватара автора здесь пока нет -- в jana он
// приезжает по сети и кешируется в базе, а у Беседки нет ещё ни того, ни
// другого. Своего заголовка и стрелки «назад» у экрана нет тоже, и это уже
// не порт: имя форума пишет строка пути каркаса, а назад и вперёд ведёт он
// же, как у браузера.

#include <functional>
#include <optional>
#include <string>

#include "pch.h"
#include "zoom_view.h"

import besedka.forum;

namespace besedka::app {

class TopicScreen {
public:
    TopicScreen();

    const wxl::UIElement& root() const { return root_.value(); }

    /// Чей это форум. Список при этом очищается: темы ещё едут.
    void setForum(const forum::ForumDescription& forum);

    /// Экран занят этим форумом: показывает его темы или ждёт их. По этому
    /// переход «назад» и «вперёд» узнаёт, что перечитывать нечего.
    bool shows(int forumId) const noexcept { return forum_ && forum_->id == forumId; }

    void show(const forum::MessagePage& page);

    /// Беда вместо тем: причина прямо в списке, чтобы не гадать над пустым
    /// экраном.
    void setError(std::wstring_view said);

    /// Страница стоит справа: выбирают в ней двойным щелчком, потому что
    /// одиночный принадлежит её содержимому. Ставит тот, кто раскладывает
    /// страницы.
    void setSecondary(bool secondary) { secondary_ = secondary; }

    /// Выбрали тему; форум -- тот, чьи темы показаны.
    std::function<void(const forum::ForumDescription&, const forum::MessageInfo&)> onOpen;

    /// Масштаб окна сменили щипком или Ctrl+колесом прямо здесь -- во столько
    /// раз.
    std::function<void(double)> onZoomChanged;

private:
    bool secondary_ = false;

    std::optional<ZoomView> list_;

    wxl::Button topicRow(const forum::MessageInfo& topic,
                         std::chrono::system_clock::time_point now,
                         const std::chrono::time_zone& zone);

    /// Открыть тему по идентификатору -- общее тело обоих щелчков.
    void openById(int32_t id);

    wxl::core::nullable<wxl::Grid> root_ = nullptr;
    wxl::core::nullable<wxl::StackPanel> topics_ = nullptr;

    /// Чей форум занял экран; пусто, пока ничей.
    std::optional<forum::ForumDescription> forum_;

    forum::MessagePage shown_;
};

}  // namespace besedka::app
