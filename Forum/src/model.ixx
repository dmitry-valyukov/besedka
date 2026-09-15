// Предметная модель форума: то, чем сервер отвечает, переведённое на язык
// программы. Ни строчки о сети и ни строчки об интерфейсе -- эти структуры
// одинаково годятся и разбору ответа, и списку на экране, и хранилищу.
//
// Имена полей -- те, что у сервера (`forumID`, `isTopic`, `answersCount`),
// потому что сверять модель с ответом придётся ещё не раз, а перевод имён
// сделал бы каждую такую сверку загадкой. Исключение одно: `parentID` и
// прочие `ID` пишутся как `parentId` -- в наших правилах это camelCase, и
// заглавные буквы посреди слова тут ничего не значат.
//
// Текст -- std::wstring, а не UTF-8. Всё, что с этой моделью происходит
// дальше, -- показ: список тем, дерево ответов, разметка через wxl::rsdn
// (у неё дверь для wstring_view) и подписи в WinUI, где строка и так
// UTF-16. Перевод делается один раз, при разборе ответа, а не на каждом
// кадре.
//
// Время -- system_clock::time_point, а не строка, какой оно приходит.
// Сообщения сортируются, группируются по дням и показываются в местном
// часовом поясе; строке ISO 8601 для всего этого пришлось бы разбираться
// заново в каждом месте.

export module besedka.forum:model;

import std;

export namespace besedka::forum {

/// Группа, в которой форум стоит на витрине: «Сайт», «Программирование ::
/// Microsoft Windows». Порядок групп задаёт сервер полем sortOrder.
struct ForumGroup {
    int id = 0;
    std::wstring name;
    int sortOrder = 0;
};

/// Форум целиком, как его описывает `/forums`.
struct ForumDescription {
    int id = 0;

    /// Короткое имя, оно же кусок адреса на сайте: `rsdn`, `cpp`, `winapi`.
    std::wstring code;

    std::wstring name;
    std::wstring description;

    ForumGroup group;

    /// Из тех, что показываются наверху витрины.
    bool isInTop = false;

    /// Форум о самом сайте, а не о предмете.
    bool isSiteSubject = false;

    /// Служебный: не для разговоров.
    bool isService = false;

    /// Сообщения в нём оценивают.
    bool isRated = false;

    /// Писать сюда можно. Форумы, закрытые на запись, остаются читаемыми.
    bool isWriteAllowed = false;

    /// Сколько оценок в сутки положено; 0, когда ограничения нет.
    int rateLimit = 0;
};

/// Автор сообщения -- ровно столько, сколько показывает список.
struct Author {
    int id = 0;
    std::wstring displayName;

    /// Хеш для gravatar.com: по нему берётся картинка автора.
    std::wstring gravatarHash;

    /// `User`, `Admin`, `Moderator`.
    std::wstring role;
};

/// Сообщение без тела -- то, из чего собирается и список тем, и дерево
/// ответов.
struct MessageInfo {
    int id = 0;
    int forumId = 0;

    /// Корневое сообщение темы; у самой темы совпадает с id.
    int topicId = 0;

    /// Сообщение, на которое отвечали; 0 у корня темы.
    int parentId = 0;

    Author author;
    std::wstring subject;

    std::chrono::system_clock::time_point createdOn;

    /// Когда правили; равно createdOn, если не правили ни разу.
    std::chrono::system_clock::time_point updatedOn;

    bool isTopic = false;

    /// Сколько ответов в теме. Считает сервер, и только у корня темы.
    int answersCount = 0;
};

/// Сообщение вместе с телом.
struct Message {
    MessageInfo info;

    /// Тело: разметка автора при `formatBody=false`, серверный HTML при
    /// `formatBody=true`. Беседка просит первое -- см. .claude/decisions.md.
    std::wstring body;

    /// Тело собрано сервером в HTML. Ложь -- значит, это разметка автора, и
    /// разбирать её нам, через wxl::rsdn.
    bool isFormatted = false;
};

/// Страница сообщений: сервер отдаёт их порциями и говорит, сколько всего.
///
/// Сообщения здесь с телами, а не заголовки: `/messages` умеет отдать тела
/// прямо в списке (`withBodies=true`), и дерево темы поэтому читается одним
/// запросом вместо сотни. Там, где тела не просили — список тем, — они
/// просто пусты.
struct MessagePage {
    std::vector<Message> items;

    /// Сколько сообщений отвечает запросу целиком, а не сколько в items.
    int total = 0;

    /// С какого по счёту начинается эта страница.
    int offset = 0;
};

/// Учётная запись -- та, под которой вошли.
struct Account {
    int id = 0;
    std::wstring login;
    std::wstring email;
    std::wstring displayName;
    std::wstring gravatarHash;
    std::wstring role;
};

/// Что сервер рассказывает о себе. Первый же запрос Беседки: он проверяет
/// дорогу, не спрашивая ни входа, ни данных.
struct ServiceInfo {
    std::wstring name;
    std::wstring serverVersion;
    std::chrono::system_clock::time_point serverTime;
    std::chrono::system_clock::time_point serverBuildDate;
};

}  // namespace besedka::forum
