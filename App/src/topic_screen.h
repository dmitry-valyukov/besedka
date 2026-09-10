#pragma once
// Темы форума.
//
// Порт TopicListScreen с его TopicCard из jana: заголовок темы, под ним
// автор, число ответов и дата. Граватара автора здесь пока нет -- в jana он
// приезжает по сети и кешируется в базе, а у Беседки нет ещё ни того, ни
// другого. Стрелки «назад» у экрана нет тоже, и это уже не порт: назад и
// вперёд ведёт каркас, как у браузера.

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

    /// Чей это форум -- показывается в заголовке, пока темы ещё едут.
    void setForum(const forum::ForumDescription& forum);

    /// Экран занят этим форумом: показывает его темы или ждёт их. По этому
    /// переход «назад» и «вперёд» узнаёт, что перечитывать нечего.
    bool shows(int forumId) const noexcept { return forumId_ == forumId; }

    void show(const forum::MessagePage& page);

    /// Беда вместо тем: причина прямо в списке, чтобы не гадать над пустым
    /// экраном.
    void setError(std::wstring_view said);

    /// Страница стоит справа: выбирают в ней двойным щелчком, потому что
    /// одиночный принадлежит её содержимому. Ставит тот, кто раскладывает
    /// страницы.
    void setSecondary(bool secondary) { secondary_ = secondary; }

    /// Масштаб списка; общий для всех списков, ставит навигатор.
    void setZoom(double factor);

    std::function<void(const forum::MessageInfo&)> onOpen;

    /// Масштаб сменили щипком или Ctrl+колесом прямо здесь.
    std::function<void(double)> onZoomChanged;

private:
    bool secondary_ = false;

    std::optional<ZoomView> list_;

    wxl::Button topicRow(const forum::MessageInfo& topic,
                         std::chrono::system_clock::time_point now,
                         const std::chrono::time_zone& zone);

    /// Открыть тему по идентификатору -- общее тело обоих щелчков.
    void openById(int32_t id);

    wxl::Nullable<wxl::Grid> root_ = nullptr;
    wxl::Nullable<wxl::StackPanel> topics_ = nullptr;
    wxl::Nullable<wxl::TextBlock> title_ = nullptr;
    wxl::Nullable<wxl::TextBlock> counter_ = nullptr;

    /// Чей форум занял экран; пусто, пока ничей.
    std::optional<int> forumId_;

    forum::MessagePage shown_;
};

}  // namespace besedka::app
