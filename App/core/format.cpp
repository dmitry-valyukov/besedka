module besedka.app;

import std;
import besedka.forum;
import wxl.core;

namespace besedka::app {

using namespace std::chrono;

std::wstring relativeDate(const system_clock::time_point moment,
                          const system_clock::time_point now, const time_zone& zone) {
    if (moment == system_clock::time_point{}) return {};

    const zoned_time local{&zone, floor<seconds>(moment)};
    const zoned_time today{&zone, floor<seconds>(now)};

    const year_month_day then{floor<days>(local.get_local_time())};
    const year_month_day day{floor<days>(today.get_local_time())};

    if (then == day) return std::format(L"{:%H:%M}", local);

    if (then.year() == day.year()) return std::format(L"{:%d.%m %H:%M}", local);

    return std::format(L"{:%d.%m.%y}", local);
}

std::wstring fullDate(const system_clock::time_point moment, const time_zone& zone) {
    if (moment == system_clock::time_point{}) return {};

    const zoned_time local{&zone, floor<seconds>(moment)};

    return std::format(L"{:%d.%m.%Y %H:%M}", local);
}

std::wstring reasonOf(const std::exception_ptr& why) {
    try {
        std::rethrow_exception(why);
    } catch (const forum::HttpError& refused) {
        // Ни проверки, ни перекодировки: текст починен там, где вошёл в
        // программу, и досюда доехал проверенным типом. Здесь он всего лишь
        // выходит наружу -- в обычную широкую строку, какую ждёт XAML.
        const std::wstring_view said = refused.said().wchars();

        return refused.status() == 0 ? std::format(L"Сервер недоступен: {}", said)
                                     : std::format(L"Сервер отказал: {}", said);
    } catch (const std::exception&) {
        return L"Не вышло поговорить с сервером.";
    }
}

}  // namespace besedka::app
