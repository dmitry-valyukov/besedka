// Куда в Беседке можно прийти: витрина форумов, темы форума, сообщения темы
// и две вкладки, за которыми пока ничего нет. Маршрут -- значение: его кладут
// в историю, по нему узнают экран, вкладку и подпись, его сравнивают с тем,
// что уже показано.
//
// Форум и тема едут в маршруте целиком, а не номером: заголовок экрана нужен
// раньше, чем ответит сервер, а хранилища, из которого его можно было бы
// взять по номеру, пока нет. Появится хранилище -- маршрут ужмётся до номера,
// и история станет годна для записи на диск.

export module besedka.app:route;

import std;
import besedka.forum;

export namespace besedka::app {

/// Вкладки нижней панели, в порядке jana.
enum class Tab { forums, watched, outbox };

/// Экраны -- по одному на род маршрута, и каждый в одном экземпляре.
///
/// Отсюда обещание, на котором стоит двухстраничный показ: две соседние
/// записи истории всегда разного рода. Обе видны разом, а один элемент не
/// может стоять в дереве дважды. Обещание держат переходы (см. Navigator:
/// глубже витрины идут темы, глубже тем -- сообщения, а вкладки открывают
/// верхний уровень), а planPages его проверяет.
enum class Screen { forums, topics, messages, watched, outbox };

struct ForumsRoute {};
struct WatchedRoute {};
struct OutboxRoute {};

/// Темы одного форума.
struct TopicsRoute {
    forum::ForumDescription forum;
};

/// Сообщения одной темы. Форум едет вместе с темой: у темы есть только его
/// номер, а строке пути нужно имя.
struct MessagesRoute {
    forum::ForumDescription forum;
    forum::MessageInfo topic;
};

using Route = std::variant<ForumsRoute, WatchedRoute, OutboxRoute, TopicsRoute, MessagesRoute>;

Screen screenOf(const Route& route);

/// Вкладка, к которой относится страница: темы и сообщения -- к форумам.
Tab tabOf(const Route& route);

/// Верхний уровень вкладки: куда ведёт щелчок по ней.
Route rootOf(Tab tab);

/// Та же страница: та же витрина, тот же форум, та же тема.
bool sameRoute(const Route& left, const Route& right);

/// Чем страница названа: имя форума, тема, название вкладки. Последнее звено
/// пути.
std::wstring titleOf(const Route& route);

/// Путь до страницы, звеньями: «WinAPI», «Тема». Корень вкладки в него не
/// входит -- вкладка и так подсвечена внизу.
std::vector<std::wstring> pathOf(const Route& route);

}  // namespace besedka::app
