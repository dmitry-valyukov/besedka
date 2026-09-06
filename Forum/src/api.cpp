module besedka.forum;

import std;
import wxl.async;
import wxl.json;
import wxl.text;

namespace besedka::forum {
namespace {

/// Число как кусок адреса. Через wxl::text -- то есть без локали: с
/// настройками на русский обычная печать числа однажды поставит запятую
/// туда, где сервер ждёт цифру.
std::wstring digits(int value) { return wxl::text::to_wstring(value); }

/// Ответ -> модель. Отказ сервера превращается в исключение здесь, а не в
/// транспорте: транспорт не знает, что значит код, а тут известно, что
/// любой отказ на чтение -- это неудача запроса целиком.
///
/// Документ живёт ровно на время разбора: модель забирает из него всё, что
/// ей нужно, своими строками, и дерево после этого не нужно никому.
template <typename Read>
auto reading(Read read) {
    return [read = std::move(read)](const Response& answer) {
        if (!answer.ok())
            throw HttpError(answer.status,
                            std::format("сервер ответил {}", answer.status));

        wxl::json::document document;

        return read(document.load(std::string(answer.body)));
    };
}

}  // namespace

Api::Api(const std::wstring_view server) : server_(server) {}

void Api::setToken(const std::wstring_view token) { http_.setToken(token); }

bool Api::signedIn() const noexcept { return http_.signedIn(); }

std::wstring Api::url(
    const std::wstring_view path,
    const std::initializer_list<std::pair<std::wstring_view, std::wstring>> query) const {
    std::wstring address;

    address.reserve(server_.size() + path.size() + 64);
    address += server_;
    address += L'/';
    address += path;

    bool first = true;

    for (const auto& [name, value] : query) {
        address += first ? L'?' : L'&';
        first = false;

        address += name;
        address += L'=';
        address += value;
    }

    return address;
}

wxl::async::future<ServiceInfo> Api::serviceInfo() const {
    return http_.get(url(L"service/info", {})).next(reading(&readServiceInfo));
}

wxl::async::future<std::vector<ForumDescription>> Api::forums() const {
    return http_.get(url(L"forums", {})).next(reading(&readForums));
}

wxl::async::future<MessagePage> Api::topics(const int forumId, const int limit,
                                            const int offset) const {
    // order=1 -- новыми вперёд, как показывает список тем сам форум.
    return http_
        .get(url(L"messages", {{L"forumID", digits(forumId)},
                               {L"onlyTopics", L"true"},
                               {L"limit", digits(limit)},
                               {L"offset", digits(offset)},
                               {L"order", L"1"}}))
        .next(reading(&readMessagePage));
}

wxl::async::future<MessagePage> Api::answers(const int topicId, const int limit,
                                             const int offset) const {
    // order=0 -- в порядке появления: дерево ответов собирается по
    // parentID, и родитель обязан приехать раньше ребёнка.
    //
    // withBodies=true -- тела прямо в списке. Сервер это умеет, и потому
    // тема читается одним запросом, а не одним на список и сотней на
    // сообщения. formatBody=false -- разметка автора, как и везде.
    return http_
        .get(url(L"messages", {{L"topicID", digits(topicId)},
                               {L"onlyTopics", L"false"},
                               {L"limit", digits(limit)},
                               {L"offset", digits(offset)},
                               {L"order", L"0"},
                               {L"withBodies", L"true"},
                               {L"formatBody", L"false"}}))
        .next(reading(&readMessagePage));
}

wxl::async::future<Message> Api::message(const int id) const {
    // formatBody=false -- разметка автора. Ради этого Беседка и заводится:
    // цитаты, смайлы и подсветка наши, а не серверные.
    return http_
        .get(url(L"messages/" + digits(id), {{L"withBodies", L"true"}, {L"formatBody", L"false"}}))
        .next(reading(&readMessage));
}

wxl::async::future<Account> Api::me() const {
    return http_.get(url(L"accounts/me", {})).next(reading(&readAccount));
}

}  // namespace besedka::forum
