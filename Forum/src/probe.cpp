// Проба дороги к api.rsdn.org: запрос списка форумов и вывод его на экран.
//
// Проверяется здесь не список, а три вещи разом. Что HTTPS достаётся даром:
// шифрует Schannel внутри Windows.Web.Http, своей библиотеки TLS не нужно.
// Что чтение открыто без входа. И что сборка Беседки правильно связана с wxl:
// текст с сервера доходит до экрана через wxl::text, а не через свою
// перекодировку.
//
// Разбор JSON здесь готовый, из Windows.Data.Json, и это временно: он кладёт
// COM-объект на каждый узел, а страницы сообщений читаются постоянно. Чем его
// заменить — записано в docs/decisions.md.
//
// Запуск: build\x64\Forum\Debug\forum-probe.exe

#include <windows.h>

#include <cstdio>
#include <string>
#include <string_view>

#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Web.Http.h>

import wxl.text;

namespace {

// Текст с сервера — чужой, и непарный суррогат в нём не запрещён ничем, кроме
// вежливости. repaired() ставит U+FFFD вместо испорченного и отдаёт то, что
// печатать уже можно.
std::string utf8(std::wstring_view text) {
    return std::string(wxl::text::repaired(text).to_utf8().chars());
}

// Колонка шириной в символах, а не в байтах: printf считает байты, и от
// кириллицы, у которой их по два на букву, таблица разъезжается. Считает
// символы wxl::text — своего счёта кодовых точек в проекте нет и не будет.
std::string padded(std::wstring_view text, std::size_t width) {
    std::string bytes = std::string(wxl::text::repaired(text).to_utf8().chars());
    const std::optional<wxl::text::u8_view> checked =
        wxl::text::checked(std::string_view(bytes));
    const std::size_t points = checked ? wxl::text::count_code_points(*checked) : bytes.size();
    if (points < width) bytes.append(width - points, ' ');
    return bytes;
}

std::wstring_view value_or_empty(const winrt::Windows::Data::Json::JsonObject& object,
                                 const winrt::hstring& name) {
    if (!object.HasKey(name)) return {};
    const winrt::Windows::Data::Json::IJsonValue value = object.GetNamedValue(name);
    if (value.ValueType() != winrt::Windows::Data::Json::JsonValueType::String) return {};
    return value.GetString();
}

}  // namespace

int main() {
    SetConsoleOutputCP(CP_UTF8);
    winrt::init_apartment();

    using namespace winrt::Windows::Data::Json;
    using namespace winrt::Windows::Foundation;
    using namespace winrt::Windows::Web::Http;

    try {
        HttpClient client;
        const winrt::hstring body =
            client.GetStringAsync(Uri(L"https://api.rsdn.org/forums")).get();

        const JsonArray forums = JsonArray::Parse(body);
        std::printf("Форумов: %u\n\n", forums.Size());

        for (const IJsonValue& item : forums) {
            const JsonObject forum = item.GetObject();
            const double id = forum.GetNamedNumber(L"id", 0);
            const std::wstring_view code = value_or_empty(forum, L"code");
            const std::wstring_view name = value_or_empty(forum, L"name");

            std::wstring_view group;
            if (forum.HasKey(L"forumGroup") &&
                forum.GetNamedValue(L"forumGroup").ValueType() == JsonValueType::Object) {
                group = value_or_empty(forum.GetNamedObject(L"forumGroup"), L"name");
            }

            std::printf("%5d  %s %s %s\n", static_cast<int>(id), padded(code, 18).c_str(),
                        padded(name, 46).c_str(), utf8(group).c_str());
        }
    } catch (const winrt::hresult_error& error) {
        std::printf("Не вышло: %s (0x%08X)\n", utf8(error.message()).c_str(),
                    static_cast<unsigned>(error.code()));
        return 1;
    }

    return 0;
}
