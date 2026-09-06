#pragma once
// Сообщения темы -- то, ради чего Беседка и заводится.
//
// Порт TopicMessagesScreen из jana по составу и расходится с ней по
// существу. Там тело приходит с сервера готовым HTML и переводится в
// элементы Compose конвертером (`ui/utils/HtmlToComposeConverter.kt`); здесь
// приходит разметка автора, и разбирает её wxl::RsdnBlock -- цитаты по
// префиксам, смайлы картинками, код с подсветкой. См. docs/decisions.md.
//
// Дерево ответов показано отступом, а не узлами с раскрытием: у сообщения
// есть parentID, и глубина -- это длина цепочки родителей внутри страницы.
// Отступ читается сразу и не требует ни одного щелчка.

#include <functional>
#include <string>

#include "pch.h"

import besedka.forum;

namespace besedka::app {

class MessageScreen {
public:
    MessageScreen();

    const wxl::UIElement& root() const { return root_.value(); }

    /// Какая тема открыта -- показывается, пока сообщения ещё едут.
    void setTopic(const forum::MessageInfo& topic);

    void show(const forum::MessagePage& page);

    void setError(std::wstring_view said);

    /// Куда картинки разметки смотрят за смайлами: каталог, в котором лежит
    /// `smiles/`.
    void setBaseDirectory(std::wstring_view directory);

    std::function<void()> onBack;

private:
    wxl::UIElement messageCard(const forum::Message& message, int depth);

    wxl::Nullable<wxl::Grid> root_ = nullptr;
    wxl::Nullable<wxl::StackPanel> messages_ = nullptr;
    wxl::Nullable<wxl::TextBlock> title_ = nullptr;
    wxl::Nullable<wxl::TextBlock> counter_ = nullptr;

    std::wstring baseDirectory_;
};

}  // namespace besedka::app
