#pragma once
// Сообщения темы -- то, ради чего Беседка и заводится.
//
// Порт TopicMessagesScreen из jana по составу и расходится с ней по
// существу. Там тело приходит с сервера готовым HTML и переводится в
// элементы Compose конвертером (`ui/utils/HtmlToComposeConverter.kt`); здесь
// приходит разметка автора, и разбирает её wxl::RsdnBlock -- цитаты по
// префиксам, смайлы картинками, код с подсветкой.
//
// Дерево ответов показано отступом, а не узлами с раскрытием: у сообщения
// есть parentID, и глубина -- это длина цепочки родителей внутри страницы
// (replyDepths из besedka.app). Отступ читается сразу и не требует ни одного
// щелчка.
//
// Своего заголовка и стрелки «назад» у экрана нет: тему называет строка пути
// каркаса, а назад и вперёд ведёт он же, как у браузера.

#include <functional>
#include <optional>
#include <string>

#include "pch.h"
#include "zoom_view.h"

import besedka.forum;

namespace besedka::app {

class MessageScreen {
public:
    MessageScreen();

    const wxl::UIElement& root() const { return root_.value(); }

    /// Какая тема открыта. Список при этом очищается: сообщения ещё едут.
    void setTopic(const forum::MessageInfo& topic);

    /// Экран занят этой темой: показывает её сообщения или ждёт их. По этому
    /// переход «назад» и «вперёд» узнаёт, что перечитывать нечего.
    bool shows(int topicId) const noexcept { return topicId_ == topicId; }

    void show(const forum::MessagePage& page);

    void setError(std::wstring_view said);

    /// Куда картинки разметки смотрят за смайлами: каталог, в котором лежит
    /// `smiles/`.
    void setBaseDirectory(std::wstring_view directory);

    /// Масштаб сообщений; общий для всех списков, ставит навигатор.
    void setZoom(double factor);

    /// Масштаб сменили щипком или Ctrl+колесом прямо здесь.
    std::function<void(double)> onZoomChanged;

private:
    std::optional<ZoomView> list_;

    wxl::UIElement messageCard(const forum::Message& message, int depth,
                               const std::chrono::time_zone& zone);

    wxl::core::nullable<wxl::Grid> root_ = nullptr;
    wxl::core::nullable<wxl::StackPanel> messages_ = nullptr;

    /// Чья тема заняла экран; пусто, пока ничья.
    std::optional<int> topicId_;

    std::wstring baseDirectory_;
};

}  // namespace besedka::app
