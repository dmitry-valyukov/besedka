module besedka.forum;

import std;
import wxl.json;
import wxl.core;

namespace besedka::forum {
namespace {

using wxl::json::value;

/// Текст модели -- UTF-16: дальше он идёт на экран, а там строка и так
/// UTF-16. Перевод делается здесь, один раз на поле.
std::wstring wide(const wxl::core::u8_view utf8) {
    std::wstring out;

    wxl::core::unicode::append_utf16(out, utf8);

    return out;
}

std::wstring wide(const value& from) { return wide(from.as_string()); }

int number(const value& from) { return static_cast<int>(from.as_int()); }

// Поле даты целиком, или отказ: try_parse не трогает `out`, пока не разберёт
// весь кусок, так что половины числа тут не бывает.
bool digits(const std::string_view text, int& out) { return wxl::core::try_parse(text, out); }

}  // namespace

// Разбирается по местам, а не по разделителям: в ISO 8601 у каждого поля
// своя длина, и это единственный формат времени, который приходит с
// сервера. std::chrono::from_stream отвергнут -- он тянет за собой поток и
// локаль ради того же самого.
std::chrono::system_clock::time_point readTimestamp(const wxl::core::u8_view stamp) {
    using namespace std::chrono;

    const std::string_view text = stamp.chars();

    if (text.size() < 19 || text[4] != '-' || text[7] != '-' || text[13] != ':' || text[16] != ':')
        return {};

    if (text[10] != 'T' && text[10] != 't' && text[10] != ' ') return {};

    int years = 0;
    int months = 0;
    int days = 0;
    int hh = 0;
    int mm = 0;
    int ss = 0;

    if (!digits(text.substr(0, 4), years) || !digits(text.substr(5, 2), months) ||
        !digits(text.substr(8, 2), days) || !digits(text.substr(11, 2), hh) ||
        !digits(text.substr(14, 2), mm) || !digits(text.substr(17, 2), ss))
        return {};

    const year_month_day date{year{years}, month{static_cast<unsigned>(months)},
                              day{static_cast<unsigned>(days)}};

    if (!date.ok()) return {};

    std::size_t at = 19;

    // Доли секунды: сервер шлёт то три знака, то семь. Дальше миллисекунд
    // нам ничего не нужно -- сообщения ими не различаются, -- а лишние
    // знаки просто дочитываются и выбрасываются.
    milliseconds fraction{0};

    if (at < text.size() && text[at] == '.') {
        ++at;

        int value = 0;
        int taken = 0;

        while (at < text.size() && text[at] >= '0' && text[at] <= '9') {
            if (taken < 3) {
                value = value * 10 + (text[at] - '0');
                ++taken;
            }
            ++at;
        }

        while (taken != 0 && taken < 3) {
            value *= 10;
            ++taken;
        }

        fraction = milliseconds{value};
    }

    // Смещение вычитается, а не прибавляется: 16:11 при +03:00 -- это 13:11
    // по Гринвичу, и модель держит время в UTC, чтобы сравнение двух
    // сообщений не зависело от того, где сидел их автор.
    minutes offset{0};

    if (at < text.size() && (text[at] == '+' || text[at] == '-') && text.size() - at >= 6 &&
        text[at + 3] == ':') {
        int oh = 0;
        int om = 0;

        if (digits(text.substr(at + 1, 2), oh) && digits(text.substr(at + 4, 2), om)) {
            offset = hours{oh} + minutes{om};

            if (text[at] == '-') offset = -offset;
        }
    }

    const sys_seconds moment = sys_days{date} + hours{hh} + minutes{mm} + seconds{ss};

    return time_point_cast<system_clock::duration>(moment + fraction - offset);
}

ForumGroup readForumGroup(const value& from) {
    return ForumGroup{
        .id = number(from["id"]),
        .name = wide(from["name"]),
        .sortOrder = number(from["sortOrder"]),
    };
}

ForumDescription readForum(const value& from) {
    return ForumDescription{
        .id = number(from["id"]),
        .code = wide(from["code"]),
        .name = wide(from["name"]),
        .description = wide(from["description"]),
        .group = readForumGroup(from["forumGroup"]),
        .isInTop = from["isInTop"].as_bool(),
        .isSiteSubject = from["isSiteSubject"].as_bool(),
        .isService = from["isService"].as_bool(),
        .isRated = from["isRated"].as_bool(),
        .isWriteAllowed = from["isWriteAllowed"].as_bool(),
        .rateLimit = number(from["rateLimit"]),
    };
}

std::vector<ForumDescription> readForums(const value& from) {
    std::vector<ForumDescription> forums;

    forums.reserve(from.elements().size());

    for (const value& forum : from.elements()) forums.push_back(readForum(forum));

    return forums;
}

Author readAuthor(const value& from) {
    return Author{
        .id = number(from["id"]),
        .displayName = wide(from["displayName"]),
        .gravatarHash = wide(from["gravatarHash"]),
        .role = wide(from["role"]),
    };
}

MessageInfo readMessageInfo(const value& from) {
    return MessageInfo{
        .id = number(from["id"]),
        .forumId = number(from["forumID"]),
        .topicId = number(from["topicID"]),
        .parentId = number(from["parentID"]),
        .author = readAuthor(from["author"]),
        .subject = wide(from["subject"]),
        .createdOn = readTimestamp(from["createdOn"].as_string()),
        .updatedOn = readTimestamp(from["updatedOn"].as_string()),
        .isTopic = from["isTopic"].as_bool(),
        .answersCount = number(from["answersCount"]),
    };
}

MessagePage readMessagePage(const value& from) {
    MessagePage page;

    const std::span<const value> items = from["items"].elements();

    page.items.reserve(items.size());

    for (const value& item : items) page.items.push_back(readMessage(item));

    page.total = number(from["total"]);
    page.offset = number(from["offset"]);

    return page;
}

Message readMessage(const value& from) {
    const value& body = from["body"];

    return Message{
        .info = readMessageInfo(from),
        .body = wide(body["text"]),
        .isFormatted = body["isFormatted"].as_bool(),
    };
}

Account readAccount(const value& from) {
    return Account{
        .id = number(from["id"]),
        .login = wide(from["login"]),
        .email = wide(from["email"]),
        .displayName = wide(from["displayName"]),
        .gravatarHash = wide(from["gravatarHash"]),
        .role = wide(from["role"]),
    };
}

ServiceInfo readServiceInfo(const value& from) {
    return ServiceInfo{
        .name = wide(from["name"]),
        .serverVersion = wide(from["serverVersion"]),
        .serverTime = readTimestamp(from["serverTime"].as_string()),
        .serverBuildDate = readTimestamp(from["serverBuildDate"].as_string()),
    };
}

}  // namespace besedka::forum
