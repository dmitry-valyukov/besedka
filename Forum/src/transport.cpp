module;

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Web.Http.Headers.h>
#include <winrt/Windows.Web.Http.h>

module besedka.forum;

import std;
import wxl.async;
import wxl.core;

namespace besedka::forum {
namespace {

using namespace winrt::Windows::Web::Http;

using winrt::Windows::Foundation::Uri;

/// Кто мы. Сервер этого не требует, но разговаривать безымянным невежливо,
/// а в чужих журналах имя однажды пригодится.
constexpr std::wstring_view userAgent = L"Besedka";
constexpr std::wstring_view userAgentVersion = L"0.1";

/// Запрос целиком: ушёл, дождался ответа, прочитал тело, положил в
/// обещание. Корутина, а не ожидание на потоке, -- поток всё это время
/// свободен.
///
/// Клиент и запрос приняты по значению: у корутины параметры копируются в
/// её кадр, и потому ни `this`, ни чей-то ещё век ей уже не важен -- а
/// оба эти типа суть указатели с подсчётом ссылок, так что копия ничего не
/// стоит.
winrt::fire_and_forget run(HttpClient client, HttpRequestMessage request,
                           wxl::async::promise<Response> answer) {
    try {
        const HttpResponseMessage response = co_await client.SendRequestAsync(request);

        // Буфером, а не строкой: ReadAsStringAsync перевёл бы UTF-8 в
        // UTF-16, а читателю JSON нужен ровно UTF-8 -- перевод туда и
        // обратно был бы двумя проходами по сотням килобайт ради ничего.
        const winrt::Windows::Storage::Streams::IBuffer body =
            co_await response.Content().ReadAsBufferAsync();

        Response result;

        result.status = static_cast<int>(response.StatusCode());
        result.body.assign(reinterpret_cast<const char*>(body.data()), body.Length());

        answer.set_value(result);
    } catch (const winrt::hresult_error& broken) {
        // Разговор не состоялся вовсе: кода состояния тут нет и быть не
        // может, поэтому ноль.
        //
        // repaired -- дверь для чужого текста, и стоит она здесь, на входе:
        // сообщение составила Windows, а она не обещает, что её UTF-16
        // правильный. Ниже по программе гарантия едет уже в типе, и никто её
        // не проверяет заново.
        answer.set_exception(std::make_exception_ptr(
            HttpError(0, wxl::core::unicode::repaired(broken.message()))));
    } catch (...) {
        answer.set_exception(std::current_exception());
    }
}

}  // namespace

struct Http::Impl {
    HttpClient client;
    std::wstring token;
};

Http::Http() : impl_(std::make_unique<Impl>()) {
    impl_->client.DefaultRequestHeaders().UserAgent().Append(
        Headers::HttpProductInfoHeaderValue(winrt::hstring(userAgent),
                                            winrt::hstring(userAgentVersion)));
}

Http::~Http() = default;

void Http::setToken(const std::wstring_view token) { impl_->token = token; }

bool Http::signedIn() const noexcept { return !impl_->token.empty(); }

wxl::async::future<Response> Http::get(const std::wstring_view url) const {
    wxl::async::promise<Response> answer;

    HttpRequestMessage request(HttpMethod::Get(), Uri(winrt::hstring(url)));

    if (!impl_->token.empty())
        request.Headers().Authorization(
            Headers::HttpCredentialsHeaderValue(L"Bearer", winrt::hstring(impl_->token)));

    run(impl_->client, request, answer);

    return answer.get_future();
}

wxl::async::future<Response> Http::postForm(
    const std::wstring_view url,
    const std::initializer_list<std::pair<std::wstring_view, std::wstring_view>> fields) const {
    wxl::async::promise<Response> answer;

    // Карта, а не словарь наш: HttpFormUrlEncodedContent берёт именно
    // IIterable<IKeyValuePair<hstring, hstring>>, и single_threaded_map --
    // самый короткий способ ею стать.
    auto form = winrt::single_threaded_map<winrt::hstring, winrt::hstring>();

    for (const auto& [name, value] : fields)
        form.Insert(winrt::hstring(name), winrt::hstring(value));

    HttpRequestMessage request(HttpMethod::Post(), Uri(winrt::hstring(url)));

    request.Content(HttpFormUrlEncodedContent(form.GetView()));

    if (!impl_->token.empty())
        request.Headers().Authorization(
            Headers::HttpCredentialsHeaderValue(L"Bearer", winrt::hstring(impl_->token)));

    run(impl_->client, request, answer);

    return answer.get_future();
}

}  // namespace besedka::forum
