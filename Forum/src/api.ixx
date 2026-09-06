// Сервер RSDN: какие вопросы Беседка ему задаёт и чем он на них отвечает.
// Слой над :transport (дорога) и :parse (перевод ответа в модель) -- здесь
// только адреса и склейка.
//
// Устройство взято у jana (`M:\source\jana`, api/RsdnApi.kt): те же шесть
// запросов, те же параметры. Разошлись в одном, и это записано в
// docs/decisions.md: тело сообщения запрашивается сырым (formatBody=false),
// потому что разбирает его wxl::rsdn, а не сервер.
//
// **Откуда звать.** С потока, которому принадлежит пул STA, -- то есть с
// потока интерфейса. Пул запрещает выделять и освобождать память на чужом
// потоке (списки свободных блоков у него без блокировок), а разбор ответа
// как раз выделяет. C++/WinRT возвращает корутину туда, откуда её начали:
// значит, запрос, начатый на потоке интерфейса, там же и разбирается.
// Начатый с чужого потока разберётся на чужом -- и это ошибка, которую
// отладочная сборка пула ловит проверкой.
//
// Кому нужен ответ на другом потоке -- тому :transport и :parse отдельно.
// Байты не принадлежат никакому потоку, да и готовое дерево читать можно
// откуда угодно; нельзя только строить и разрушать его не там.

export module besedka.forum:api;

import :model;
import :transport;
import std;
import wxl.async;

export namespace besedka::forum {

/// Адрес сервера по умолчанию. REST, а не старый SOAP: почему -- в
/// docs/decisions.md.
inline constexpr std::wstring_view rsdnServer = L"https://api.rsdn.org";

class Api {
public:
    explicit Api(std::wstring_view server = rsdnServer);

    /// Токен доступа. Пустой -- выход; читать форумы можно и без него.
    void setToken(std::wstring_view token);

    bool signedIn() const noexcept;

    /// Что сервер рассказывает о себе. Самый дешёвый способ проверить, что
    /// дорога есть: ни входа, ни данных не требует.
    wxl::async::future<ServiceInfo> serviceInfo() const;

    /// Витрина форумов целиком. Сервер отдаёт её одним куском -- их
    /// восемь десятков, и страниц тут нет.
    wxl::async::future<std::vector<ForumDescription>> forums() const;

    /// Темы форума, новыми вперёд.
    wxl::async::future<MessagePage> topics(int forumId, int limit = 50, int offset = 0) const;

    /// Сообщения темы, включая корневое, в порядке появления.
    wxl::async::future<MessagePage> answers(int topicId, int limit = 100, int offset = 0) const;

    /// Сообщение вместе с телом -- разметкой автора, а не серверным HTML.
    wxl::async::future<Message> message(int id) const;

    /// Профиль того, под кем вошли. Без токена сервер отвечает отказом, и
    /// это тот самый отказ, по которому видно, что токен протух.
    wxl::async::future<Account> me() const;

    /// Адрес запроса. Открыт, потому что проверяется тестом: порядок
    /// параметров и разделители -- ровно то, на чём ошибаются молча.
    ///
    /// Значения не кодируются: в этих запросах они -- числа и `true` с
    /// `false`. Первое же значение, которое придётся кодировать, поедет
    /// формой, а форму кодирует Windows (см. Http::postForm).
    std::wstring url(std::wstring_view path,
                     std::initializer_list<std::pair<std::wstring_view, std::wstring>> query) const;

private:
    Http http_;
    std::wstring server_;
};

}  // namespace besedka::forum
