// Дорога до сервера: запрос ушёл, ответ пришёл, и больше ничего. Ни JSON,
// ни модели, ни знания о том, что за адреса эти -- всё это выше, в :api.
//
// Ответ отдаётся будущим (wxl::async::future), а не ожиданием на месте.
// Windows.Web.Http асинхронна до самого низа, и занимать поток ожиданием
// ответа значило бы держать его спящим ровно там, где спать не за чем, --
// см. правило Беседки и Буквицы: поток будят события, а не опрос.
//
// Тело ответа -- std::string, то есть байты как пришли. Перекодировки в
// UTF-16 и обратно тут нет нарочно: ответ сервера -- UTF-8, читатель JSON
// ест UTF-8, и лишний перевод туда-сюда стоил бы двух проходов по сотням
// килобайт ради ничего.
//
// **Где завершается будущее.** Там, куда C++/WinRT возвращает корутину:
// запрос, начатый в потоке STA (то есть в потоке интерфейса), туда же и
// возвращается, а начатый в MTA завершается на потоке пула. Это не
// мелочь, а несущая часть устройства: дерево JSON живёт в пуле STA, и
// потому разбор ответа обязан идти на потоке пула -- см. :api и
// docs/decisions.md.

export module besedka.forum:transport;

import std;
import wxl.async;
import wxl.unicode;

export namespace besedka::forum {

/// Ответ сервера: код состояния и тело, как оно пришло.
struct Response {
    int status = 0;

    /// UTF-8, ни во что не перекодированный. Проверка на правильность --
    /// дело читателя JSON: он проверяет документ целиком до разбора.
    std::string body;

    /// Сервер ответил согласием. Всё остальное -- отказ, и что он значит,
    /// решает тот, кто запрашивал: 404 на сообщение -- это «нет такого», а
    /// 401 на своём профиле -- это «токен протух».
    bool ok() const noexcept { return status >= 200 && status < 300; }
};

/// Разговор не состоялся: сети нет, имя не разрешилось, сертификат не
/// принят, сервер отказал. Отказ с кодом несёт код; сорванный разговор --
/// ноль, потому что кода в нём и не было.
class HttpError : public std::runtime_error {
public:
    HttpError(int status, wxl::unicode::u16_text said)
        : std::runtime_error(status == 0 ? "network failure"
                                         : std::format("http status {}", status)),
          status_(status), said_(std::move(said)) {}

    int status() const noexcept { return status_; }

    /// Причина словами -- та, что показывают человеку.
    ///
    /// Проверенный текст, а не голая строка: он пришёл снаружи -- от Windows
    /// или от сервера, -- и то, что его починили на входе, должно быть видно
    /// в типе, а не держаться на памяти читающего.
    ///
    /// `what()` при этом нарочно английский и без текста снаружи («http status
    /// 500», «network failure»): у него тогда вовсе не возникает вопроса о
    /// кодировке, а общий обработчик и отладчик всё равно что-то видят.
    const wxl::unicode::u16_text& said() const noexcept { return said_; }

private:
    int status_;
    wxl::unicode::u16_text said_;
};

/// Один разговорчик с сервером, живущий столько же, сколько приложение:
/// внутри HttpClient, а с ним соединения, которые переиспользуются от
/// запроса к запросу. Заводить его на каждый запрос значило бы каждый раз
/// заново договариваться о TLS.
class Http {
public:
    Http();
    ~Http();

    Http(const Http&) = delete;
    Http& operator=(const Http&) = delete;

    /// Токен доступа, которым подписываются запросы (`Authorization:
    /// Bearer …`). Пустой снимает подпись -- так выглядит выход.
    void setToken(std::wstring_view token);

    bool signedIn() const noexcept;

    /// GET по готовому адресу.
    wxl::async::future<Response> get(std::wstring_view url) const;

    /// POST формой `application/x-www-form-urlencoded` -- то, чем говорят с
    /// `/connect/token`.
    ///
    /// Кодирует поля сама Windows (HttpFormUrlEncodedContent), и это не
    /// лень: процентное кодирование -- часть формата, и своё, написанное
    /// рядом, было бы вторым мнением о том, о чём двух мнений быть не
    /// должно.
    wxl::async::future<Response> postForm(
        std::wstring_view url,
        std::initializer_list<std::pair<std::wstring_view, std::wstring_view>> fields) const;

private:
    struct Impl;

    std::unique_ptr<Impl> impl_;
};

}  // namespace besedka::forum
