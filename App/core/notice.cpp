module besedka.app;

import std;
import wxl.async;
import wxl.core;

namespace besedka::app {

using wxl::async::last_error;

std::wstring reasonOf(const last_error& failure) {
    std::wstring words = failure.explanation();
    if (!words.empty()) return words;

    // Системе нечего сказать о коде: показывается диагностика -- что делали, код и
    // «unknown error»; текст наш и ASCII, но проверяется, как любой чужой.
    return reasonOf(static_cast<const std::exception&>(failure));
}

std::wstring reasonOf(const std::exception& failure) {
    const std::optional<wxl::core::u8_view> said = wxl::core::unicode::checked(std::string_view(failure.what()));
    return said ? std::wstring(said->to_utf16().wchars()) : std::wstring(L"(сообщение не в UTF-8)");
}

Notice noticeOf(std::exception_ptr error) {
    try {
        std::rethrow_exception(std::move(error));
    } catch (const last_error& failure) {
        return {L"Не удалось выполнить операцию с файлом", reasonOf(failure)};
    } catch (const std::exception& failure) {
        return {L"Ошибка", reasonOf(failure)};
    } catch (...) {
        return {L"Неизвестная ошибка", L"Сценарий прерван."};
    }
}

Notice noticeOf(std::wstring headline, std::wstring details, const last_error& failure) {
    if (!details.empty()) details += L"\n\n";
    details += reasonOf(failure);
    return {std::move(headline), std::move(details)};
}

}  // namespace besedka::app
