#pragma once
// Темы форума.
//
// Порт TopicListScreen с его TopicCard из jana: заголовок темы, под ним
// автор, число ответов и дата. Граватара автора здесь пока нет -- в jana он
// приезжает по сети и кешируется в базе, а у Беседки нет ещё ни того, ни
// другого.

#include <functional>
#include <string>

#include "pch.h"

import besedka.forum;

namespace besedka::app {

class TopicScreen {
public:
    TopicScreen();

    const wxl::UIElement& root() const { return root_.value(); }

    /// Чей это форум -- показывается в заголовке, пока темы ещё едут.
    void setForum(const forum::ForumDescription& forum);

    void show(const forum::MessagePage& page);

    /// Беда вместо тем: причина прямо в списке, чтобы не гадать над пустым
    /// экраном.
    void setError(std::wstring_view said);

    std::function<void(const forum::MessageInfo&)> onOpen;
    std::function<void()> onBack;

private:
    wxl::Button topicRow(const forum::MessageInfo& topic);

    wxl::Nullable<wxl::Grid> root_ = nullptr;
    wxl::Nullable<wxl::StackPanel> topics_ = nullptr;
    wxl::Nullable<wxl::TextBlock> title_ = nullptr;
    wxl::Nullable<wxl::TextBlock> counter_ = nullptr;

    forum::MessagePage shown_;
};

}  // namespace besedka::app
